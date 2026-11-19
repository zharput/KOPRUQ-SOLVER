// Regression: thermal loads inside load COMBINATIONS (LinearStaticSolver::solveCases and its
// rigid-link path). Before 2026-11-19 a combination's combined load case omitted the member
// thermal loads, so its displacements (superposed case solutions) were right but its recovered
// element end forces and support reactions lacked the thermal fixed-end forces (found on MIDAS
// verification model 21: combination axial forces off by exactly E*A*alpha*dT*sum(f)).
// Every expected value below is derived by hand from classical bar/beam theory -- never from the
// production code -- and, independently, each combination result must equal the factored sum of
// the separately solved load-case results (linear superposition).
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Id;
using kopruq::LoadCase;
using kopruq::LoadCombination;
using kopruq::MemberThermalLoad;
using kopruq::Model;

namespace {
using Restraints = std::map<Id, std::array<bool, 6>>;
constexpr std::array<bool, 6> kFixed{true, true, true, true, true, true};

void near(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1.0)) << what;
}
const kopruq::ElementResults& elementOf(const kopruq::StaticSolution& s, Id id) {
  for (const auto& e : s.results.elements) if (e.element_id == id) return e;
  throw std::runtime_error("element not found");
}
const kopruq::NodeResults& nodeOf(const kopruq::StaticSolution& s, Id id) {
  for (const auto& n : s.results.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
// Every element end force and every reaction of combination `k` equals sum(f * case result).
void expectSuperposition(const kopruq::MultiCaseSolution& sol, const std::vector<LoadCombination>& combos) {
  for (std::size_t k = 0; k < combos.size(); ++k) {
    const auto& c = sol.combinations[k];
    for (const auto& e : c.results.elements)
      for (int i = 0; i < 12; ++i) {
        double expected = 0;
        for (const auto& [idx, f] : combos[k].terms) expected += f * elementOf(sol.cases[idx], e.element_id).local_end_force[i];
        near(e.local_end_force[i], expected, "combination element end force == sum(f * case end force)");
      }
    for (const auto& n : c.results.nodes)
      for (int i = 0; i < 6; ++i) {
        double expected = 0;
        for (const auto& [idx, f] : combos[k].terms) expected += f * nodeOf(sol.cases[idx], n.node_id).reaction[i];
        near(n.reaction[i], expected, "combination reaction == sum(f * case reaction)");
      }
  }
}
} // namespace

// Fixed-fixed bar, uniform dT, combination 2.5 * T: no displacement, so N = -E*A*alpha*(2.5*dT)
// (compression) and both supports react with +/- E*A*alpha*(2.5*dT) along the bar.
TEST(ThermalCombination, FixedFixedUniformCombinationAxialAndReactions) {
  constexpr double E = 2e8, A = 0.02, L = 4.0, ALPHA = 1.2e-5, DT = 30.0, F = 2.5;
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L, 0, 0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, A, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}};
  Restraints rs = {{1, kFixed}, {2, kFixed}};
  LoadCase t; t.name = "T"; t.thermal = {MemberThermalLoad{7, DT, 0, 0, 0, 0}};
  std::vector<LoadCombination> combos = {{"C", {{0, F}}}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {t}, combos);
  ASSERT_TRUE(sol.status.success) << sol.status.message;

  const double N = -E * A * ALPHA * F * DT; // tension-positive
  const auto& el = elementOf(sol.combinations[0], 7);
  near(el.local_end_force[0], -N, "end-1 axial force (local convention) == -N");
  near(el.local_end_force[6], N, "end-2 axial force (local convention) == +N");
  // Statics at a support node carrying only this bar: reaction = -(force the element exerts on
  // the node) = force the node exerts on the element = the element's end force. The compressed
  // bar pushes node 1 towards -x, so the support pushes back with +|N| (= -N, N < 0).
  near(nodeOf(sol.combinations[0], 1).reaction[0], -N, "node 1 reaction FX");
  near(nodeOf(sol.combinations[0], 2).reaction[0], N, "node 2 reaction FX");
  near(nodeOf(sol.combinations[0], 1).reaction[0] + nodeOf(sol.combinations[0], 2).reaction[0], 0.0, "reactions self-equilibrated");
  expectSuperposition(sol, combos);
}

// Fixed-fixed beam, linear gradient dTz over depth h, combination 0.6 * G: zero curvature is
// enforced, so a constant restraint moment |M| = E*Iy*alpha*(0.6*dTz)/h with no shear.
TEST(ThermalCombination, FixedFixedGradientCombinationMoment) {
  constexpr double E = 2e8, IY = 1e-4, L = 5.0, ALPHA = 1.0e-5, DTZ = 20.0, H = 1.0, F = 0.6;
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L, 0, 0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, 0.02, IY, 2e-4, 3e-4}};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}};
  Restraints rs = {{1, kFixed}, {2, kFixed}};
  LoadCase g; g.name = "G"; g.thermal = {MemberThermalLoad{7, 0, 0, 0, DTZ, H}};
  std::vector<LoadCombination> combos = {{"C", {{0, F}}}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {g}, combos);
  ASSERT_TRUE(sol.status.success) << sol.status.message;

  const double M = E * IY * ALPHA * F * DTZ / H;
  const auto& el = elementOf(sol.combinations[0], 7);
  near(std::abs(el.local_end_force[4]), M, "|end-1 My| == E*Iy*alpha*f*dT/h");
  near(std::abs(el.local_end_force[10]), M, "|end-2 My| == E*Iy*alpha*f*dT/h");
  near(el.local_end_force[4] + el.local_end_force[10], 0.0, "constant moment: end moments opposite");
  near(el.local_end_force[2], 0.0, "no shear Vz");
  near(std::abs(nodeOf(sol.combinations[0], 1).reaction[4]), M, "|node 1 reaction MY|");
  expectSuperposition(sol, combos);
}

// Two bars in series between fixed ends (node 1 and 3), bar 1 heated (case T), axial point load
// P at the middle node (case P), combination 1.3*T + 0.7*P. Hand compatibility, tension-positive:
//   N1 = EA1*(u2/L1 - alpha*dTe), N2 = -EA2*u2/L2, node-2 equilibrium N1 - N2 = Pe
//   => u2 = (Pe + EA1*alpha*dTe) / (EA1/L1 + EA2/L2), with dTe = 1.3*dT, Pe = 0.7*P.
// Also checks that a combination mixing thermal and mechanical cases keeps both.
TEST(ThermalCombination, SeriesBarsMixedThermalAndMechanicalCombination) {
  constexpr double E = 2e8, A1 = 0.02, A2 = 0.01, L1 = 4.0, L2 = 3.0, ALPHA = 1.2e-5, DT = 30.0, P = 150.0;
  constexpr double FT = 1.3, FP = 0.7;
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L1, 0, 0}, {3, L1 + L2, 0, 0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, A1, 1e-4, 1e-4, 1e-4}, {2, A2, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}, {8, 2, 3, 1, 2, {0, 1, 0}}};
  Restraints rs = {{1, kFixed}, {3, kFixed}};
  LoadCase t; t.name = "T"; t.thermal = {MemberThermalLoad{7, DT, 0, 0, 0, 0}};
  LoadCase p; p.name = "P"; p.nodal = {kopruq::NodalLoad{2, (Eigen::Matrix<double, 6, 1>() << P, 0, 0, 0, 0, 0).finished()}};
  std::vector<LoadCombination> combos = {{"C", {{0, FT}, {1, FP}}}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {t, p}, combos);
  ASSERT_TRUE(sol.status.success) << sol.status.message;

  const double EA1 = E * A1, EA2 = E * A2, dTe = FT * DT, Pe = FP * P;
  const double u2 = (Pe + EA1 * ALPHA * dTe) / (EA1 / L1 + EA2 / L2);
  const double N1 = EA1 * (u2 / L1 - ALPHA * dTe), N2 = -EA2 * u2 / L2;
  const auto& c = sol.combinations[0];
  near(nodeOf(c, 2).displacement[0], u2, "mid-node displacement");
  near(elementOf(c, 7).local_end_force[0], -N1, "bar 1 axial end force == -N1");
  near(elementOf(c, 8).local_end_force[0], -N2, "bar 2 axial end force == -N2");
  near(nodeOf(c, 1).reaction[0], -N1, "node 1 reaction FX == -N1");
  near(nodeOf(c, 3).reaction[0], N2, "node 3 reaction FX == +N2");
  near(nodeOf(c, 1).reaction[0] + nodeOf(c, 3).reaction[0] + Pe, 0.0, "global equilibrium sum FX = 0");
  expectSuperposition(sol, combos);
}

// Same defect on the separate rigid-link code path (solveCasesWithRigidLinks): bar 1-2 heated,
// node 2 rigidly linked (slave) to fully fixed master node 3, so the bar is fixed-fixed and
// N = -E*A*alpha*(f*dT).
TEST(ThermalCombination, RigidLinkPathUniformCombinationAxial) {
  constexpr double E = 2e8, A = 0.02, L = 4.0, ALPHA = 1.2e-5, DT = 30.0, F = 1.7;
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L, 0, 0}, {3, L + 1.0, 0, 0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, A, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}};
  m.rigid_links = {{1, 3, 2}};
  Restraints rs = {{1, kFixed}, {3, kFixed}};
  LoadCase t; t.name = "T"; t.thermal = {MemberThermalLoad{7, DT, 0, 0, 0, 0}};
  std::vector<LoadCombination> combos = {{"C", {{0, F}}}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {t}, combos);
  ASSERT_TRUE(sol.status.success) << sol.status.message;

  const double N = -E * A * ALPHA * F * DT;
  near(elementOf(sol.combinations[0], 7).local_end_force[0], -N, "end-1 axial force == -N");
  near(elementOf(sol.combinations[0], 7).local_end_force[6], N, "end-2 axial force == +N");
  expectSuperposition(sol, combos);
}
