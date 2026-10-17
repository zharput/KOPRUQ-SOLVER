#pragma once
// 3D pin-ended truss member, implemented as a Beam3D specialization rather
// than a separate stiffness/mass/load/recovery code path.
//
// A beam with all rotational DOFs released at both ends (RX, RY, RZ at
// node1 and node2) has, after static condensation (Releases.hpp), a local
// stiffness matrix whose only nonzero entries are the axial terms
// (+-EA/L); every bending/shear/torsion term vanishes exactly, because with
// both end moments zero the member carries no transverse end force
// (V = (M1+M2)/L = 0). Transformed to global axes this is bit-for-bit the
// classical 3D truss stiffness (EA/L)*[[c c^T, -c c^T],[-c c^T, c c^T]]
// with c the member's unit direction vector -- independent of the choice
// of reference_y, since the retained (translational) stiffness never
// depends on the member's rotated local y/z axes once all rotations are
// released. This file only picks a valid reference_y (any vector not
// parallel to the member) and the release pattern; Beam3D/Releases/
// MatrixAssembler/LinearStaticSolver carry the already-verified numerics
// unchanged.
//
// Mass for a truss member follows the same Guyan (static) condensation of
// the beam's consistent mass matrix used for every other released-end beam
// (MassMatrix.hpp, Releases.hpp) rather than the textbook isotropic rod
// mass matrix rho*A*L/6*[[2I,I],[I,2I]]; this is a documented, intentional
// consequence of reusing the existing, already-verified release
// methodology instead of adding a second, independently-tested mass
// formulation for one element type.
#include "../core/Model.hpp"
#include "Beam3D.hpp"
#include "Releases.hpp"
#include <cmath>

namespace kopruq {

// Release pattern that reduces a Beam3D to a pin-ended 3D truss member.
// RY, RZ (bending) are released at both ends, which is what drops every
// transverse/bending stiffness term to zero (see file header). RX
// (torsion) is released at node2 only, not both ends: releasing it at
// either end alone already drives the retained torsional stiffness to
// exactly zero too (the beam's torsion block is rank-1, k*[[1,-1],[-1,1]],
// so condensing out one of its two DOFs always zeroes the other), but
// releasing it at BOTH ends creates a genuine, separate zero-energy
// mechanism -- free relative spin about the member's own axis, decoupled
// from everything else -- which Releases.hpp's stability check correctly
// rejects as unstable. The asymmetric choice gets the same zero torsional
// stiffness without tripping that (correct) safety check. Either way, a
// node connected only by trusses still has no rotational stiffness source
// and must have its RX/RY/RZ restrained, exactly as documented below.
inline Releases trussReleases() {
  Releases r{};
  for (int i : {4, 5, 9, 10, 11}) r[i] = true;
  return r;
}

// Any unit vector not parallel to `direction`, for use as reference_y on a
// truss member where the actual local y/z orientation is immaterial (every
// bending/torsion term is released away). Picks global Z unless the member
// is itself nearly parallel to Z, in which case it picks global Y.
inline Eigen::Vector3d trussReference(const Eigen::Vector3d& direction) {
  Eigen::Vector3d d = direction.normalized();
  Eigen::Vector3d z(0, 0, 1);
  if (std::abs(d.dot(z)) > 1 - 1e-6) return Eigen::Vector3d(0, 1, 0);
  return z;
}

// Convenience constructor for direct C++ callers (mirrors the JSON reader's
// "trusses" array): a Beam3DConnection with the truss release pattern and
// an automatically chosen reference_y.
inline Beam3DConnection makeTrussConnection(Id id, Id node1, Id node2, Id material, Id section, const Eigen::Vector3d& node1Coords, const Eigen::Vector3d& node2Coords) {
  Beam3DConnection e;
  e.id = id; e.node1 = node1; e.node2 = node2; e.material = material; e.section = section;
  e.reference_y = trussReference(node2Coords - node1Coords);
  e.releases = trussReleases();
  return e;
}

} // namespace kopruq
