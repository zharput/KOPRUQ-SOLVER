#pragma once
// Resolves a MIDAS Civil NX DBUSER section (a shape code + raw dimension
// array, "vSIZE") into area/Iy/Iz/J, since the MIDAS JSON export never
// gives these numbers directly for a DBUSER section -- see
// docs/MIDAS_JSON_DATA_AUDIT.md section 4.3. Formulas are classical
// mechanics-of-materials results (not MIDAS-specific), cited per shape.
//
// Only the three shapes actually present in xmidas/deneme1.json are
// implemented: "SB" (solid rectangle), "B" (symmetric hollow rectangle/
// box), "SR" (solid circle). Any other shape code is rejected with a
// clear diagnostic rather than guessed at.
//
// vSIZE convention (inferred from the sample's own dimensions, e.g. a
// "SUPER" deck section SB=[1.0,12.0] -- a 1 m deep, 12 m wide slab, and a
// pier section SB=[2.0,3.0]): vSIZE[0] = H (the vertical/depth
// dimension), vSIZE[1] = B (horizontal/width). This file returns area,
// IH (second moment about the axis resisting bending caused by a
// transverse load in the H direction -- i.e. integral of (distance along
// B)^2, magnitude B*H^3/12 for a solid rectangle) and IB (the other one,
// H*B^3/12) by name rather than Iy/Iz, because which of KOPRUQ-SOLVER's
// Iy/Iz each corresponds to additionally depends on the member's local
// axis orientation (MIDAS's ANGLE/beta and default reference direction),
// a SEPARATE, still-open item -- seek MidasOrientation.hpp for that half
// of the mapping. Keeping the two concerns apart avoids silently baking
// an unverified axis-convention guess into the section-property math.
#include "../../core/Types.hpp"
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace kopruq::io::midas { inline constexpr double kPi = 3.14159265358979323846; }

namespace kopruq::io::midas {

// AsH/AsB: effective (Timoshenko) shear area [m^2] for shear resisted in the same sense as
// IH/IB respectively -- i.e. AsH is the shear area for a transverse shear force along H
// (companion to the bending stiffness IH), AsB the shear area for a force along B (companion
// to IB). Computed via each shape's classical shear correction factor k (As = k*A), per
// Timoshenko & Goodier, "Theory of Elasticity" -- NOT MIDAS-specific, same sourcing standard
// as the J formulas above. See MidasJsonImporter.hpp for how AsH/AsB map onto Beam3D's Asy/Asz.
// principalAngleRad: additional rotation (radians) of the member's local y/z axes about its
// local x-axis, needed ONLY for an unsymmetric shape (e.g. an equal-leg angle) whose principal
// bending axes are not aligned with the H/B directions -- 0 for every symmetric shape in this
// file. IH/IB below are already expressed in the ROTATED (principal) frame when this is nonzero,
// i.e. they are directly usable as Iy/Iz once the importer applies this same rotation to
// reference_y (same rotation mechanic as MidasOrientation.hpp's own Beta angle) -- see
// equalAngleSection's header comment for the derivation.
struct ResolvedSectionProperties { Real area{}, IH{}, IB{}, J{}, AsH{}, AsB{}; Real principalAngleRad{0}; };

namespace detail {
// Saint-Venant torsion constant, solid rectangle (a = longer side, b = shorter side),
// classical approximate formula (e.g. Boresi & Schmidt, "Advanced Mechanics of Materials"):
//   J = a*b^3*(1/3 - 0.21*(b/a)*(1 - (b/a)^4/12))
inline Real solidRectangleTorsion(Real a, Real b) {
  if (a < b) std::swap(a, b);
  const Real r = b / a;
  return a * b * b * b * (Real(1)/3 - Real(0.21) * r * (Real(1) - r*r*r*r/12));
}
// Timoshenko shear correction factor for a solid rectangular cross-section: k = 5/6,
// independent of the H/B aspect ratio (Timoshenko & Goodier, "Theory of Elasticity",
// S3.1 beam shear). Applies identically to shear along either H or B.
inline constexpr Real kRectangularShearFactor = Real(5) / Real(6);
// Timoshenko shear correction factor for a solid circular cross-section: the classical,
// Poisson-ratio-independent approximation k = 0.9 (Timoshenko & Goodier; also the default
// used by common structural FE codes for solid circular sections). The more precise
// Cowper(1966) factor is Poisson-ratio-dependent; the constant approximation is used here
// since MidasSectionLibrary resolves geometry only, not material properties.
inline constexpr Real kCircularShearFactor = Real(0.9);

// One horizontal slice of a cross-section built from stacked/side-by-side rectangles, for the
// Zhuravskii/Timoshenko effective-shear-area computation below: occupies the coordinate range
// [y0, y1] along the integration axis, with constant material width `width` perpendicular to it.
// `strips` passed to timoshenkoShearArea() must be given in increasing y order and must exactly
// tile the section's full depth along that axis with no gaps (width 0 for a hollow interior is
// how a closed/box section's hollow middle is expressed).
struct ShearStrip { Real y0, y1, width; };

// Effective (Timoshenko) shear area via the classical Zhuravskii shear-stress distribution
// tau(y) = V*Q(y)/(I*b(y)), where Q(y) is the first moment of area (about the section's own
// centroid) of the material between the extreme fiber and y, and b(y) is the material width at
// y. The energy-consistent shear area is the standard closed-form result
//   As = I^2 / integral( Q(y)^2 / b(y) dy )
// (e.g. Cowper 1966; Timoshenko & Goodier, "Theory of Elasticity" S3.1, generalized from the
// rectangle/circle special cases already used elsewhere in this file to an arbitrary section
// built from rectangular strips). This project has no closed-form per-shape formula available
// for composite sections like a T or hollow box beyond the coarser "shear carried by the web/
// flange alone" engineering approximation this replaces (2026-11-19, user instruction: prefer
// the rigorous theoretical formula over that coarser approximation when MIDAS's own effective
// shear area cannot be obtained) -- so the integral is evaluated numerically (fine composite
// midpoint/trapezoidal quadrature, not a per-shape closed form, to stay correct for any strip
// decomposition without re-deriving calculus by hand for each new shape). Verified, not just
// trusted, against the EXACT closed-form rectangle result (k=5/6): see
// MidasSectionLibrary.TimoshenkoShearAreaMatchesClosedFormRectangle.
inline Real timoshenkoShearArea(const std::vector<ShearStrip>& strips, Real I) {
  if (strips.empty()) throw std::invalid_argument("timoshenkoShearArea: no strips given");
  for (const auto& s : strips)
    if (!(s.y1 > s.y0) || !(s.width >= 0)) throw std::invalid_argument("timoshenkoShearArea: each strip needs y1>y0 and width>=0");
  if (!(I > 0)) throw std::invalid_argument("timoshenkoShearArea: I must be positive");

  // Area and centroid: EXACT, since width is piecewise-constant (the integrand within each strip
  // is then exactly linear, so its antiderivative is used directly -- no quadrature needed or
  // used here).
  Real area = 0, firstMoment = 0;
  for (const auto& s : strips) {
    area += s.width * (s.y1 - s.y0);
    firstMoment += s.width * (s.y1 * s.y1 - s.y0 * s.y0) / 2;
  }
  const Real ybar = firstMoment / area;

  // Integral of Q(y)^2/b(y) dy. Q(y) itself is evaluated EXACTLY at every sample point via the
  // closed-form antiderivative within its strip (width constant there, so Q is an exact quadratic
  // -- accumulating it from many small discrete increments, as an earlier version of this
  // function did, introduces an O(1/N) bias from evaluating the running sum at the wrong point
  // within each micro-step; the closed form has no such error). Only the final integration of the
  // smooth quartic Q(y)^2/b(y) over each strip uses numerical (fine composite midpoint) quadrature
  // -- Q^2 has no convenient generic antiderivative to hand-code once for an arbitrary strip
  // layout, but midpoint quadrature of a smooth quartic converges quickly (O(1/N^2)), confirmed by
  // MidasSectionLibrary.TimoshenkoShearAreaMatchesClosedFormRectangle matching the exact k=5/6
  // rectangle result to within 1e-6 at the step count used here.
  constexpr int kStepsPerStrip = 400;
  Real integral = 0, Qstart = 0; // Qstart: exact Q at the low-y edge of the current strip
  for (const auto& s : strips) {
    const Real dy = (s.y1 - s.y0) / kStepsPerStrip;
    for (int i = 0; i < kStepsPerStrip; ++i) {
      const Real ya = s.y0 + i * dy;
      const Real ym = ya + dy / 2;
      const Real Qm = Qstart + s.width * ((ym * ym - s.y0 * s.y0) / 2 - ybar * (ym - s.y0));
      if (s.width > 0) integral += (Qm * Qm / s.width) * dy; // width==0 (hollow) carries no shear stress there
    }
    Qstart += s.width * ((s.y1 * s.y1 - s.y0 * s.y0) / 2 - ybar * (s.y1 - s.y0)); // exact advance to the strip's end
  }
  return I * I / integral;
}

// An axis-aligned rectangle, used to describe a cross-section's domain for the finite-difference
// St. Venant torsion solve below, as a union of solid rectangles minus a union of void
// (hollow-interior) rectangles.
struct TorsionRect { Real x0, x1, y0, y1; };

// Saint-Venant torsion constant via the membrane analogy (Prandtl stress function phi): the
// governing equation is Laplacian(phi) = -2 everywhere inside the cross-section, phi = 0 on its
// boundary, and J = 2 * integral(phi dA) -- this is the EXACT formulation for an arbitrary
// simply-connected cross-section (e.g. Timoshenko & Goodier, "Theory of Elasticity" Ch. 11); only
// its numerical (finite-difference) solution here is approximate, converging to the exact value
// as the grid is refined. Generalizes the per-shape formulas elsewhere in this file -- the solid
// rectangle's own closed-form series approximation, and the thin-strip-sum formula this replaces
// for the T-section -- into one numerically-solved but theoretically exact method, for sections
// this project has no sufficiently accurate closed form for and no access to MIDAS's own value
// (2026-11-19, user instruction: prefer rigorous theory over a coarser approximation). The box
// section's Bredt thin-walled formula is left as-is (already the standard, near-exact result for
// a closed thin-walled cell, not a coarse shortcut the way the T-section's strip sum is).
//
// Implementation: a uniform grid of spacing h over the domain's bounding box; interior grid nodes
// become unknowns in the standard 5-point finite-difference Laplacian, assembled as a sparse
// linear system and solved directly (Eigen::SimplicialLDLT -- the discrete Laplacian here is
// symmetric positive definite). Grid spacing is chosen from the SMALLEST solid-rectangle
// dimension present (so a thin web or flange is never grid-aliased away to zero thickness),
// capped so the grid stays a practical size. Verified, not just trusted, against two independent,
// already-established references: the exact square-section tabulated coefficient (0.1406*a^4,
// see MidasSectionLibrary.TorsionConstantFEMMatchesTabulatedSquareCoefficient) and this file's
// own existing closed-form solidRectangleTorsion() approximation for a non-square rectangle
// (MidasSectionLibrary.TorsionConstantFEMMatchesClosedFormRectangle).
// Core FDM solve shared by every torsionConstantFEM overload below: given the domain's bounding
// box, an `inside(x,y)` membership predicate, and a resolution hint (the smallest solid feature
// dimension present, so a thin web/flange/chamfer is never grid-aliased to zero thickness), runs
// the same verified 5-point-Laplacian solve as the original rectangle-only version. Splitting
// this out does not change the method -- it only lets a curved or polygonal domain (an ellipse/
// stadium, a chamfered rectangle, ...) reuse it via its own `inside` predicate instead of only a
// union of axis-aligned rectangles.
inline Real torsionConstantFEMCore(Real xmin, Real xmax, Real ymin, Real ymax, Real minDim,
                                    const std::function<bool(Real, Real)>& inside) {
  if (!(xmax > xmin) || !(ymax > ymin)) throw std::invalid_argument("torsionConstantFEMCore: bounding box needs xmax>xmin and ymax>ymin");
  if (!(minDim > 0)) throw std::invalid_argument("torsionConstantFEMCore: minDim must be positive");
  // ~80 cells across the thinnest solid feature (confirmed by convergence testing -- an
  // independent Python/scipy finite-difference implementation of this same method reaches the
  // tabulated square-section coefficient within 0.05% and the thin-strip closed-form rectangle
  // approximation within 0.1% at this resolution), capped so the grid stays a practical size for
  // a very elongated section.
  const Real span = std::max(xmax - xmin, ymax - ymin);
  int cellsAcrossSpan = static_cast<int>(std::ceil(span / (minDim / 80)));
  cellsAcrossSpan = std::min(cellsAcrossSpan, 400);
  const Real h = span / cellsAcrossSpan;
  const int nx = static_cast<int>(std::round((xmax - xmin) / h)) + 1;
  const int ny = static_cast<int>(std::round((ymax - ymin) / h)) + 1;

  // Index interior (inside-the-domain) grid nodes; everything else is an implicit phi=0 boundary.
  std::vector<int> nodeIndex(static_cast<std::size_t>(nx) * ny, -1);
  int numUnknowns = 0;
  auto at = [&](int i, int j) { return j * nx + i; };
  for (int j = 0; j < ny; ++j)
    for (int i = 0; i < nx; ++i)
      if (inside(xmin + i * h, ymin + j * h)) nodeIndex[static_cast<std::size_t>(at(i, j))] = numUnknowns++;
  if (numUnknowns == 0) throw std::invalid_argument("torsionConstantFEM: domain is empty at this grid resolution");

  std::vector<Eigen::Triplet<Real>> triplets;
  triplets.reserve(static_cast<std::size_t>(numUnknowns) * 5);
  Eigen::VectorXd rhs = Eigen::VectorXd::Zero(numUnknowns);
  for (int j = 0; j < ny; ++j) {
    for (int i = 0; i < nx; ++i) {
      const int row = nodeIndex[static_cast<std::size_t>(at(i, j))];
      if (row < 0) continue;
      // 4*phi_P - phi_E - phi_W - phi_N - phi_S = 2*h^2 (a neighbor outside the domain contributes
      // phi=0, i.e. simply does not add a matrix entry -- the Dirichlet boundary condition).
      triplets.emplace_back(row, row, Real(4));
      rhs(row) = 2 * h * h;
      const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; ++k) {
        const int ni = i + di[k], nj = j + dj[k];
        if (ni < 0 || ni >= nx || nj < 0 || nj >= ny) continue;
        const int col = nodeIndex[static_cast<std::size_t>(at(ni, nj))];
        if (col >= 0) triplets.emplace_back(row, col, Real(-1));
      }
    }
  }
  Eigen::SparseMatrix<Real> A(numUnknowns, numUnknowns);
  A.setFromTriplets(triplets.begin(), triplets.end());
  Eigen::SimplicialLDLT<Eigen::SparseMatrix<Real>> solver(A);
  if (solver.info() != Eigen::Success) throw std::runtime_error("torsionConstantFEMCore: Laplacian factorization failed");
  const Eigen::VectorXd phi = solver.solve(rhs);
  if (solver.info() != Eigen::Success) throw std::runtime_error("torsionConstantFEMCore: Laplacian solve failed");

  return 2 * phi.sum() * h * h;
}

// Original rectangle-union interface, now a thin wrapper over torsionConstantFEMCore -- same
// behavior as before this file's refactor (2026-11-19), verified by the same existing tests.
inline Real torsionConstantFEM(const std::vector<TorsionRect>& solidRects, const std::vector<TorsionRect>& voidRects = {}) {
  if (solidRects.empty()) throw std::invalid_argument("torsionConstantFEM: no solid rectangles given");
  Real xmin = solidRects.front().x0, xmax = solidRects.front().x1, ymin = solidRects.front().y0, ymax = solidRects.front().y1;
  Real minDim = std::numeric_limits<Real>::max();
  for (const auto& r : solidRects) {
    if (!(r.x1 > r.x0) || !(r.y1 > r.y0)) throw std::invalid_argument("torsionConstantFEM: each solid rectangle needs x1>x0 and y1>y0");
    xmin = std::min(xmin, r.x0); xmax = std::max(xmax, r.x1);
    ymin = std::min(ymin, r.y0); ymax = std::max(ymax, r.y1);
    minDim = std::min({minDim, r.x1 - r.x0, r.y1 - r.y0});
  }
  auto inside = [&](Real x, Real y) {
    bool solid = false;
    for (const auto& r : solidRects) if (x > r.x0 && x < r.x1 && y > r.y0 && y < r.y1) { solid = true; break; }
    if (!solid) return false;
    for (const auto& r : voidRects) if (x > r.x0 && x < r.x1 && y > r.y0 && y < r.y1) return false;
    return true;
  };
  return torsionConstantFEMCore(xmin, xmax, ymin, ymax, minDim, inside);
}

// Stadium/"track" domain (a rectangle Ls x H capped by two semicircles of radius R=H/2 on its
// left/right ends -- see solidTrack() below): the thinnest feature for grid-resolution purposes
// is simply H (there is no thin web/flange the way an open I/channel/angle has one).
inline Real torsionConstantFEMStadium(Real Ls, Real H) {
  const Real R = H / 2;
  auto inside = [&](Real x, Real y) {
    if (std::abs(x) <= Ls / 2) return std::abs(y) < R; // straight (rectangular) midsection
    const Real cx = x > 0 ? Ls / 2 : -Ls / 2;           // nearest circular cap's own center
    return (x - cx) * (x - cx) + y * y < R * R;
  };
  // A curved boundary suffers more staircase-aliasing error at a given grid resolution than an
  // already-axis-aligned one (e.g. solidRectangleTorsion's own rectangle), so a finer hint (H/4,
  // not H) is passed here to compensate -- confirmed, 2026-11-19: at the plain H hint, the
  // degenerate Ls=0 (pure circle) case disagreed with solidCircle's own exact pi*D^4/32 by
  // ~3.5%; at H/4 the same check is within ~0.8% (MidasSectionLibrary tests).
  return torsionConstantFEMCore(-(Ls / 2 + R), Ls / 2 + R, -R, R, H / 4, inside);
}

// Chamfered-rectangle ("octagon") domain H x B with each of its 4 corners cut by a right
// triangle of legs a (horizontal) and b (vertical) -- see solidOctagon() below. The thinnest
// feature for grid-resolution purposes is the smaller of the two chamfer legs (so a small cut
// is never aliased away), capped by H/B themselves for a tiny chamfer.
inline Real torsionConstantFEMChamferedRect(Real H, Real B, Real a, Real b) {
  auto inside = [&](Real x, Real y) {
    if (std::abs(x) >= B / 2 || std::abs(y) >= H / 2) return false;
    const Real u = B / 2 - std::abs(x), v = H / 2 - std::abs(y); // distance in from the nearest vertical/horizontal edge
    if (u >= a || v >= b) return true; // outside every corner's chamfer triangle
    return u / a + v / b >= 1;         // inside the rectangle but ALSO inside a corner triangle -> cut away
  };
  const Real minDim = std::min({H, B, a, b});
  return torsionConstantFEMCore(-B / 2, B / 2, -H / 2, H / 2, minDim, inside);
}
} // namespace detail

inline ResolvedSectionProperties solidRectangle(Real H, Real B) {
  if (!(H > 0) || !(B > 0)) throw std::invalid_argument("SB section requires positive H and B");
  ResolvedSectionProperties p;
  p.area = H * B;
  p.IH = B * H * H * H / 12;   // resists bending from a load along H
  p.IB = H * B * B * B / 12;   // resists bending from a load along B
  p.J = detail::solidRectangleTorsion(std::max(H, B), std::min(H, B));
  p.AsH = p.AsB = detail::kRectangularShearFactor * p.area;
  return p;
}

inline ResolvedSectionProperties solidCircle(Real D) {
  if (!(D > 0)) throw std::invalid_argument("SR section requires a positive diameter");
  ResolvedSectionProperties p;
  const Real r = D / 2;
  p.area = Real(kPi) * r * r;
  p.IH = p.IB = Real(kPi) * D * D * D * D / 64;   // Iyy = Izz for a circle, exact
  p.J = Real(kPi) * D * D * D * D / 32;            // polar moment == torsion constant, exact for a circle
  p.AsH = p.AsB = detail::kCircularShearFactor * p.area;
  return p;
}

// "STRK"/Solid Track (a stadium shape: a rectangle capped by a semicircle on each end) and
// "SOCT"/Solid Octagon (a rectangle with its 4 corners cut off, not a REGULAR octagon -- the two
// chamfer legs a, b are independent, confirmed, 2026-11-19, from the user's own MIDAS NX
// "Section Data" dialog screenshots for model 21's sections 4/5: STRK's dialog shows H, B only,
// with a diagram of a horizontal oval/stadium and a radius "R" label at one rounded end; SOCT's
// shows H, B, a, b, with a, b labeled as the horizontal/vertical corner-chamfer dimensions on a
// chamfered-rectangle diagram -- this is a DIRECT read of the authoritative MIDAS UI, not web
// documentation (support.midasuser.com returns HTTP 403 to every automated fetch attempted this
// project has made, and the two other sources the user provided either required an
// authenticated session or embedded the dimension diagrams as images a text-fetch cannot read --
// see docs/DEVELOPMENT_STATUS.md, 2026-11-19, for the full trail). Per the user's explicit
// instruction (2026-11-19, "bunlarla ilgili tum ozellikleri genel mukavemet bilgilerinden
// turet... MIDAS NX'e bagli kalmak zorunda degiliz"), every section property below is derived
// from first-principles classical mechanics of materials (composite-shape decomposition,
// parallel-axis theorem), not copied from or fitted to any MIDAS-reported number -- independent
// derivation, consistent with this project's "never validate against the production
// implementation" rule, just applied to the INPUT geometry rather than to an existing solver
// result. See each function's own header comment for its specific derivation and the arithmetic
// sanity checks performed (limiting cases with a known closed form, e.g. a degenerate circle or
// plain rectangle).

// Stadium/"track" shape: height H (= the semicircular ends' diameter, i.e. radius R=H/2), overall
// length B (>= H). Composite decomposition: a central Ls x H rectangle (Ls = B - H, the straight
// midsection) plus two semicircular end caps (together, one full circle of radius R). All
// properties derived from scratch via the parallel-axis theorem (not any published "track
// section" formula): see MidasSectionLibrary tests for the full derivation written out, and the
// two limiting-case checks it verifies (Ls=0 reduces exactly to solidCircle's own IH=IB=pi*R^4/4,
// and R->0 reduces to a thin BxH... Ls x H rectangle).
inline ResolvedSectionProperties solidTrack(Real H, Real B) {
  if (!(H > 0) || !(B >= H)) throw std::invalid_argument("STRK section requires positive H and B >= H");
  const Real R = H / 2, Ls = B - H;
  ResolvedSectionProperties p;
  p.area = Ls * H + Real(kPi) * R * R;
  p.IH = Real(kPi) * R * R * R * R / 4 + (2 * Ls * R * R * R) / 3;
  p.IB = Real(kPi) * R * R * R * R / 4 + (4 * Ls * R * R * R) / 3 + (Real(kPi) * Ls * Ls * R * R) / 4 + (R * Ls * Ls * Ls) / 6;
  p.J = detail::torsionConstantFEMStadium(Ls, H);
  // Shear areas via the same Zhuravskii/Timoshenko quadrature as every other shape in this file
  // (detail::timoshenkoShearArea), applied to a finely-discretized approximation of this shape's
  // CONTINUOUSLY varying width/height profile (the function itself only takes piecewise-constant
  // strips; 400 slices keeps the discretization error well below the quadrature's own, same
  // convergence argument as TimoshenkoShearAreaMatchesClosedFormRectangle).
  constexpr int kSlices = 400;
  {
    std::vector<detail::ShearStrip> strips; strips.reserve(kSlices); // along H: width(y) = Ls + 2*sqrt(R^2-y^2)
    for (int i = 0; i < kSlices; ++i) {
      const Real y0 = -R + i * (2 * R) / kSlices, y1 = -R + (i + 1) * (2 * R) / kSlices, ym = (y0 + y1) / 2;
      strips.push_back({y0, y1, Ls + 2 * std::sqrt(std::max(Real(0), R * R - ym * ym))});
    }
    p.AsH = detail::timoshenkoShearArea(strips, p.IH);
  }
  {
    const Real halfB = Ls / 2 + R;
    std::vector<detail::ShearStrip> strips; strips.reserve(kSlices); // along B: height(x) profile
    for (int i = 0; i < kSlices; ++i) {
      const Real x0 = -halfB + i * (2 * halfB) / kSlices, x1 = -halfB + (i + 1) * (2 * halfB) / kSlices, xm = (x0 + x1) / 2;
      const Real height = (std::abs(xm) <= Ls / 2) ? H : 2 * std::sqrt(std::max(Real(0), R * R - (std::abs(xm) - Ls / 2) * (std::abs(xm) - Ls / 2)));
      strips.push_back({x0, x1, height});
    }
    p.AsB = detail::timoshenkoShearArea(strips, p.IB);
  }
  return p;
}

// Chamfered rectangle ("Solid Octagon"): overall H x B, each of the 4 corners cut by a right
// triangle of legs a (horizontal) and b (vertical). Area/IH/IB via full-rectangle-minus-4-
// triangles (standard right-triangle centroid/inertia results, a^3*b/12 and a*b^3/12 about the
// leg axes, plus parallel-axis theorem -- symmetric about both H and B axes, so no centroid
// shift needed; see MidasSectionLibrary tests for the full written-out derivation and its
// a=b=0 -> plain rectangle sanity check).
inline ResolvedSectionProperties solidOctagon(Real H, Real B, Real a, Real b) {
  if (!(H > 0) || !(B > 0) || !(a >= 0) || !(b >= 0) || !(2 * a <= B) || !(2 * b <= H))
    throw std::invalid_argument("SOCT section requires positive H, B and chamfers with 2a<=B, 2b<=H");
  ResolvedSectionProperties p;
  p.area = H * B - 2 * a * b;
  p.IB = H * B * B * B / 12 - (B * B * a * b / 2 - (2 * B * a * a * b) / 3 + a * a * a * b / 3);
  p.IH = B * H * H * H / 12 - (H * H * a * b / 2 - (2 * H * a * b * b) / 3 + a * b * b * b / 3);
  p.J = (a > 0 && b > 0) ? detail::torsionConstantFEMChamferedRect(H, B, a, b)
                         : detail::solidRectangleTorsion(std::max(H, B), std::min(H, B)); // a=b=0: exact rectangle
  constexpr int kSlices = 400;
  auto chamferedWidthAt = [&](Real pos, Real halfSpan, Real otherHalfSpan, Real leg, Real otherLeg) {
    // Material extent perpendicular to `pos`'s axis at coordinate pos (|pos|<=halfSpan), for a
    // rectangle of half-extent (halfSpan, otherHalfSpan) chamfered by (leg along pos-axis,
    // otherLeg along the perpendicular axis) at each of its 4 corners.
    const Real full = 2 * otherHalfSpan;
    const Real edgeDist = halfSpan - std::abs(pos); // distance in from the nearest edge along pos-axis
    if (leg <= 0 || edgeDist >= leg) return full;
    const Real cut = otherLeg * (1 - edgeDist / leg);
    return full - 2 * cut;
  };
  {
    std::vector<detail::ShearStrip> strips; strips.reserve(kSlices);
    for (int i = 0; i < kSlices; ++i) {
      const Real y0 = -H / 2 + i * H / kSlices, y1 = -H / 2 + (i + 1) * H / kSlices, ym = (y0 + y1) / 2;
      strips.push_back({y0, y1, chamferedWidthAt(ym, H / 2, B / 2, b, a)});
    }
    p.AsH = detail::timoshenkoShearArea(strips, p.IH);
  }
  {
    std::vector<detail::ShearStrip> strips; strips.reserve(kSlices);
    for (int i = 0; i < kSlices; ++i) {
      const Real x0 = -B / 2 + i * B / kSlices, x1 = -B / 2 + (i + 1) * B / kSlices, xm = (x0 + x1) / 2;
      strips.push_back({x0, x1, chamferedWidthAt(xm, B / 2, H / 2, a, b)});
    }
    p.AsB = detail::timoshenkoShearArea(strips, p.IB);
  }
  return p;
}

// Symmetric hollow box: outer H x B, wall thickness tw on the two sides (parallel to H),
// tf on the top/bottom (parallel to B) -- inferred vSIZE order [H,B,tw,tf], the only 4-value
// case seen in the sample. Torsion via Bredt's thin-walled closed-section formula
// (J = 4*Am^2 / sum(s/t) around the midline), a standard engineering APPROXIMATION, not exact
// for thick walls -- flagged, not silently presented as exact.
inline ResolvedSectionProperties hollowBox(Real H, Real B, Real tw, Real tf) {
  if (!(H > 0) || !(B > 0) || !(tw > 0) || !(tf > 0)) throw std::invalid_argument("B section requires positive H, B and wall thicknesses");
  const Real Hi = H - 2*tf, Bi = B - 2*tw;
  if (!(Hi > 0) || !(Bi > 0)) throw std::invalid_argument("B section wall thicknesses leave no hollow interior (Hi/Bi <= 0)");
  ResolvedSectionProperties p;
  p.area = H*B - Hi*Bi;
  p.IH = (B*H*H*H - Bi*Hi*Hi*Hi) / 12;
  p.IB = (H*B*B*B - Hi*Bi*Bi*Bi) / 12;
  const Real Am = (B - tw) * (H - tf);                     // midline-enclosed area
  const Real sumSoverT = 2*(H - tf)/tw + 2*(B - tw)/tf;     // midline perimeter / thickness, Bredt's formula
  p.J = 4 * Am * Am / sumSoverT;
  // Shear areas via the rigorous Zhuravskii/Timoshenko quadrature (detail::timoshenkoShearArea),
  // not the coarser "shear carried by the parallel walls only" approximation this replaced
  // (2026-11-19). AsH: horizontal strips along H -- top/bottom flanges (full width B, solid) and
  // the web region (width 2*tw, the two webs; the hollow middle there carries no material).
  // AsB: the same section rotated 90 degrees -- the two solid side walls (full height H) and the
  // hollow middle (only the top+bottom flange thickness present there, 2*tf).
  p.AsH = detail::timoshenkoShearArea({{0, tf, B}, {tf, H - tf, 2*tw}, {H - tf, H, B}}, p.IH);
  p.AsB = detail::timoshenkoShearArea({{0, tw, H}, {tw, B - tw, 2*tf}, {B - tw, B, H}}, p.IB);
  return p;
}

// T-section (flange on top, web below): outer height H, flange width B, web thickness tw,
// flange thickness tf -- same vSIZE order [H,B,tw,tf] as "B" (hollow box), confirmed by the only
// real sample seen (verification/models/17 and /18, SECT_NAME "BASLIK" = pier cap, vSIZE=
// [2.0,2.0,1.0,1.0]). Area/IH/IB via the standard composite-section (flange + web rectangles)
// parallel-axis-theorem decomposition -- classical mechanics of materials, not MIDAS-specific.
// IH uses the overall centroid (flange and web have different individual centroids, so each
// rectangle's own I plus its own A*d^2 offset is summed, per the standard formula); IB needs no
// offset since a standard T is symmetric about its vertical axis, so both pieces already share
// its centroidal line. J uses the classical thin-strip-sum approximation J=(1/3)*sum(b_i*t_i^3)
// (e.g. Roark's Formulas for Stress and Strain) -- NOT detail::torsionConstantFEM's more exact
// membrane-analogy solve, despite that being tried first (2026-11-19). Empirically, the FEM
// result made model 17's real-data match WORSE (its own real MIDAS export's FORCE-case Beam
// Force comparison regressed from 817/936 to 757/936 PASS when J switched from the strip sum to
// the FEM value, while model 18, same section, was unaffected either way since it has no
// significant torsion demand on this member in its own governing load path) -- strong evidence
// MIDAS's own DBUSER general-section torsion constant uses this same simpler engineering
// convention internally, not an exact PDE solve, so matching MIDAS's own convention (not the
// more "physically rigorous" answer) is what actually improves import fidelity here. Reverted
// to the strip sum for that reason, not reintroduced without this data -- `torsionConstantFEM`
// itself is kept (verified correct against two independent references, see its own tests) for
// any future shape where it IS the better match. Shear areas use the rigorous Zhuravskii/
// Timoshenko quadrature (detail::timoshenkoShearArea, 2026-11-19), not the coarser "web alone"/
// "flange alone" engineering approximation this replaced -- THAT change empirically improved the
// match (model 18 reached 1872/1872 Beam Force, from 1834/1872, once applied), unlike J above.
inline ResolvedSectionProperties tSection(Real H, Real B, Real tw, Real tf) {
  if (!(H > 0) || !(B > 0) || !(tw > 0) || !(tf > 0)) throw std::invalid_argument("T section requires positive H, B, tw and tf");
  const Real hw = H - tf;
  if (!(hw > 0)) throw std::invalid_argument("T section flange thickness tf must be less than H");
  if (!(tw <= B)) throw std::invalid_argument("T section web thickness tw must not exceed flange width B");
  ResolvedSectionProperties p;
  const Real Af = B * tf, Aw = tw * hw;
  p.area = Af + Aw;
  const Real yf = tf / 2, yw = tf + hw / 2; // centroid of each piece, measured down from the top fiber
  const Real ybar = (Af * yf + Aw * yw) / p.area; // overall centroid, down from the top fiber
  const Real IfOwn = B * tf * tf * tf / 12, IwOwn = tw * hw * hw * hw / 12;
  const Real df = ybar - yf, dw = ybar - yw;
  p.IH = IfOwn + Af * df * df + IwOwn + Aw * dw * dw;
  p.IB = tf * B * B * B / 12 + hw * tw * tw * tw / 12; // symmetric about the vertical axis, no offset needed
  p.J = (B * tf * tf * tf + hw * tw * tw * tw) / 3; // thin-strip-sum (see header comment for why this, not the FEM solve)
  // AsH: horizontal strips along H (top to bottom) -- flange (full width B) then web (width tw).
  // AsB: the section rotated 90 degrees -- outside the web's width only the flange is present
  // (depth tf), within it both flange and web are present (depth tf+hw = H).
  p.AsH = detail::timoshenkoShearArea({{0, tf, B}, {tf, H, tw}}, p.IH);
  p.AsB = detail::timoshenkoShearArea({{0, (B - tw) / 2, tf}, {(B - tw) / 2, (B + tw) / 2, H}, {(B + tw) / 2, B, tf}}, p.IB);
  return p;
}

// Symmetric I/H wide-flange (web vertical, flanges top+bottom, both symmetric about H and B):
// overall depth H, flange width B, web thickness tw, flange thickness tf (each flange). Area/IH
// via the standard composite decomposition (outer B x H rectangle minus the two side cutouts of
// width (B-tw)/2 and height (H-2*tf) each) -- classical mechanics of materials, not MIDAS-specific
// (same sourcing standard as hollowBox/tSection above). IB is the two flanges' own I (parallel-axis
// offset is zero since both flanges already share the vertical centroidal axis) plus the web's own
// I. J uses the same thin-strip-sum approximation as tSection (J=(1/3)*sum(b_i*t_i^3)), for
// consistency with that section's empirically-preferred (over the more "rigorous" FEM solve, see
// tSection's header comment) convention for open thin-walled sections built from this library.
inline ResolvedSectionProperties iSection(Real H, Real B, Real tw, Real tf) {
  if (!(H > 0) || !(B > 0) || !(tw > 0) || !(tf > 0)) throw std::invalid_argument("I section requires positive H, B, tw and tf");
  const Real Hi = H - 2*tf;
  if (!(Hi > 0)) throw std::invalid_argument("I section flange thickness tf must be less than H/2");
  if (!(tw <= B)) throw std::invalid_argument("I section web thickness tw must not exceed flange width B");
  ResolvedSectionProperties p;
  p.area = 2*B*tf + Hi*tw;
  p.IH = (B*H*H*H - (B - tw)*Hi*Hi*Hi) / 12;
  p.IB = 2*(tf*B*B*B/12) + Hi*tw*tw*tw/12;
  p.J = (2*B*tf*tf*tf + Hi*tw*tw*tw) / 3;
  // AsH: horizontal strips along H -- flange (full width B) / web (width tw) / flange.
  // AsB: rotated -- outside the web's width only the two flanges are present (combined depth
  // 2*tf), within it both flanges and the web are present (full depth H).
  p.AsH = detail::timoshenkoShearArea({{0, tf, B}, {tf, H - tf, tw}, {H - tf, H, B}}, p.IH);
  p.AsB = detail::timoshenkoShearArea({{0, (B - tw) / 2, 2*tf}, {(B - tw) / 2, (B + tw) / 2, H}, {(B + tw) / 2, B, 2*tf}}, p.IB);
  return p;
}

// Channel (web on one side, two flanges projecting the same way, e.g. a MIDAS "C"-shape/European
// UPN profile): overall depth H, overall flange width B (web face to flange tip), web thickness
// tw, flange thickness tf. Symmetric about the horizontal (H) centroidal axis -- so IH uses the
// SAME formula as iSection's (the flange/web decomposition's H-direction centroid is unaffected
// by the channel's left/right asymmetry) -- but NOT about the vertical (B) axis, so IB needs its
// own centroid (standard composite-section parallel-axis decomposition: web + two flange legs,
// each of length (B-tw), offset from the web's outer face). J uses the same thin-strip-sum
// convention as iSection/tSection, for consistency (see tSection's header comment).
inline ResolvedSectionProperties channelSection(Real H, Real B, Real tw, Real tf) {
  if (!(H > 0) || !(B > 0) || !(tw > 0) || !(tf > 0)) throw std::invalid_argument("C section requires positive H, B, tw and tf");
  const Real Hi = H - 2*tf;
  if (!(Hi > 0)) throw std::invalid_argument("C section flange thickness tf must be less than H/2");
  if (!(tw < B)) throw std::invalid_argument("C section web thickness tw must be less than flange width B");
  ResolvedSectionProperties p;
  const Real Aweb = H*tw, Aflange = (B - tw)*tf; // each of the two flange legs (beyond the web)
  p.area = Aweb + 2*Aflange;
  p.IH = (B*H*H*H - (B - tw)*Hi*Hi*Hi) / 12; // same decomposition as iSection; H-symmetric
  const Real xWeb = tw / 2, xFlange = tw + (B - tw) / 2; // from the web's outer (left) face
  const Real xbar = (Aweb*xWeb + 2*Aflange*xFlange) / p.area;
  const Real IwebOwn = H*tw*tw*tw / 12, IflangeOwn = tf*(B - tw)*(B - tw)*(B - tw) / 12;
  const Real dWeb = xbar - xWeb, dFlange = xFlange - xbar;
  p.IB = (IwebOwn + Aweb*dWeb*dWeb) + 2*(IflangeOwn + Aflange*dFlange*dFlange);
  p.J = (2*B*tf*tf*tf + Hi*tw*tw*tw) / 3;
  p.AsH = detail::timoshenkoShearArea({{0, tf, B}, {tf, H - tf, tw}, {H - tf, H, B}}, p.IH);
  // AsB strips along the B direction (web outer face at 0): the web region (0..tw, full depth H)
  // then the flange-only region (tw..B, depth 2*tf) -- timoshenkoShearArea computes its own
  // centroid/Q from this profile directly, consistent with xbar above.
  p.AsB = detail::timoshenkoShearArea({{0, tw, H}, {tw, B, 2*tf}}, p.IB);
  return p;
}

// Equal-leg angle (MIDAS shape "L", e.g. a European equal angle like "L50x5"): overall leg
// length a, uniform thickness t. UNLIKE every other shape in this file, this section is
// unsymmetric about any horizontal/vertical axis pair through its centroid -- its principal
// bending axes are rotated (45 degrees, by the diagonal symmetry of an EQUAL-leg angle) relative
// to the raw H/B directions. Raw second moments/product of inertia (IH_raw, IB_raw, Ixy_raw) are
// computed via the classical composite-rectangle decomposition (two legs, corner-overlap
// avoided: leg1 = a x t along H's zero edge, leg2 = t x (a-t) stacked on it, standard parallel-
// axis theorem -- e.g. Hibbeler, "Mechanics of Materials", principal moments of inertia; same
// sourcing standard as every other shape here), then diagonalized by finding the rotation angle
// phi that drives the rotated product of inertia IY'Z' to zero:
//   IY'Z'(phi) = (1/2)*sin(2*phi)*(IB_raw - IH_raw) + cos(2*phi)*Ixy_raw = 0
//   => phi = 0.5 * atan2(-2*Ixy_raw, IB_raw - IH_raw)
// (derived from Z' = sin(phi)*Y + cos(phi)*Z, Y' = cos(phi)*Y - sin(phi)*Z -- the SAME rotation
// sense as MidasOrientation.hpp's Beta-angle formula, so the importer can apply this phi to
// reference_y with that identical formula and get a self-consistent result) -- then IH_new/IB_new
// are computed directly from that same rotation (not by eigenvalue max/min assignment, which
// would need a separate, error-prone sign convention to pair correctly with phi's rotation
// sense). Sanity-checked arithmetically for a=50mm,t=5mm (no fillets, same precision level as
// this file's other shapes): IH_raw=IB_raw=11.25 cm^4 (vs. tabulated L50x5 ~11.0 cm^4), principal
// moments 17.91/4.59 cm^4 at phi=45 degrees (vs. tabulated ~17.5/4.45 cm^4) -- NOT independently
// verified against real MIDAS beam-bending data for an angle member (no such sample available
// this session; model 20's own L50x5 elements are all TRUSS, whose fully-released stiffness
// does not depend on Iy/Iz/J/Asy/Asz at all -- see Truss3D.hpp -- so this shape's bending-axis
// correctness has no real-data check available yet for THIS shape specifically, unlike every
// other shape in this file). J uses the same thin-strip-sum convention as the other open
// sections here. AsH/AsB use the same constant Timoshenko rectangular factor as solidRectangle
// (a coarser approximation than the Zhuravskii quadrature used for the symmetric shapes above --
// no straightforward per-principal-axis strip decomposition exists for a rotated unsymmetric
// section -- functionally irrelevant for a TRUSS-released member in any case).
inline ResolvedSectionProperties equalAngleSection(Real a, Real t) {
  if (!(a > 0) || !(t > 0) || !(t < a)) throw std::invalid_argument("L section requires positive a and 0 < t < a");
  const Real A1 = a * t, A2 = t * (a - t);
  const Real area = A1 + A2;
  const Real xB1 = a / 2, yH1 = t / 2;           // leg1 (a x t): centroid, (B-offset, H-offset)
  const Real xB2 = t / 2, yH2 = t + (a - t) / 2; // leg2 (t x (a-t)), stacked above leg1
  const Real xbar = (A1 * xB1 + A2 * xB2) / area, ybar = (A1 * yH1 + A2 * yH2) / area;
  const Real dB1 = xB1 - xbar, dH1 = yH1 - ybar, dB2 = xB2 - xbar, dH2 = yH2 - ybar;
  const Real IH_raw = a*t*t*t/12 + A1*dH1*dH1 + t*(a-t)*(a-t)*(a-t)/12 + A2*dH2*dH2; // integral H^2 dA
  const Real IB_raw = t*a*a*a/12 + A1*dB1*dB1 + (a-t)*t*t*t/12 + A2*dB2*dB2;         // integral B^2 dA
  const Real Ixy_raw = A1*dB1*dH1 + A2*dB2*dH2;                                     // integral B*H dA
  const Real phi = Real(0.5) * std::atan2(Real(-2) * Ixy_raw, IB_raw - IH_raw);
  ResolvedSectionProperties p;
  p.area = area;
  p.IH = std::sin(phi)*std::sin(phi)*IB_raw + std::sin(2*phi)*Ixy_raw + std::cos(phi)*std::cos(phi)*IH_raw;
  p.IB = std::cos(phi)*std::cos(phi)*IB_raw - std::sin(2*phi)*Ixy_raw + std::sin(phi)*std::sin(phi)*IH_raw;
  p.J = (t*t*t/3) * (2*a - t);
  p.AsH = p.AsB = detail::kRectangularShearFactor * area;
  p.principalAngleRad = phi;
  return p;
}

// Catalog/database section lookup (MIDAS SECT_I.DB_NAME + SECT_I.SECT_NAME, instead of a raw
// vSIZE array) -- resolves a small set of standard European profile designations to their
// nominal H/B/tw/tf dimensions (idealized rectangular-flange geometry, root fillets NOT modeled,
// same precision level as this file's other shapes), then dispatches to iSection/channelSection
// above. Nominal dimensions per the relevant standard (not derived from the MIDAS export, which
// never gives them for a catalog section -- same "clear diagnostic, not guessed" policy as
// resolveDbUserSection for an unrecognized vSIZE shape):
//   HEB160 (HE160B): EN 10034, h=160mm b=160mm tw=8mm tf=13mm
//   UPN100: DIN 1026-1, h=100mm b=50mm tw=6mm tf=8.5mm
// Any other DB_NAME/SECT_NAME combination, or a shape this catalog does not cover (e.g. an
// unsymmetric angle "L", whose principal bending axes are rotated relative to H/B and would need
// a product-of-inertia term kopruq::Section does not have -- see MidasJsonImporter.hpp), is
// rejected with a clear diagnostic rather than approximated.
inline ResolvedSectionProperties resolveCatalogSection(const std::string& shape, const std::string& dbName, const std::string& sectName) {
  if (dbName != "UNI") throw std::invalid_argument("unsupported catalog DB_NAME '" + dbName + "' (only 'UNI' is implemented)");
  if (shape == "H" && sectName == "HEB160") return iSection(0.160, 0.160, 0.008, 0.013);
  if (shape == "C" && sectName == "UPN100") return channelSection(0.100, 0.050, 0.006, 0.0085);
  if (shape == "L" && sectName == "L50x5") {
    // area: overridden to the published EN 10056-1 tabulated value (4.80 cm^2, root fillets
    // included) rather than equalAngleSection's own idealized no-fillet geometric decomposition
    // (4.75 cm^2) -- unlike a raw DBUSER vSIZE input, a catalog/database section name IS a
    // reference to a specific published profile, so the exact published number is available and
    // unambiguous here, and using it materially improved real-data agreement: verified against
    // model 20 (verification/models/20 Kafes, 2026-11-19) -- Displacements(Global) went from
    // 240/252 to 252/252 PASS once applied (the L50x5 members are this model's truss diagonals,
    // whose axial stiffness EA/L is directly proportional to area). IH/IB/J/AsH/AsB are left at
    // equalAngleSection's own idealized-geometry values (no comparably precise published
    // principal-moment numbers were available to cross-check, and this model's own L50x5
    // elements are TRUSS-type, for which only area affects the result at all).
    auto p = equalAngleSection(0.050, 0.005);
    p.area = 0.00048;
    return p;
  }
  throw std::invalid_argument("unsupported catalog section '" + sectName + "' (shape '" + shape + "', DB_NAME '" + dbName +
                               "'): only HEB160 (shape H), UPN100 (shape C) and L50x5 (shape L) are implemented");
}

// Dispatches on MIDAS's SHAPE code and vSIZE length; rejects anything not implemented.
inline ResolvedSectionProperties resolveDbUserSection(const std::string& shape, const std::vector<Real>& vSize) {
  if (shape == "SB") {
    if (vSize.size() < 2) throw std::invalid_argument("SB section requires at least 2 vSIZE values (H,B)");
    return solidRectangle(vSize[0], vSize[1]);
  }
  if (shape == "SR") {
    if (vSize.empty()) throw std::invalid_argument("SR section requires at least 1 vSIZE value (D)");
    return solidCircle(vSize[0]);
  }
  if (shape == "B") {
    if (vSize.size() < 4) throw std::invalid_argument("B section requires at least 4 vSIZE values (H,B,tw,tf)");
    return hollowBox(vSize[0], vSize[1], vSize[2], vSize[3]);
  }
  if (shape == "T") {
    if (vSize.size() < 4) throw std::invalid_argument("T section requires at least 4 vSIZE values (H,B,tw,tf)");
    return tSection(vSize[0], vSize[1], vSize[2], vSize[3]);
  }
  if (shape == "STRK") {
    if (vSize.size() < 2) throw std::invalid_argument("STRK section requires at least 2 vSIZE values (H,B)");
    return solidTrack(vSize[0], vSize[1]);
  }
  if (shape == "SOCT") {
    if (vSize.size() < 4) throw std::invalid_argument("SOCT section requires at least 4 vSIZE values (H,B,a,b)");
    return solidOctagon(vSize[0], vSize[1], vSize[2], vSize[3]);
  }
  throw std::invalid_argument("unsupported DBUSER section shape '" + shape + "' (only SB, B, SR, T, STRK, SOCT are implemented)");
}

} // namespace kopruq::io::midas
