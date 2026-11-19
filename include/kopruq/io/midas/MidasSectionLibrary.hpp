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
#include <algorithm>
#include <cmath>
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
struct ResolvedSectionProperties { Real area{}, IH{}, IB{}, J{}, AsH{}, AsB{}; };

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
// its centroidal line. J (open thin-walled section, flange + web as two rectangular strips) uses
// the classical J = (1/3)*sum(b_i*t_i^3) approximation (e.g. Roark's Formulas for Stress and
// Strain) -- an open-section approximation, not exact, same sourcing standard and same caveat as
// the hollow box's Bredt formula above. Shear areas use the rigorous Zhuravskii/Timoshenko
// quadrature (detail::timoshenkoShearArea, 2026-11-19), not the coarser "web alone"/"flange
// alone" engineering approximation this replaced -- still not empirically cross-checked against
// MIDAS's own effective shear area (inaccessible), but now the best available theoretical result
// rather than a simplified shortcut.
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
  p.J = (B * tf * tf * tf + hw * tw * tw * tw) / 3;
  // AsH: horizontal strips along H (top to bottom) -- flange (full width B) then web (width tw).
  // AsB: the section rotated 90 degrees -- outside the web's width only the flange is present
  // (depth tf), within it both flange and web are present (depth tf+hw = H).
  p.AsH = detail::timoshenkoShearArea({{0, tf, B}, {tf, H, tw}}, p.IH);
  p.AsB = detail::timoshenkoShearArea({{0, (B - tw) / 2, tf}, {(B - tw) / 2, (B + tw) / 2, H}, {(B + tw) / 2, B, tf}}, p.IB);
  return p;
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
  throw std::invalid_argument("unsupported DBUSER section shape '" + shape + "' (only SB, B, SR, T are implemented)");
}

} // namespace kopruq::io::midas
