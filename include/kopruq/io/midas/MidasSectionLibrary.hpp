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
#include "../../sections/SectionFE.hpp"
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
// extentH / extentB: the section's overall outer dimensions along H (local z at Beta=0) and B
// (local y) -- what MIDAS's temperature gradient "Use Section Hz / Hy" takes as the distance
// between the two outermost faces (confirmed on model 22's PSC box, 2026-11-20). Set by the
// dispatchers (resolveDbUserSection / psc1CellSection); 0 means "not defined", and a gradient
// needing it is rejected.
// topFiber: distance from the centroid up to the +z (top, H-direction) extreme fiber -- where a MIDAS
// "Center-Top" (OFFSET_PT "CT") section offset puts the node (MidasJsonImporter.hpp). Set by the
// shape functions whose centroid is not at mid-depth (T, unequal-flange H, PSC); withExtents()
// fills H/2 for every other (vertically symmetric) shape. 0 = not defined.
struct ResolvedSectionProperties {
  Real area{}, IH{}, IB{}, J{}, AsH{}, AsB{}; Real extentH{0}, extentB{0}; Real topFiber{0};
  bool axialOnly{false}; // only valid for TRUSS members (bending conventions unverified), see angleUserSection
};

namespace detail {
inline ResolvedSectionProperties withExtents(ResolvedSectionProperties p, Real H, Real B) { p.extentH = H; p.extentB = B; if (p.topFiber == 0) p.topFiber = H / 2; return p; }
} // namespace detail

namespace detail {
// Saint-Venant torsion constant, solid rectangle (a = longer side, b = shorter side),
// classical approximate formula (e.g. Boresi & Schmidt, "Advanced Mechanics of Materials"):
//   J = a*b^3*(1/3 - 0.21*(b/a)*(1 - (b/a)^4/12))
inline Real solidRectangleTorsion(Real a, Real b) {
  if (a < b) std::swap(a, b);
  const Real r = b / a;
  return a * b * b * b * (Real(1)/3 - Real(0.21) * r * (Real(1) - r*r*r*r/12));
}
// Roark's torsion constant of a T section without fillet (flange B x tf, web hw x tw below it,
// hw = H - tf): K1 + K2 + alpha D^4 -- see tSection() for the formula, source and the MIDAS
// evidence that this is MIDAS NX's own DBUSER T convention.
inline Real roarkTeeTorsion(Real B, Real tf, Real hw, Real tw) {
  const Real K1 = B * tf * tf * tf * (Real(1) / 3 - Real(0.21) * (tf / B) * (1 - std::pow(tf / B, 4) / 12));
  const Real K2 = hw * tw * tw * tw * (Real(1) / 3 - Real(0.105) * (tw / hw) * (1 - std::pow(tw / hw, 4) / 192));
  const Real t = std::min(tf, tw), t1 = std::max(tf, tw);
  const Real alpha = (t / t1) * Real(0.15);
  const Real D = (tf * tf + tw * tw / 4) / tf;
  return K1 + K2 + alpha * D * D * D * D;
}
// Roark's torsion constant of a symmetric I section WITH web-to-flange fillets r (El Darwish &
// Johnston; Roark's Formulas for Stress and Strain, torsion of an I section): flanges b x tf, web
// tw over the clear height h - 2 tf:
//   K1 = b tf^3 [1/3 - 0.21 (tf/b)(1 - tf^4/(12 b^4))],  K2 = (h - 2 tf) tw^3 / 3,
//   alpha = (t/t1)(0.15 + 0.10 r/tf), t = min(tf, tw), t1 = max(tf, tw),
//   D = ((tf + r)^2 + tw (r + tw/4)) / (2 r + tf),      J = 2 K1 + K2 + 2 alpha D^4.
// MIDAS NX's DBUSER H convention when r1 > 0 -- see unequalFlangeISection().
inline Real roarkIFilletTorsion(Real h, Real b, Real tw, Real tf, Real r) {
  const Real K1 = b * tf * tf * tf * (Real(1) / 3 - Real(0.21) * (tf / b) * (1 - std::pow(tf / b, 4) / 12));
  const Real K2 = (h - 2 * tf) * tw * tw * tw / 3;
  const Real t = std::min(tf, tw), t1 = std::max(tf, tw);
  const Real alpha = (t / t1) * (Real(0.15) + Real(0.10) * r / tf);
  const Real D = ((tf + r) * (tf + r) + tw * (r + tw / 4)) / (2 * r + tf);
  return 2 * K1 + K2 + 2 * alpha * D * D * D * D;
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
  // J and shear areas follow MIDAS NX's own DBUSER STRK convention (user decision 2026-11-20:
  // match MIDAS). Identified from MIDAS's own data -- its Section_prop export and per-element
  // back-calculation from its displacement/beam-force results -- and confirmed EXACTLY (6
  // significant digits) on two independent geometries, model 21 SEC4 (H=0.9, B=2.0) and SEC6
  // (H=1.2, B=3.0):
  //   J   = Roark solid-rectangle formula (detail::solidRectangleTorsion) applied to the
  //         EQUAL-AREA rectangle of the same height: width A/H, height H
  //   AsH = (5/6)*Ls*H + 0.9*pi*R^2   (shear along H: rectangle part k=5/6, circular ends k=0.9)
  //   AsB =       Ls*H + 0.9*pi*R^2   (shear along B)
  // These are MIDAS's engineering conventions, not exact elasticity: the exact St. Venant J of a
  // stadium is ~4-5% higher (detail::torsionConstantFEMStadium, kept and directly tested).
  p.J = detail::solidRectangleTorsion(p.area / H, H);
  const Real circularPart = detail::kCircularShearFactor * Real(kPi) * R * R;
  p.AsH = detail::kRectangularShearFactor * Ls * H + circularPart;
  p.AsB = Ls * H + circularPart;
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
  // J follows MIDAS NX's own DBUSER SOCT convention (user decision 2026-11-20: match MIDAS):
  //   J = Roark solid-rectangle formula (detail::solidRectangleTorsion) on the reduced rectangle
  //       (B - a/4) x (H - b/4)
  // Identified 2026-11-20 from a controlled series of MIDAS sections the user built for this
  // purpose (model 21 SEC5, SEC7..SEC10: chamfered, a=b~0, a only, b only) and confirmed EXACTLY
  // to 6 significant digits on six geometries, each back-calculated per element (J = T*L/(G*dth))
  // from MIDAS's own displacement/beam-force results and matching its Section_prop export:
  //   [H,B,a,b]=[1.0,2.0,0.3,0.2] 0.379948 | [1.3,2.5,1.0,0.5] 0.818870 | [1.3,2.5,0.4,0.25]
  //   1.026500 (earlier revision) | [1.3,2.5,~0,~0] 1.234699 | [1.3,2.5,0.45,~0] 1.153053 |
  //   [1.3,2.5,~0,0.5] 0.953202.
  // This is MIDAS's engineering convention, NOT exact elasticity: it depends on a and b separately
  // (a chamfer of zero area, e.g. a=0.45, b->0, still lowers J by ~7%), and against the exact St.
  // Venant value (independent Python FDM, h=H/300) it is ~13% LOW for moderate chamfers ([1.0,2.0,
  // 0.3,0.2]: 0.379948 vs 0.437099) but ~28% HIGH for a large one ([1.3,2.5,1.0,0.5]: 0.818870 vs
  // 0.637362). The exact value stays available as
  // detail::torsionConstantFEMChamferedRect (kept and directly tested). a=b=0 reduces exactly to
  // solidRectangle's own J.
  p.J = detail::solidRectangleTorsion(B - a / 4, H - b / 4);
  // Shear areas: MIDAS NX's own DBUSER SOCT convention, AsH = AsB = (5/6)*A -- confirmed exactly
  // on two geometries (model 21 SEC5 and SEC7, MIDAS Section_prop export and back-calculation
  // from MIDAS's own results, 2026-11-20). a=b=0 reduces to solidRectangle's own 5/6*A.
  p.AsH = detail::kRectangularShearFactor * p.area;
  p.AsB = detail::kRectangularShearFactor * p.area;
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
  // Shear areas: MIDAS NX's own DBUSER box convention, the gross wall areas parallel to the shear
  // (user decision 2026-11-20: match MIDAS). Confirmed exactly from MIDAS's own data on two
  // boxes: model 21 B[1.5,1.0,0.1,0.2] (AsH 0.300000, AsB 0.400000; also its Section_prop
  // export) and model 18 B[3,5,0.5,0.5] (AsH 3.00000, back-calculated from its results).
  //   AsH = 2*tw*H  (shear along H carried by the two webs, full height)
  //   AsB = 2*tf*B  (shear along B carried by the two flanges, full width)
  // Supersedes the Zhuravskii quadrature used 2026-11-19..20, which was physically finer but
  // ~0.5-6% away from MIDAS's values (model 18 passed only within tolerance).
  p.AsH = 2 * tw * H;
  p.AsB = 2 * tf * B;
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
  // J and shear areas: MIDAS NX's own DBUSER T convention (user decision 2026-11-20: match MIDAS),
  // superseding the strip-sum J and the Zhuravskii shear areas described above. Evidence, all
  // from MIDAS's own results for the T [H,B,tw,tf] = [2,2,1,1] pier cap ("BASLIK") of models
  // 17/18/22, back-calculated per element:
  //   J   = Roark's T-section formula (Roark's Formulas for Stress and Strain, torsion of a T
  //         section; El Darwish & Johnston), no fillet (r = 0):
  //           K1 = B tf^3 [1/3 - 0.21 (tf/B)(1 - tf^4/(12 B^4))]          (flange)
  //           K2 = hw tw^3 [1/3 - 0.105 (tw/hw)(1 - tw^4/(192 hw^4))]     (web, hw = H - tf)
  //           alpha = (t/t1) 0.15, t = min(tf, tw), t1 = max(tf, tw);  D = (tf^2 + tw^2/4) / tf
  //           J = K1 + K2 + alpha D^4
  //         -> 1.0528515625; MIDAS (model 22, TU case, torsion of the cap beam) 1.052851615.
  //         Exact St. Venant (SectionFE.hpp) would be 0.9276, the strip sum 1.0 -- neither is MIDAS's.
  //   AsH = tw * H                    (shear along H, the web) -> 2.0; MIDAS (model 22) 2.0000
  //   AsB = (5/6) * B * tf            (shear along B, the flange) -> 1.66667; MIDAS (model 18,
  //         POINT case) 1.666666-1.666669
  // The same web / 5/6-flange shear convention as the DBUSER "H" section (unequalFlangeISection).
  // J evidence is one geometry; Roark's formula is stated for tw < 2 tf (D's derivation).
  p.J = detail::roarkTeeTorsion(B, tf, hw, tw);
  p.topFiber = ybar;
  p.AsH = tw * H;
  p.AsB = detail::kRectangularShearFactor * B * tf;
  return p;
}

// MIDAS NX DBUSER "H" (I/H section with possibly unequal flanges), vSIZE [H, B1, tw, tf1, B2, tf2,
// r1, r2]: overall depth H, top flange B1 x tf1, web thickness tw, bottom flange B2 x tf2, r1 the
// web-to-flange fillet radius (see below), r2 the flange-tip radius (rejected). Web clear height
// hw = H - tf1 - tf2. Every property below follows MIDAS NX's own DBUSER H convention, identified
// 2026-11-20 by back-calculating each one per element from MIDAS's own displacement/beam-force
// results of verification model 10 (section "PROFIL" [1.0, 0.5, 0.02, 0.03, 0.6, 0.05], four
// elements, nodal-load cases only) and matched to 6-9 significant digits:
//   A   = B1*tf1 + B2*tf2 + tw*hw                                   (MIDAS 0.06340000)
//   IH  = strong axis, about the overall centroid (parallel-axis)    (MIDAS 0.010876745)
//   IB  = tf1*B1^3/12 + tf2*B2^3/12 + hw*tw^3/12                      (MIDAS 0.001213113)
//   J   = (B1*tf1^3 + B2*tf2^3 + (H - (tf1+tf2)/2)*tw^3) / 3          (MIDAS 0.000032060)
//         -- thin-strip sum with the web measured flange-midline to flange-midline; the clear-
//            height (0.000031953) and full-height (0.000032167) variants do NOT match
//   AsH = tw*H                       (shear along H, the web)          (MIDAS 0.020000)
//   AsB = (5/6)*(B1*tf1 + B2*tf2)    (shear along B, the flanges)      (MIDAS 0.037500)
// A, IH and IB fix the vSIZE order unambiguously (any other pairing changes A or IB). Evidence is
// one geometry (the only DBUSER H available); a second geometry would be a useful confirmation.
// The section is used about its centroid (OFFSET_PT "CC"); for unequal flanges the shear centre
// is off the centroid, which MIDAS's beam does not model either (the 2-node Timoshenko fit of its
// results has residual ~1e-7).
// Fillets (2026-11-20, verification model 20 "PROF-I" [0.16, 0.16, 0.008, 0.013, 0, 0, r1 = 0.015]):
// r1 is the web-to-flange fillet radius -- four spandrels (one each side of the web, under each
// flange). MIDAS includes them in A and the bending inertias (back-calculated A 0.00542514 =
// 2*b*tf + (h-2tf)*tw + (4-pi)*r^2, IH 2.4920e-5 = the strip-integrated 2.491996e-5; IB confirmed
// on model 10, 2026-11-21) but NOT in AsH (0.001280 = H*tw) or AsB.
// Torsion constant (2026-11-21, verification model 10 rebuilt with filleted user H sections):
// r1 = 0 -> the midline thin-strip sum; r1 > 0 -> Roark's filleted I-section formula
// (detail::roarkIFilletTorsion). MIDAS back-calculated J: [0.93218, 0.4191, 0.02032, 0.03429,
// r1 0.02413] 1.4548661-672e-5 vs Roark 1.4548667e-5; [0.35052, 0.37338, 0.015621, 0.015621,
// r1 0.017653] 1.6694107-110e-6 vs 1.6694108e-6; [0.9, 1.0, 0.03, 0.03, r1 0] 2.5830000e-5 =
// strip (Roark 2.5813e-5). This contradicts the manual's DB/User sentence "When the user inputs r1
// and r2, the calculation of Ixx does not take into account r1 and r2" -- MIDAS's results win.
// Roark's formula is for a symmetric I: unequal flanges with r1 > 0 are rejected (no data).
// One spandrel, measured from its flange-face edge (width w(y) = r - sqrt(r^2 - (r-y)^2), 0<=y<=r):
//   A_f = r^2 (1 - pi/4),  S_f = int y dA = r^3 (5/6 - pi/4),  I_f = int y^2 dA = r^4 (1 - 5 pi/16)
// (the same three numbers about the web face, by the spandrel's symmetry). The same model shows that
// B2 = tf2 = 0 means "bottom flange = top flange" (A and IH match the symmetric section); the
// dispatcher applies that.
inline ResolvedSectionProperties unequalFlangeISection(Real H, Real B1, Real tw, Real tf1, Real B2, Real tf2, Real r1 = 0) {
  if (!(H > 0) || !(B1 > 0) || !(tw > 0) || !(tf1 > 0) || !(B2 > 0) || !(tf2 > 0))
    throw std::invalid_argument("H section requires positive H, B1, tw, tf1, B2 and tf2");
  if (!(r1 >= 0)) throw std::invalid_argument("H section fillet radius r1 must not be negative");
  const Real hw = H - tf1 - tf2;
  if (!(hw > 0)) throw std::invalid_argument("H section flange thicknesses tf1+tf2 must be less than H");
  if (!(tw <= B1) || !(tw <= B2)) throw std::invalid_argument("H section web thickness tw must not exceed either flange width");
  if (r1 > 0 && (!(2 * r1 <= hw) || !(tw + 2 * r1 <= std::min(B1, B2))))
    throw std::invalid_argument("H section fillet radius r1 does not fit between the flanges / within the flange width");
  if (r1 > 0 && (B1 != B2 || tf1 != tf2))
    throw std::invalid_argument("H section with fillets r1 > 0 and unequal flanges is not supported (MIDAS's torsion constant verified only for symmetric filleted sections)");
  ResolvedSectionProperties p;
  const Real A1 = B1 * tf1, Aw = tw * hw, A2 = B2 * tf2;
  const Real Af = r1 * r1 * (1 - Real(kPi) / 4), Sf = r1 * r1 * r1 * (Real(5) / 6 - Real(kPi) / 4),
             If = r1 * r1 * r1 * r1 * (1 - 5 * Real(kPi) / 16);
  p.area = A1 + Aw + A2 + 4 * Af;
  // First and second moments about the TOP fiber (zeta measured downward).
  const Real y1 = tf1 / 2, yw = tf1 + hw / 2, y2 = H - tf2 / 2; // piece centroids, down from the top fiber
  Real S0 = A1 * y1 + Aw * yw + A2 * y2;
  Real I0 = B1 * tf1 * tf1 * tf1 / 12 + A1 * y1 * y1 + tw * hw * hw * hw / 12 + Aw * yw * yw + B2 * tf2 * tf2 * tf2 / 12 + A2 * y2 * y2;
  // two spandrels under the top flange (zeta = tf1 + y), two above the bottom flange (zeta = H - tf2 - y)
  S0 += 2 * (tf1 * Af + Sf) + 2 * ((H - tf2) * Af - Sf);
  I0 += 2 * (tf1 * tf1 * Af + 2 * tf1 * Sf + If) + 2 * ((H - tf2) * (H - tf2) * Af - 2 * (H - tf2) * Sf + If);
  const Real ybar = S0 / p.area;
  p.IH = I0 - p.area * ybar * ybar;
  // About the vertical centroidal axis (the web centre line): spandrels at tw/2 + x from it.
  p.IB = tf1 * B1 * B1 * B1 / 12 + tf2 * B2 * B2 * B2 / 12 + hw * tw * tw * tw / 12
       + 4 * ((tw / 2) * (tw / 2) * Af + tw * Sf + If);
  p.topFiber = ybar;
  p.J = r1 > 0 ? detail::roarkIFilletTorsion(H, B1, tw, tf1, r1)                                       // MIDAS, r1 > 0
               : (B1 * tf1 * tf1 * tf1 + B2 * tf2 * tf2 * tf2 + (H - (tf1 + tf2) / 2) * tw * tw * tw) / 3; // MIDAS, r1 = 0
  p.AsH = tw * H;
  p.AsB = detail::kRectangularShearFactor * (A1 + A2);
  return p;
}

// MIDAS NX DBUSER "C" (channel, user dimensions), vSIZE [H, B1, tw, tf1, B2, tf2, r1, r2]: web of
// thickness tw on one side, flanges B1 x tf1 (top) and B2 x tf2 (bottom) measured from the web's
// outer face (B includes tw). Verification model 20 "PROF-U" [0.1, 0.05, 0.006, 0.0085, 0.05,
// 0.0085]: MIDAS's A = 0.001348 = B1*tf1 + B2*tf2 + (H - tf1 - tf2)*tw (back-calculated). About the
// horizontal axis the channel is the same as the I/H section with the same flange widths, so A, IH,
// the centroid depth, AsH = tw*H and J are taken from unequalFlangeISection; IB is the channel's own
// (flanges on one side, own horizontal centroid). IB, J and AsB are geometric / by analogy and NOT
// verified against MIDAS (model 20 is an X-Z plane model: only A, IH, AsH enter it). Fillets rejected.
inline ResolvedSectionProperties channelUserSection(Real H, Real B1, Real tw, Real tf1, Real B2, Real tf2) {
  if (!(tw < B1) || !(tw < B2)) throw std::invalid_argument("C section web thickness tw must be less than both flange widths");
  ResolvedSectionProperties p = unequalFlangeISection(H, B1, tw, tf1, B2, tf2);
  const Real hw = H - tf1 - tf2;
  const Real A1 = B1 * tf1, A2 = B2 * tf2, Aw = hw * tw;
  const Real xbar = (A1 * B1 / 2 + A2 * B2 / 2 + Aw * tw / 2) / p.area; // from the web's outer face
  p.IB = tf1 * B1 * B1 * B1 / 12 + A1 * (B1 / 2 - xbar) * (B1 / 2 - xbar)
       + tf2 * B2 * B2 * B2 / 12 + A2 * (B2 / 2 - xbar) * (B2 / 2 - xbar)
       + hw * tw * tw * tw / 12 + Aw * (tw / 2 - xbar) * (tw / 2 - xbar);
  return p;
}

// MIDAS NX DBUSER "L" (angle, user dimensions), vSIZE [H, B, tw, tf]: vertical leg H x tw, horizontal
// leg B x tf. Verification model 20 "PROF-L" [0.05, 0.05, 0.005, 0.005] (TRUSS diagonals): MIDAS's
// A = 0.000475 = H*tw + (B - tw)*tf (back-calculated from its truss forces and displacements). An
// angle's principal axes are rotated with respect to H/B, and how MIDAS orients a BEAM with an angle
// section is unverified -- so the section is axialOnly: accepted for TRUSS members (only A enters),
// rejected for BEAM members by the importer. The geometric centroidal IH/IB/J returned here only
// keep the (fully released) truss element well-defined; they do not affect a truss result.
inline ResolvedSectionProperties angleUserSection(Real H, Real B, Real tw, Real tf) {
  if (!(H > 0) || !(B > 0) || !(tw > 0) || !(tf > 0) || !(tw < B) || !(tf < H)) throw std::invalid_argument("L section requires positive H, B, tw, tf with tw < B and tf < H");
  ResolvedSectionProperties p;
  const Real Av = H * tw, Ah = (B - tw) * tf;
  p.area = Av + Ah;
  const Real zv = H / 2, zh = tf / 2, xv = tw / 2, xh = tw + (B - tw) / 2; // from the bottom / web outer face
  const Real zbar = (Av * zv + Ah * zh) / p.area, xbar = (Av * xv + Ah * xh) / p.area;
  p.IH = tw * H * H * H / 12 + Av * (zv - zbar) * (zv - zbar) + (B - tw) * tf * tf * tf / 12 + Ah * (zh - zbar) * (zh - zbar);
  p.IB = H * tw * tw * tw / 12 + Av * (xv - xbar) * (xv - xbar) + tf * (B - tw) * (B - tw) * (B - tw) / 12 + Ah * (xh - xbar) * (xh - xbar);
  p.J = (H * tw * tw * tw + (B - tw) * tf * tf * tf) / 3;
  p.AsH = p.AsB = detail::kRectangularShearFactor * p.area;
  p.axialOnly = true;
  return p;
}

// Catalog / database profiles (MIDAS SECT_I.DB_NAME: AISC, EN/UNI, ...) are NOT supported -- removed
// 2026-11-20 by user decision (the solver targets precast concrete bridges first; steel profiles are
// entered as user-defined I/H or T sections). MidasJsonImporter.hpp rejects any DB_NAME section.

// Dispatches on MIDAS's SHAPE code and vSIZE length; rejects anything not implemented.
inline ResolvedSectionProperties resolveDbUserSection(const std::string& shape, const std::vector<Real>& vSize) {
  if (shape == "SB") {
    if (vSize.size() < 2) throw std::invalid_argument("SB section requires at least 2 vSIZE values (H,B)");
    return detail::withExtents(solidRectangle(vSize[0], vSize[1]), vSize[0], vSize[1]);
  }
  if (shape == "SR") {
    if (vSize.empty()) throw std::invalid_argument("SR section requires at least 1 vSIZE value (D)");
    return detail::withExtents(solidCircle(vSize[0]), vSize[0], vSize[0]);
  }
  if (shape == "B") {
    if (vSize.size() < 4) throw std::invalid_argument("B section requires at least 4 vSIZE values (H,B,tw,tf)");
    return detail::withExtents(hollowBox(vSize[0], vSize[1], vSize[2], vSize[3]), vSize[0], vSize[1]);
  }
  if (shape == "T") {
    if (vSize.size() < 4) throw std::invalid_argument("T section requires at least 4 vSIZE values (H,B,tw,tf)");
    return detail::withExtents(tSection(vSize[0], vSize[1], vSize[2], vSize[3]), vSize[0], vSize[1]);
  }
  if (shape == "STRK") {
    if (vSize.size() < 2) throw std::invalid_argument("STRK section requires at least 2 vSIZE values (H,B)");
    return detail::withExtents(solidTrack(vSize[0], vSize[1]), vSize[0], vSize[1]);
  }
  if (shape == "SOCT") {
    if (vSize.size() < 4) throw std::invalid_argument("SOCT section requires at least 4 vSIZE values (H,B,a,b)");
    return detail::withExtents(solidOctagon(vSize[0], vSize[1], vSize[2], vSize[3]), vSize[0], vSize[1]);
  }
  if (shape == "H") {
    if (vSize.size() < 6) throw std::invalid_argument("H section requires at least 6 vSIZE values (H,B1,tw,tf1,B2,tf2)");
    if (vSize.size() > 7 && vSize[7] != 0)
      throw std::invalid_argument("H section with a nonzero flange-tip radius r2 is not supported (no MIDAS data)");
    Real B2 = vSize[4], tf2 = vSize[5];
    if (B2 == 0 && tf2 == 0) { B2 = vSize[1]; tf2 = vSize[3]; } // MIDAS: empty bottom flange = top flange (model 20)
    const Real r1 = vSize.size() > 6 ? vSize[6] : Real(0);
    return detail::withExtents(unequalFlangeISection(vSize[0], vSize[1], vSize[2], vSize[3], B2, tf2, r1), vSize[0], std::max(vSize[1], B2));
  }
  if (shape == "C") {
    if (vSize.size() < 6) throw std::invalid_argument("C section requires at least 6 vSIZE values (H,B1,tw,tf1,B2,tf2)");
    if ((vSize.size() > 6 && vSize[6] != 0) || (vSize.size() > 7 && vSize[7] != 0))
      throw std::invalid_argument("C section with nonzero fillet radii r1/r2 is not supported (no MIDAS data)");
    Real B2 = vSize[4], tf2 = vSize[5];
    if (B2 == 0 && tf2 == 0) { B2 = vSize[1]; tf2 = vSize[3]; }
    return detail::withExtents(channelUserSection(vSize[0], vSize[1], vSize[2], vSize[3], B2, tf2), vSize[0], std::max(vSize[1], B2));
  }
  if (shape == "L") {
    if (vSize.size() < 4) throw std::invalid_argument("L section requires at least 4 vSIZE values (H,B,tw,tf)");
    if ((vSize.size() > 4 && vSize[4] != 0) || (vSize.size() > 5 && vSize[5] != 0))
      throw std::invalid_argument("L section with nonzero radii is not supported (no MIDAS data)");
    return angleUserSection(vSize[0], vSize[1], vSize[2], vSize[3]); // no extents: gradient / CT offset not defined
  }
  throw std::invalid_argument("unsupported DBUSER section shape '" + shape + "' (only SB, B, SR, T, H, C, L (truss only), STRK, SOCT are implemented)");
}

// MIDAS NX PSC "1CEL" (PSC-1CELL single-cell box girder), symmetric, all joints (JO1..JO3,
// JI1..JI5) off. Parameter order of the four JSON arrays = MIDAS API order (midasapi-python
// Section.PSC.CEL12), confirmed for every nonzero entry against the user's MIDAS "Section Data"
// dialog of verification model 22 (2026-11-20):
//   vSIZE_PSC_A = [HO1, HO2, HO2-1, HO2-2, HO3, HO3-1]       vSIZE_PSC_B = [BO1, BO1-1, BO1-2, BO2, BO2-1, BO3]
//   vSIZE_PSC_C = [HI1, HI2, HI2-1, HI2-2, HI3, HI3-1, HI4, HI4-1, HI4-2, HI5]
//   vSIZE_PSC_D = [BI1, BI1-1, BI1-2, BI2-1, BI3, BI3-1, BI3-2, BI4]
// Geometry (MIDAS manual "Section Properties - PSC", PSC-1CELL guide diagram; half section, the
// centre line at y = 0, top at z = 0). Joint dimensions (-1/-2 suffixes, BI4) only apply when the
// matching joint is on, and are ignored otherwise, as in MIDAS's dialog (greyed out):
//   outer: cantilever tip thickness HO1 at y = W = BO1+BO2+BO3; straight underside to the web top
//          at y = BO2+BO3, depth HO1+HO2; outer web face to the bottom corner y = BO3, depth
//          H = HO1+HO2+HO3; flat bottom.
//   inner: top slab HI1 thick on the centre line, straight to the inner web top at y = BI1, depth
//          HI1+HI2; inner web to y = BI3, depth HI1+HI2+HI3; straight to the centre line at depth
//          H-HI5 (bottom slab HI5 thick there, HI5+HI4 at the web). HI1+..+HI5 must equal H.
// A, centroid, Iy, Iz: exact polygon integration (SectionGeometry.hpp). Confirmed on model 22's
// box against MIDAS's own Section Properties table: A 10.58, Iyy 13.67209, Izz 68.70453, centroid
// 1.326008 below the top (all exact).
// J and shear areas: MIDAS computes them numerically on a section mesh (its dialog's "Mesh Size for
// Stiff. Calc.", and its values sit between a coarse and a converged FE solution -- 2026-11-20).
// Here: the 2D FE section analysis of SectionFE.hpp (warping-function torsion, Pilkey shear
// functions with nu = 0 -- section properties in MIDAS are material independent), on a mesh fine
// enough to be converged (h = min wall thickness / 16, at most ~80k nodes). Being converged, the
// values sit slightly BELOW MIDAS's mesh-dependent ones: model 22 box J 27.85 vs 27.92 (-0.25%),
// Asy 5.422 vs 5.435 (-0.25%), Asz 3.654 vs 3.681 (-0.75%).
struct Psc1CellDims { Real HO1, HO2, HO3, BO1, BO2, BO3, HI1, HI2, HI3, HI4, HI5, BI1, BI3; };

inline ResolvedSectionProperties psc1CellSection(const Psc1CellDims& d) {
  const Real H = d.HO1 + d.HO2 + d.HO3, W = d.BO1 + d.BO2 + d.BO3;
  if (!(d.HO1 > 0) || !(d.HO2 >= 0) || !(d.HO3 > 0) || !(d.BO1 >= 0) || !(d.BO2 >= 0) || !(d.BO3 > 0))
    throw std::invalid_argument("PSC 1CEL: outer dimensions must be HO1, HO3, BO3 > 0 and HO2, BO1, BO2 >= 0");
  if (!(d.HI1 > 0) || !(d.HI2 >= 0) || !(d.HI3 > 0) || !(d.HI4 >= 0) || !(d.HI5 > 0) || !(d.BI1 > 0) || !(d.BI3 > 0))
    throw std::invalid_argument("PSC 1CEL: inner dimensions must be HI1, HI3, HI5, BI1, BI3 > 0 and HI2, HI4 >= 0");
  const Real Hi = d.HI1 + d.HI2 + d.HI3 + d.HI4 + d.HI5;
  if (std::abs(Hi - H) > 1e-9 * H)
    throw std::invalid_argument("PSC 1CEL: inner heights HI1+..+HI5 do not add up to the outer height HO1+HO2+HO3");
  using sections::Contour;
  using sections::Point2;
  Contour outer; // counter-clockwise
  auto push = [](Contour& c, Real y, Real z) { if (c.empty() || (c.back() - Point2(y, z)).norm() > 1e-12) c.emplace_back(y, z); };
  push(outer, d.BO3, -H); push(outer, d.BO3 + d.BO2, -(d.HO1 + d.HO2)); push(outer, W, -d.HO1); push(outer, W, 0);
  push(outer, -W, 0); push(outer, -W, -d.HO1); push(outer, -(d.BO3 + d.BO2), -(d.HO1 + d.HO2)); push(outer, -d.BO3, -H);
  Contour hole; // clockwise (a hole for computePolygonSection)
  const Real zWebTop = -(d.HI1 + d.HI2), zWebBot = -(H - d.HI5 - d.HI4);
  push(hole, 0, -(H - d.HI5)); push(hole, -d.BI3, zWebBot); push(hole, -d.BI1, zWebTop); push(hole, 0, -d.HI1);
  push(hole, d.BI1, zWebTop); push(hole, d.BI3, zWebBot);
  // The cell must lie strictly inside the outer boundary (webs of positive thickness).
  for (const auto& p : hole)
    if (!sections::detail_fe::pointInContour(outer, p) || std::abs(p.x()) >= W)
      throw std::invalid_argument("PSC 1CEL: the inner cell is not strictly inside the outer boundary (check BI1/BI3 against BO2+BO3/BO3)");
  const auto geo = sections::computePolygonSection({outer, hole});
  ResolvedSectionProperties p;
  p.area = geo.area;
  p.IH = geo.Iyy; // about the horizontal centroidal axis (vertical bending), this project's Iy
  p.IB = geo.Izz;
  Real tmin = std::min({d.HO1, d.HI1, d.HI5});
  {
    // Web thickness: horizontal distance between the outer and inner web lines at the inner web's
    // top and bottom ends (the thinnest point of a straight web is at one of its ends). Only sets
    // the mesh size; the wall geometry itself is validated by the inside-test above.
    auto outerY = [&](Real z) { // outer web face, z clamped to [-H, -(HO1+HO2)]
      const Real z0 = -H, z1 = -(d.HO1 + d.HO2);
      z = std::clamp(z, z0, z1);
      return d.BO3 + d.BO2 * (z - z0) / (z1 - z0);
    };
    tmin = std::min({tmin, outerY(zWebTop) - d.BI1, outerY(zWebBot) - d.BI3});
  }
  if (!(tmin > 0)) throw std::invalid_argument("PSC 1CEL: a wall has non-positive thickness");
  const Real h = std::max(tmin / 16, std::sqrt(geo.area / 80000));
  const auto fe = sections::analyzeSectionFE({outer, hole}, h, 0.0);
  p.J = fe.J;
  p.AsH = fe.Asz; // shear along H (vertical)
  p.AsB = fe.Asy; // shear along B (horizontal)
  p.extentH = H;
  p.extentB = 2 * W;
  p.topFiber = -geo.Cz; // top fiber at z = 0
  return p;
}

} // namespace kopruq::io::midas
