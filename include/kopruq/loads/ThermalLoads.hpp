#pragma once
// Uniform and linear-gradient temperature loads for Beam3D members (Phase
// 7T): converts a member's temperature change into the SAME kind of
// consistent equivalent nodal load vector Loads.hpp already produces for
// distributed forces/moments, so it flows through the existing
// assembleLoadCase/recoverAssembled pipeline (releases, offsets,
// equilibrium/energy checks) completely unchanged.
//
// Three independent effects, derived directly (not assumed) from the same
// cubic-Hermite/linear shape functions already used throughout this
// project (MassMatrix.hpp, Loads.hpp) via the standard "equivalent nodal
// load from an initial strain" virtual-work integral f_i = integral(E *
// (initial strain or curvature) * (shape function or its derivative
// conjugate to dof i)) dx, each over the member's full length (thermal
// effects here are uniform along the member, so the integral is a plain
// closed form, not numerical quadrature):
//
// 1. Uniform temperature change uniform_delta_t: free axial strain
//    epsilon_T = alpha_T * uniform_delta_t. Virtual work through the
//    linear axial shape functions gives
//      f(UX1) = -E*A*epsilon_T,  f(UX2) = +E*A*epsilon_T.
//    Verified (ThermalLoadTests.cpp): a free cantilever elongates by
//    exactly alpha_T*uniform_delta_t*L; a fully restrained member develops
//    |N| = E*A*alpha_T*|uniform_delta_t|.
//
// 2. Gradient across local z, gradient_z_delta_t over gradient_z_depth:
//    induces a uniform thermal curvature kappa_z = alpha_T *
//    gradient_z_delta_t / gradient_z_depth in the LOCAL x-z bending plane
//    (UZ/RY dofs, this project's Iy). Virtual work through the cubic
//    Hermite shape functions' SECOND derivative (curvature is conjugate
//    to an initial-curvature load, not the first-derivative/rotation
//    conjugacy a real distributed MOMENT load uses) gives, respecting the
//    z-plane's established theta_y=-dw/dx sign convention (same flip as
//    MassMatrix.hpp/Loads.hpp):
//      f(RY1) = +E*Iy*kappa_z,  f(RY2) = -E*Iy*kappa_z,  (UZ1,UZ2 dofs: 0)
//
// 3. Gradient across local y, gradient_y_delta_t over gradient_y_depth:
//    the same derivation in the x-y plane (UY/RZ dofs, this project's Iz,
//    no sign flip in that plane's convention):
//      f(RZ1) = -E*Iz*kappa_y,  f(RZ2) = +E*Iz*kappa_y,  (UY1,UY2 dofs: 0)
//
// A temperature load has no physical external force/moment resultant (it
// is an initial-strain/eigenstrain load, not a real applied action), so
// the returned MemberLoadEffect's force/moment_node1 are always zero --
// this keeps the global equilibrium-resultant bookkeeping in
// LinearStaticSolver::assembleLoadCase/recoverAssembled meaningful (it
// tracks genuinely external applied loads vs. reactions); the thermal
// contribution still enters the solve correctly through `equivalent`,
// exactly like any other member load's equivalent nodal load vector.
#include "../core/Types.hpp"
#include "Loads.hpp"
#include <cmath>
#include <stdexcept>

namespace kopruq {
namespace detail {

inline MemberLoadEffect thermalLoadEffect(const MemberThermalLoad& ld, Real alphaT, Real E, Real A, Real Iy, Real Iz) {
  if (!std::isfinite(alphaT) || !std::isfinite(E) || !std::isfinite(A) || !std::isfinite(Iy) || !std::isfinite(Iz))
    throw std::invalid_argument("thermal load requires finite material/section properties");
  if (!std::isfinite(ld.uniform_delta_t) || !std::isfinite(ld.gradient_y_delta_t) || !std::isfinite(ld.gradient_z_delta_t)
      || !std::isfinite(ld.gradient_y_depth) || !std::isfinite(ld.gradient_z_depth))
    throw std::invalid_argument("thermal load values must be finite");
  if (ld.gradient_y_delta_t != 0 && !(ld.gradient_y_depth > 0)) throw std::invalid_argument("gradient_y_depth must be positive when gradient_y_delta_t is nonzero");
  if (ld.gradient_z_delta_t != 0 && !(ld.gradient_z_depth > 0)) throw std::invalid_argument("gradient_z_depth must be positive when gradient_z_delta_t is nonzero");

  MemberLoadEffect e; // force/moment_node1 stay Zero: a thermal load has no real external resultant.
  const Real epsilonT = alphaT * ld.uniform_delta_t;
  e.equivalent(0) -= E * A * epsilonT;
  e.equivalent(6) += E * A * epsilonT;

  if (ld.gradient_y_delta_t != 0) {
    const Real kappaY = alphaT * ld.gradient_y_delta_t / ld.gradient_y_depth;
    e.equivalent(5) -= E * Iz * kappaY;
    e.equivalent(11) += E * Iz * kappaY;
  }
  if (ld.gradient_z_delta_t != 0) {
    const Real kappaZ = alphaT * ld.gradient_z_delta_t / ld.gradient_z_depth;
    e.equivalent(4) += E * Iy * kappaZ;
    e.equivalent(10) -= E * Iy * kappaZ;
  }
  return e;
}

} // namespace detail
} // namespace kopruq
