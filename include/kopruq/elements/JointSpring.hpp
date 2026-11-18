#pragma once
// Single-node elastic support ("joint spring" / MIDAS NX "Nodal Spring", NSPR): six independent
// linear springs, one per local DOF, connecting one model node to a fixed, infinitely rigid
// ground -- the single-node counterpart to SpringLink.hpp's two-node ElasticLink. Used for
// foundation/soil springs at a single point, as distinct from a two-node ElasticLink (bearing/
// isolator between two nodes) and from RigidLink (master-slave kinematic constraint between two
// nodes) -- three deliberately separate concepts, not variations of one element.
//
// Orientation (local_x, reference_y) works exactly as SpringLink's does: given directly, not
// derived from node geometry (there is no "other node" to derive a direction from). Local
// stiffness is a plain 6x6 diagonal (k per local DOF, no coupling between DOFs -- same
// independent-per-DOF convention as SpringLink); global stiffness/force are obtained by the same
// 3x3 rotation R (from springAxes) applied to both the translational and rotational halves,
// matching SpringLink's/Beam3D's own transformation convention.
#include "SpringLink.hpp"

namespace kopruq {

// local_x/reference_y: see SpringLink.hpp's SpringConnection -- identical meaning, single node.
struct JointSpringConnection { Id id{}, node{}; Eigen::Vector3d local_x{1,0,0}, reference_y{0,1,0}; SpringStiffness stiffness{}; };

using JointSpringMatrix = Eigen::Matrix<Real,6,6>;

inline JointSpringMatrix jointSpringLocalStiffness(const SpringStiffness& k) {
  if (!springStiffnessValid(k)) throw Beam3DError(Beam3DErrorCode::InvalidProperty, "joint spring stiffness must be finite and non-negative");
  JointSpringMatrix m = JointSpringMatrix::Zero();
  for (int i = 0; i < 6; ++i) m(i, i) = k[i];
  return m;
}

inline Eigen::Matrix3d jointSpringRotation(const Eigen::Vector3d& localX, const Eigen::Vector3d& referenceY) {
  auto a = springAxes(localX, referenceY);
  Eigen::Matrix3d R; R.row(0) = a.x.transpose(); R.row(1) = a.y.transpose(); R.row(2) = a.z.transpose();
  return R;
}

inline JointSpringMatrix jointSpringTransformation(const Eigen::Vector3d& localX, const Eigen::Vector3d& referenceY) {
  JointSpringMatrix T = JointSpringMatrix::Zero();
  Eigen::Matrix3d R = jointSpringRotation(localX, referenceY);
  T.block<3,3>(0, 0) = R; T.block<3,3>(3, 3) = R;
  return T;
}

inline JointSpringMatrix jointSpringGlobalStiffness(const Eigen::Vector3d& localX, const Eigen::Vector3d& referenceY, const SpringStiffness& k) {
  auto T = jointSpringTransformation(localX, referenceY);
  return T.transpose() * jointSpringLocalStiffness(k) * T;
}

} // namespace kopruq
