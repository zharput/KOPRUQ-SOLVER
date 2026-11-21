// 2026-11-21 -- consistent member-load vectors for Timoshenko members (Loads.hpp: interdependent
// interpolation shape functions, Phi = 12 E I / (G As L^2)). Dedicated new file.
//
// Expected values are closed-form Timoshenko beam solutions derived by hand (not from the code
// under test). Bending in the local x-y plane (Iz, Asy; v = UY, theta = RZ), member along +X with
// reference_y = +Y, E = 3e7, nu = 0.2 (G = 1.25e7), Iz = 0.5, Asy = 0.4, L = 5 -> Phi = 1.44.
//  1. Cantilever under a uniform distributed moment m about z: statically determinate, shear
//     V = 0 everywhere, M(x) = m (L - x). Hence theta_tip = m L^2 / (2 E I) and, since v' = theta
//     when V = 0, v_tip = m L^3 / (3 E I) exactly -- no shear term. (An Euler-Bernoulli load vector
//     on the Timoshenko element adds a spurious m L / (G As).)
//  2. Cantilever under a point force P (local y) at x = a: v(a) = P a^3/(3EI) + P a/(G As),
//     theta(a) = P a^2/(2EI); the unloaded remainder rotates rigidly, so
//     v_tip = v(a) + theta(a) (L - a), theta_tip = theta(a).
//  3. Fixed-fixed beam (two elements, free middle node) under a uniform distributed moment m:
//     V constant, M(x) = M0 + (V - m) x; theta(0) = theta(L) = 0 and v(0) = v(L) = 0 with
//     v' = theta + V/(G As) give V = m/(1 + Phi_L), M0 = m L Phi_L / (2 (1 + Phi_L)),
//     Phi_L = 12 E I/(G As L^2) of the whole span (verification model 23 situation).
//  4. Invariants: a full-length uniform transverse load has the same vector for any Phi
//     (qL/2, qL^2/12); at Phi = 0 the vector equals the cubic-Hermite one.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::LoadCase;

namespace {
constexpr double E = 3e7, NU = 0.2, G = E / (2 * (1 + NU)), A = 1.0, IY = 0.3, IZ = 0.5, J = 0.4, ASY = 0.4, ASZ = 0.6;
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
const kopruq::NodeResults& nodeOf(const kopruq::StaticSolution& s, Id id) {
  for (const auto& n : s.results.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
Model beamModel(std::vector<double> xs) {
  Model m;
  for (std::size_t i = 0; i < xs.size(); ++i) m.nodes.push_back({static_cast<Id>(i + 1), xs[i], 0, 0});
  m.materials = {{1, E, NU, 0}};
  kopruq::Section s{1, A, IY, IZ, J}; s.Asy = ASY; s.Asz = ASZ;
  m.sections = {s};
  for (std::size_t i = 0; i + 1 < xs.size(); ++i) m.beams.push_back({static_cast<Id>(i + 1), static_cast<Id>(i + 1), static_cast<Id>(i + 2), 1, 1, {0, 1, 0}});
  return m;
}
const std::array<bool,6> kFixed{true, true, true, true, true, true};
kopruq::MemberLoad uniformMomentZ(Id element, double m) {
  kopruq::MemberLoad l; l.element = element; l.axes = kopruq::LoadAxes::Local; l.direction = 2;
  l.kind = kopruq::MemberLoadKind::Moment; l.w1 = l.w2 = m; return l;
}
}

TEST(TimoshenkoMemberLoad, CantileverUniformMomentHasNoShearDeflection) {
  const double L = 5.0, m = 100.0, EI = E * IZ;
  ASSERT_NEAR(12 * EI / (G * ASY * L * L), 1.44, 1e-12); // Phi of this test
  auto model = beamModel({0, L});
  LoadCase lc; lc.member = {uniformMomentZ(1, m)};
  auto sol = kopruq::LinearStaticSolver::solveCases(model, {{1, kFixed}}, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  rel(nodeOf(sol, 2).displacement[5], m * L * L / (2 * EI), "tip RZ = m L^2 / 2EI");
  rel(nodeOf(sol, 2).displacement[1], m * L * L * L / (3 * EI), "tip UY = m L^3 / 3EI (no shear term)");
  rel(nodeOf(sol, 1).reaction[5], -m * L, "base MZ");
  EXPECT_NEAR(nodeOf(sol, 1).reaction[1], 0.0, 1e-8);
  EXPECT_LT(sol.results.energy_error, 1e-9);
}

TEST(TimoshenkoMemberLoad, CantileverOffCentrePointLoad) {
  const double L = 5.0, a = 1.5, P = 200.0, EI = E * IZ, GA = G * ASY;
  auto model = beamModel({0, L});
  kopruq::MemberPointLoad pl; pl.element = 1; pl.axes = kopruq::LoadAxes::Local; pl.direction = 1;
  pl.kind = kopruq::MemberLoadKind::Force; pl.value = P; pl.position = a / L;
  LoadCase lc; lc.point = {pl};
  auto sol = kopruq::LinearStaticSolver::solveCases(model, {{1, kFixed}}, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  const double va = P * a * a * a / (3 * EI) + P * a / GA, tha = P * a * a / (2 * EI);
  rel(nodeOf(sol, 2).displacement[1], va + tha * (L - a), "tip UY");
  rel(nodeOf(sol, 2).displacement[5], tha, "tip RZ");
  // The tip end forces vanish (free end): end force = k u - f must be zero there.
  for (const auto& e : sol.results.elements)
    for (int i = 6; i < 12; ++i) EXPECT_NEAR(e.local_end_force[(std::size_t)i], 0.0, 1e-7) << "free-end force " << i;
}

TEST(TimoshenkoMemberLoad, FixedFixedUniformMomentMatchesTimoshenkoFixedEndForces) {
  const double L = 8.0, m = 100.0, EI = E * IZ;
  const double phiL = 12 * EI / (G * ASY * L * L);
  auto model = beamModel({0, L / 2, L});
  LoadCase lc; lc.member = {uniformMomentZ(1, m), uniformMomentZ(2, m)};
  auto sol = kopruq::LinearStaticSolver::solveCases(model, {{1, kFixed}, {3, kFixed}}, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  const double V = m / (1 + phiL), M0 = m * L * phiL / (2 * (1 + phiL));
  const auto& r1 = nodeOf(sol, 1); const auto& r3 = nodeOf(sol, 3);
  rel(std::abs(r1.reaction[1]), V, "|reaction FY| = m/(1+Phi)");
  rel(std::abs(r3.reaction[1]), V, "|reaction FY| at the other end");
  rel(std::abs(r1.reaction[5]), M0, "|reaction MZ| = m L Phi / 2(1+Phi)");
  rel(std::abs(r3.reaction[5]), M0, "|reaction MZ| at the other end");
  EXPECT_LT(sol.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.moment_equilibrium_error, 1e-9);
}

TEST(TimoshenkoMemberLoad, InvariantsOfTheLoadVector) {
  const double L = 4.0, q = -10.0;
  const Eigen::Matrix3d R = Eigen::Matrix3d::Identity();
  kopruq::MemberLoad full; full.axes = kopruq::LoadAxes::Local; full.direction = 1; full.w1 = full.w2 = q;
  const auto eb = kopruq::detail::memberLoadEffect(full, R, L, 0.0, 0.0);
  const auto ti = kopruq::detail::memberLoadEffect(full, R, L, 0.9, 0.0);
  for (int i = 0; i < 12; ++i) EXPECT_NEAR(ti.equivalent(i), eb.equivalent(i), 1e-12) << "full uniform load, dof " << i;
  rel(eb.equivalent(1), q * L / 2, "qL/2"); rel(eb.equivalent(5), q * L * L / 12, "qL^2/12");
  // Phi = 0: point force at s = 0.3 equals the cubic Hermite values (independent formulas).
  kopruq::MemberPointLoad p; p.axes = kopruq::LoadAxes::Local; p.direction = 1; p.value = 1.0; p.position = 0.3;
  const auto h = kopruq::detail::pointLoadEffect(p, R, L, 0.0, 0.0);
  const double s = 0.3;
  rel(h.equivalent(1), 1 - 3 * s * s + 2 * s * s * s, "H1");
  rel(h.equivalent(5), L * (s - 2 * s * s + s * s * s), "H2");
  rel(h.equivalent(7), 3 * s * s - 2 * s * s * s, "H3");
  rel(h.equivalent(11), L * (-s * s + s * s * s), "H4");
  // The resultant is independent of Phi (force sum = P, moment about node 1 = P x).
  const auto t = kopruq::detail::pointLoadEffect(p, R, L, 0.7, 0.0);
  rel(t.equivalent(1) + t.equivalent(7), 1.0, "force sum");
  rel(t.equivalent(5) + t.equivalent(11) + L * t.equivalent(7), s * L, "moment about node 1");
}
