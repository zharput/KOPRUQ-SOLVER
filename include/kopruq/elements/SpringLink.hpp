#pragma once
// 3D two-node discrete spring/link element ("ElasticLink" / MIDAS NX "ELNK" General link): six
// independent linear springs, one per member-local DOF (translational kx,ky,kz and rotational
// krx,kry,krz), connecting the relative motion of its two nodes. Used for bearings, isolators,
// soil springs and other point-to-point flexible connections -- unlike Beam3D, there is no
// coupling between DOFs (no bending theory) and, unlike Truss3D, each of the 6 DOFs has its own
// independent stiffness rather than only an axial one. Deliberately distinct from JointSpring.hpp
// (a single node to ground, MIDAS "NSPR") and from RigidLink (a kinematic master-slave
// constraint, not a flexible connection) -- three separate concepts, not variations of one
// element.
//
// Orientation is NOT derived from the node1->node2 direction (as Beam3D's is): `local_x` and
// `reference_y` are given directly, exactly like Beam3D's axes() but seeded by an explicit vector
// instead of node2-node1. This matters most for the common coincident-node case (bearing/
// isolator, zero length, no direction to derive), but applies equally when node1/node2 are apart.
//
// Local stiffness (local DOF order UX,UY,UZ,RX,RY,RZ at node1, same at node2, matching Beam3D) is,
// for each DOF i, the rank-1 2x2 block k_i*[[1,-1],[-1,1]] between node1's and node2's i-th local
// DOF -- the same pattern as a truss's axial term (Truss3D.hpp), independently per DOF. No mass
// and no distributed loads are modeled: a spring/link is massless and only nodal loads act on its
// nodes (both already-verified, unmodified mechanisms -- MatrixAssembler::assembleMass and
// LinearStaticSolver::assembleLoads).
//
// node1 and node2 need NOT be at the same location (2026-11-18, user decision after MIDAS ELNK
// import needed non-coincident links -- see docs/MIDAS_JSON_DATA_AUDIT.md 4.5). This decoupled,
// per-DOF formulation has no lever-arm coupling between a node's translational springs and the
// other node's position: under a rigid-body rotation of the whole model about any point other
// than a (coincident) spring's own location, a *separated*-node spring produces nonzero spurious
// internal force, so global force/moment equilibrium is only exactly satisfied for coincident
// nodes (caught empirically by this element's own equilibrium-residual tests, e.g.
// spring_tests.cpp's TiltedLoadedSpringSatisfiesEquilibrium, which deliberately stays coincident).
// This is a DELIBERATE, approved simplification, not an oversight: MIDAS NX's own "General" type
// Elastic Link (as opposed to its separate "Rigid" type) is documented nowhere we have access to
// as adding any such lever-arm/rigid-offset coupling either, so this models the same approximation
// MIDAS's own General link accepts for separated nodes, matching rather than exceeding MIDAS's own
// fidelity. A model relying on exact rigid-body consistency for a widely separated elastic link
// should use RigidLink (a true kinematic constraint) instead.
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
