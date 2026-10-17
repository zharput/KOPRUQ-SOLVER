#pragma once
// Rigid end offsets (eccentric connections / "insertion point"): a member's
// physical end point is displaced from its node by a rigid arm, a global-axis
// vector `offset` from the node to the member's actual end point.
//
// The member's own geometry, local stiffness/mass and member-load equivalent
// nodal loads are computed at the offset end points exactly as for a member
// with no offset. A rigid-body kinematic transform G then maps offset-point
// DOFs to the node's own DOFs:
//   u_point = u_node + theta_node x e,   theta_point = theta_node
// (e = offset, the vector from node to point; rotations are shared since the
// arm is rigid). By the usual virtual-work argument this gives
//   K_node = G^T K_point G,   M_node = G^T M_point G,   f_node = G^T f_point,
// applied once per element end (G is block-diagonal over the two ends).
// Zero offsets give G = Identity: no behaviour change for any model that
// does not use this field, preserving every previously verified result.
#include "../core/Types.hpp"
#include <Eigen/Dense>

namespace kopruq {

using OffsetMatrix = Eigen::Matrix<Real,12,12>;

inline Eigen::Matrix3d skew(const Eigen::Vector3d& e) {
  Eigen::Matrix3d s;
  s <<      0, -e.z(),  e.y(),
         e.z(),      0, -e.x(),
        -e.y(),  e.x(),      0;
  return s;
}

// u_point = G * u_node, for one element with offsets `offset1` (at node1) and
// `offset2` (at node2), each the vector from the node to the member's actual
// end point, in global axes.
inline OffsetMatrix offsetTransform(const Eigen::Vector3d& offset1, const Eigen::Vector3d& offset2) {
  OffsetMatrix G = OffsetMatrix::Identity();
  G.block<3,3>(0,3) = -skew(offset1);
  G.block<3,3>(6,9) = -skew(offset2);
  return G;
}

} // namespace kopruq
