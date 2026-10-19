#pragma once
// Closed-form parametric section properties (Phase 7S), general-purpose
// (not MIDAS-specific). Solid rectangle, hollow rectangle/box and solid
// circle already exist as kopruq::io::midas::solidRectangle/hollowBox/
// solidCircle (built for Phase 7B); migrating/renaming those into this
// namespace is a follow-up, not duplicated here. This file adds the one
// closed-form shape not yet covered: a hollow circle (annulus), exact
// formulas (no approximation, unlike the box's Bredt torsion constant).
//
// Per the user's explicit instruction, other families named in the
// Phase 7S scope (I/H/T/channel/angle, precast girder, and any further
// shapes) are deliberately NOT implemented here and require individual
// approval before being added -- "do not implement every section family
// without first reviewing existing code and obtaining phase approval."
#include "../core/Types.hpp"
#include <cmath>
#include <stdexcept>

namespace kopruq::sections {

inline constexpr Real kPi = 3.14159265358979323846;

struct ParametricSectionProperties { Real area{}, Iyy{}, Izz{}, J{}; };

// Do: outer diameter, Di: inner diameter (Di < Do). Iyy == Izz for an annulus (fully symmetric);
// J = Iyy + Izz = 2*Iyy, exact (same identity as a solid circle, Section.hpp's convention Iy=Iz here).
inline ParametricSectionProperties hollowCircle(Real Do, Real Di) {
  if (!(Do > 0) || !(Di >= 0) || !(Di < Do)) throw std::invalid_argument("hollow circle requires 0 <= Di < Do");
  ParametricSectionProperties p;
  p.area = kPi / 4 * (Do*Do - Di*Di);
  p.Iyy = p.Izz = kPi / 64 * (Do*Do*Do*Do - Di*Di*Di*Di);
  p.J = 2 * p.Iyy;
  return p;
}

} // namespace kopruq::sections
