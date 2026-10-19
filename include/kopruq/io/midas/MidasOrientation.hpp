#pragma once
// Converts a MIDAS Civil NX beam element's local axis definition (element
// direction + Beta angle "ANGLE") into KOPRUQ-SOLVER's `reference_y`
// vector (Beam3D.hpp: x = direction, y = reference_y projected and
// normalized, z = x cross y).
//
// Source: MIDAS Civil's own Analysis/Civil manual text (quoted via search,
// support.midasuser.com "Element"/"Change Element Parameters" articles --
// WebFetch to midasuser.com returns HTTP 403, an authenticated portal, so
// this is transcribed from a search-engine summary of the manual's own
// wording, not independently re-derived engineering theory; flag for
// empirical cross-check in Phase 7E once comparable results exist):
//   "the element x-axis runs from node N1 toward node N2."
//   "If the ECS x-axis is not parallel with the GCS Z-axis, the Beta angle
//    is defined as the [angle] to the ECS x-z plane from the GCS Z-axis."
//     -> at Beta=0, local z is the projection of global Z onto the plane
//        perpendicular to local x.
//   "If the ECS x-axis ... is parallel with the GCS Z-axis, the Beta angle
//    is ... the angle formed from the GCS X-axis to the ECS z-axis."
//     -> for a vertical member, at Beta=0, local z aligns with global X.
//   Beta then rotates the (y,z) pair about local x (standard right-handed
//   rotation).
//
// This file only produces the vector to feed into the EXISTING, unmodified
// Beam3D::axes()/transformation() as `reference_y`; it does not change
// any element formulation.
#include "../../core/Types.hpp"
#include <Eigen/Dense>
#include <cmath>
#include <stdexcept>

namespace kopruq::io::midas {

// localX: unit vector from node1 to node2 (NOT validated here; caller's responsibility,
// matching Beam3D::axes()'s own zero-length/non-finite checks downstream).
// betaDegrees: MIDAS "ANGLE", degrees.
inline Eigen::Vector3d midasReferenceY(const Eigen::Vector3d& localX, Real betaDegrees) {
  if (!localX.allFinite() || !std::isfinite(betaDegrees))
    throw std::invalid_argument("midasReferenceY: non-finite input");
  const Eigen::Vector3d x = localX.normalized();
  const Eigen::Vector3d globalZ(0, 0, 1), globalX(1, 0, 0);
  const bool vertical = std::abs(x.dot(globalZ)) > 1 - 1e-9; // x parallel to global Z
  const Eigen::Vector3d seed = vertical ? globalX : globalZ;
  Eigen::Vector3d z0 = seed - seed.dot(x) * x;
  const Real z0norm = z0.norm();
  if (!(z0norm > 1e-9)) throw std::invalid_argument("midasReferenceY: degenerate orientation (reference seed parallel to member axis)");
  z0 /= z0norm;
  const Eigen::Vector3d y0 = z0.cross(x); // so that x.cross(y0) == z0 (right-handed, matches Beam3D::axes)
  const Real beta = betaDegrees * Real(3.14159265358979323846) / 180;
  return std::cos(beta) * y0 - std::sin(beta) * z0;
}

} // namespace kopruq::io::midas
