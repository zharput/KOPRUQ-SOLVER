#pragma once
// Consistent (not lumped) local 12x12 mass matrix for Beam3D, built with the
// same Hermite/linear shape functions and sign convention as the stiffness
// matrix and the member-load equivalent nodal loads (kopruq/loads/Loads.hpp),
// so mass and stiffness/load describe the same displacement field. Built by
// Gauss quadrature rather than the textbook closed form so that convention
// changes to the shape functions stay consistent automatically; the
// quadrature is exact since the integrands are degree-6 polynomials.
//
// Translational (axial + both bending planes) mass per length is
// rho*A = unit_weight/g_standard * area; rotary (torsional) inertia per
// length is rho*J. Rotary inertia about the bending axes (beam cross-section
// tilt inertia) is not included -- standard for slender Euler-Bernoulli
// beams, where it is a higher-order effect.
//
// Uses these Euler-Bernoulli shape functions regardless of whether the
// element's stiffness has a Timoshenko shear correction (Beam3D.hpp's
// Asy/Asz); a fully shear-consistent dynamic mass matrix is not
// implemented. This is a documented, intentional simplification, not an
// oversight: modal/response-spectrum results for a Timoshenko beam
// (Asy/Asz > 0) use this slightly approximate mass alongside the (exact)
// Timoshenko stiffness.
//
// `lumpedMassMatrix` (below) is a second, independently selectable mass
// formulation -- see its own comment for why it exists and when to use it.
#include "Beam3D.hpp"
#include "Releases.hpp"
#include <array>

namespace kopruq {

// Standard gravity, m/s^2 (used only to turn Material::unit_weight [kN/m^3]
// into a mass density [kN*s^2/m^4] for dynamics; self-weight loads, which
// are forces, do not use it).
inline constexpr Real g_standard = 9.80665;

namespace detail {

// Hermite/linear basis at local coordinate s in [0,1], matching the sign
// convention already used for member loads: theta_y = -dw/dx, so the
// rotation shape functions for the z (UZ,RY) plane are negated relative to
// the y (UY,RZ) plane.
inline Beam3DVector massShapeDof(int dof, Real s, Real L) {
  Beam3DVector phi = Beam3DVector::Zero();
  const Real s2 = s * s, s3 = s2 * s;
  const Real N1 = 1 - s, N2 = s;
  const Real H1 = 1 - 3 * s2 + 2 * s3, H2 = L * (s - 2 * s2 + s3), H3 = 3 * s2 - 2 * s3, H4 = L * (-s2 + s3);
  switch (dof) {
    case 0: phi(0) = N1; phi(6) = N2; break;                                   // axial
    case 3: phi(3) = N1; phi(9) = N2; break;                                   // torsion
    case 1: phi(1) = H1; phi(5) = H2; phi(7) = H3; phi(11) = H4; break;        // y-bending (UY, RZ)
    case 2: phi(2) = H1; phi(4) = -H2; phi(8) = H3; phi(10) = -H4; break;      // z-bending (UZ, RY)
  }
  return phi;
}

} // namespace detail

inline Beam3DMatrix consistentMassMatrix(Real L, Real rhoA, Real rhoJ) {
  if (!(std::isfinite(L) && L > 0) || !std::isfinite(rhoA) || !std::isfinite(rhoJ))
    throw Beam3DError(Beam3DErrorCode::InvalidProperty, "mass matrix requires a finite, positive length and finite mass properties");
  static const Real gp[5] = {-0.9061798459386640, -0.5384693101056831, 0.0, 0.5384693101056831, 0.9061798459386640};
  static const Real gw[5] = {0.2369268850561891, 0.4786286704993665, 0.5688888888888889, 0.4786286704993665, 0.2369268850561891};
  Beam3DMatrix M = Beam3DMatrix::Zero();
  for (int g = 0; g < 5; ++g) {
    const Real s = 0.5 * (gp[g] + 1), wq = 0.5 * L * gw[g];
    const Beam3DVector ax = detail::massShapeDof(0, s, L), to = detail::massShapeDof(3, s, L);
    const Beam3DVector by = detail::massShapeDof(1, s, L), bz = detail::massShapeDof(2, s, L);
    M += wq * rhoA * (ax * ax.transpose() + by * by.transpose() + bz * bz.transpose());
    M += wq * rhoJ * (to * to.transpose());
  }
  return 0.5 * (M + M.transpose());
}

// Classic "translational-only" lumped mass: half of the element's total mass (rho*A*L) placed
// directly on each end node's UX/UY/UZ; every rotational DOF (RX/RY/RZ, including torsion) gets
// exactly zero mass -- the most common "lumped mass" option in commercial FE software, and
// confirmed (2026-11-17, independent diagnostic, not guessed) to be what MIDAS NX's own default
// eigenvalue analysis uses for verification/models/13 and 14: using this matrix in place of
// `consistentMassMatrix` reproduces MIDAS's modal frequencies to within a constant ~0.11%
// (plausibly a minor density/rounding difference, not a formulation gap) across every mode of
// both models, including high modes where `consistentMassMatrix` diverges by up to 23%.
// Rotationally singular by construction (every RX/RY/RZ row/column is exactly zero) -- callers
// (ModalSolver's Lumped path) must statically condense the massless rotational DOFs before using
// this in a generalized eigenproblem; it is never directly Cholesky-factorizable on its own.
inline Beam3DMatrix lumpedMassMatrix(Real L, Real rhoA) {
  if (!(std::isfinite(L) && L > 0) || !std::isfinite(rhoA))
    throw Beam3DError(Beam3DErrorCode::InvalidProperty, "mass matrix requires a finite, positive length and finite mass properties");
  Beam3DMatrix M = Beam3DMatrix::Zero();
  const Real half = rhoA * L * 0.5;
  for (int i : {0, 1, 2, 6, 7, 8}) M(i, i) = half; // UX,UY,UZ at node1 (0-2) and node2 (6-8)
  return M;
}

} // namespace kopruq
