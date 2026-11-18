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
  // Shear area approximation: for a shear force along H, only the two webs (thickness tw,
  // parallel to H) are assumed to carry it, using their clear (web) height H-tf; symmetric
  // approximation for a force along B using the two flanges (thickness tf, clear width B-tw).
  // Standard thin-walled "shear carried by the parallel walls only" engineering approximation
  // (e.g. as used for I/box sections generally) -- coarser than the exact rectangle/circle
  // factors above, flagged accordingly; this shape is not exercised by any currently verified
  // model (01-05 all use SB), so it is not yet empirically cross-checked against MIDAS.
  p.AsH = 2 * tw * (H - tf);
  p.AsB = 2 * tf * (B - tw);
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
// the hollow box's Bredt formula above. Shear areas use the standard I/T-beam engineering
// approximation that vertical (H-direction) shear is carried by the web alone (AsH = web area)
// and lateral (B-direction) shear by the flange alone, with the flange's own rectangular shear
// correction factor (5/6) applied to it -- same "coarser web/flange-area approximation" caveat as
// the hollow box's shear areas; not yet empirically cross-checked against MIDAS (USE_SHEAR_DEFORM
// is true for the only real sample seen, so this does affect that model's results).
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
  p.AsH = Aw;
  p.AsB = detail::kRectangularShearFactor * Af;
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
