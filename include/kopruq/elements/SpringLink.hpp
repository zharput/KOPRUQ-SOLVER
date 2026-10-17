#pragma once
// 3D two-node discrete spring/link element: six independent linear springs,
// one per member-local DOF (translational kx,ky,kz and rotational
// krx,kry,krz), connecting the relative motion of its two nodes. Used for
// bearings, isolators, soil springs and other point-to-point flexible
// connections -- unlike Beam3D, there is no coupling between DOFs (no
// bending theory) and, unlike Truss3D, each of the 6 DOFs has its own
// independent stiffness rather than only an axial one.
//
// Orientation is NOT derived from the node1->node2 direction (as Beam3D's
// is), because the most common use -- a bearing/isolator -- connects two
// COINCIDENT nodes (zero length), for which no direction is defined.
// Instead `local_x` and `reference_y` are given directly, exactly like
// Beam3D's axes() but seeded by an explicit vector instead of node2-node1.
//
// Local stiffness (local DOF order UX,UY,UZ,RX,RY,RZ at node1, same at
// node2, matching Beam3D) is, for each DOF i, the rank-1 2x2 block
// k_i*[[1,-1],[-1,1]] between node1's and node2's i-th local DOF -- the
// same pattern as a truss's axial term (Truss3D.hpp), independently per
// DOF. No mass and no distributed loads are modeled: a spring/link is
// massless and only nodal loads act on its nodes (both already-verified,
// unmodified mechanisms -- MatrixAssembler::assembleMass and
// LinearStaticSolver::assembleLoads).
//
// node1 and node2 MUST be at the same location. This decoupled, per-DOF
// formulation has no lever-arm coupling between a node's translational
// springs and the other node's position; under a rigid-body rotation about
// any point other than the (shared) node location, it is only rigid-body
// consistent (zero force) when the nodes coincide -- if they did not, a
// rigid rotation of the whole model would produce spurious internal force,
// breaking global force/moment equilibrium (caught empirically by this
// element's own equilibrium-residual tests). This is not a general two-
// point rigid link for spatially separated nodes; it models exactly what
// its primary use case needs -- a bearing/isolator/soil spring at a single
// point -- and nothing beyond that is claimed or supported.
inline void checkSpringNodesCoincident(const Eigen::Vector3d& p1, const Eigen::Vector3d& p2) {
  if (!(p1.allFinite() && p2.allFinite()) || (p2 - p1).norm() > 1e-9)
    throw Beam3DError(Beam3DErrorCode::InvalidProperty,
      "spring/link node1 and node2 must be at the same location (see SpringLink.hpp)");
}
#include "../core/Types.hpp"
#include "Beam3D.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace kopruq {

using SpringStiffness = std::array<Real, 6>; // kx,ky,kz [kN/m], krx,kry,krz [kNm/rad], local axes
// local_x: the spring's local x direction, global axes (need not relate to node1/node2 positions).
// reference_y: any vector not parallel to local_x, defining local y exactly as Beam3D::reference_y does.
struct SpringConnection { Id id{}, node1{}, node2{}; Eigen::Vector3d local_x{1,0,0}, reference_y{0,1,0}; SpringStiffness stiffness{}; };

inline bool springStiffnessValid(const SpringStiffness& k) {
  return std::all_of(k.begin(), k.end(), [](Real v) { return std::isfinite(v) && v >= 0; });
}

inline Beam3DAxes springAxes(const Eigen::Vector3d& localX, const Eigen::Vector3d& referenceY) {
  if (!localX.allFinite() || !referenceY.allFinite()) throw Beam3DError(Beam3DErrorCode::NonFinite, "spring local_x/reference_y contains a non-finite value");
  Real nx = localX.norm();
  if (!(nx > 0)) throw Beam3DError(Beam3DErrorCode::InvalidProperty, "spring local_x must be nonzero");
  Eigen::Vector3d x = localX / nx;
  Eigen::Vector3d y = referenceY - referenceY.dot(x) * x;
  if (!(y.norm() > 64 * std::numeric_limits<Real>::epsilon() * referenceY.norm()))
    throw Beam3DError(Beam3DErrorCode::ParallelReference, "spring reference_y is parallel to local_x");
  y.normalize();
  Eigen::Vector3d z = x.cross(y); z.normalize();
  return {x, y, z};
}

using SpringMatrix = Eigen::Matrix<Real,12,12>;

inline SpringMatrix springLocalStiffness(const SpringStiffness& k) {
  if (!springStiffnessValid(k)) throw Beam3DError(Beam3DErrorCode::InvalidProperty, "spring stiffness must be finite and non-negative");
  SpringMatrix m = SpringMatrix::Zero();
  for (int i = 0; i < 6; ++i) { m(i, i) = k[i]; m(i + 6, i + 6) = k[i]; m(i, i + 6) = m(i + 6, i) = -k[i]; }
  return m;
}

inline SpringMatrix springGlobalStiffness(const Eigen::Vector3d& localX, const Eigen::Vector3d& referenceY, const SpringStiffness& k) {
  auto a = springAxes(localX, referenceY);
  auto T = Beam3D::transformation(a);
  return T.transpose() * springLocalStiffness(k) * T;
}

} // namespace kopruq
