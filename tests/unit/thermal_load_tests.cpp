// Phase 7T — uniform and gradient temperature load analysis
// (ThermalLoads.hpp). Every expected value is derived independently from
// classical beam/bar theory (free thermal expansion, fixed-end thermal
// force/moment, constant-curvature kinematics, series-bar compatibility),
// never from the production virtual-work derivation itself.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::LoadCase;
using kopruq::MemberThermalLoad;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
const kopruq::NodeResults& nodeOf(const kopruq::StaticSolution& s, Id id) {
  for (const auto& n : s.results.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
kopruq::StaticSolution solveOne(const Model& m, const std::map<Id,std::array<bool,6>>& rs, const LoadCase& lc) {
  return kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
}
}

TEST(ThermalLoads, FreeUniformExpansionMatchesAlphaDeltaTL) {
  constexpr double E = 2e8, A = 0.02, L = 4.0, ALPHA = 1.2e-5, DT = 30.0;
  Model m;
  m.nodes = {{1,0,0,0},{2,L,0,0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, A, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7,1,2,1,1,{0,1,0}}};
  std::map<Id,std::array<bool,6>> rs = {{1,{true,true,true,true,true,true}}};
  LoadCase lc; lc.thermal = {MemberThermalLoad{7, DT, 0,0, 0,0}};
  auto sol = solveOne(m, rs, lc);
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  rel(nodeOf(sol,2).displacement[0], ALPHA*DT*L, "tip displacement == alpha*deltaT*L");
  // force_equilibrium_error is a SCALE-NORMALIZED ratio (balance/scale); for a free thermal
  // expansion the applied-load resultant af is exactly zero by design (ThermalLoads.hpp:
  // a thermal load has no real external resultant), so that ratio is mathematically exactly
  // 1.0 for ANY nonzero reaction, however round-off-small -- it cannot distinguish "truly
  // zero" from "a real violation" in this specific degenerate case. Check the raw (absolute)
  // reaction/balance norm instead, against the problem's own force scale (E*A ~ 4e6).
  EXPECT_LT(sol.results.force_balance_norm, 1e-6);
  EXPECT_LT(sol.results.energy_error, 1e-8);
}

TEST(ThermalLoads, FullyRestrainedUniformGivesEAAlphaDeltaT) {
  constexpr double E = 2e8, A = 0.02, L = 4.0, ALPHA = 1.2e-5, DT = 30.0;
  Model m;
  m.nodes = {{1,0,0,0},{2,L,0,0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, A, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7,1,2,1,1,{0,1,0}}};
  std::map<Id,std::array<bool,6>> rs = {{1,{true,true,true,true,true,true}},{2,{true,true,true,true,true,true}}};
  LoadCase lc; lc.thermal = {MemberThermalLoad{7, DT, 0,0, 0,0}};
  auto sol = solveOne(m, rs, lc);
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  const double expectedN = E*A*ALPHA*DT;
  rel(std::abs(sol.results.elements[0].local_end_force[0]), expectedN, "|N| == E*A*alpha*deltaT");
  EXPECT_LT(sol.results.energy_error, 1e-8);
}

TEST(ThermalLoads, TwoBarSeriesPartialRestraintMatchesHandCompatibility) {
  constexpr double E = 2e8, A1 = 0.02, A2 = 0.01, L1 = 4.0, L2 = 3.0, ALPHA = 1.2e-5, DT = 30.0;
  Model m;
  m.nodes = {{1,0,0,0},{2,L1,0,0},{3,L1+L2,0,0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, A1, 1e-4, 1e-4, 1e-4}, {2, A2, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7,1,2,1,1,{0,1,0}}, {8,2,3,1,2,{0,1,0}}};
  std::map<Id,std::array<bool,6>> rs = {{1,{true,true,true,true,true,true}},{3,{true,true,true,true,true,true}}};
  LoadCase lc; lc.thermal = {MemberThermalLoad{7, DT, 0,0, 0,0}}; // only bar 1 (element 7) heated
  auto sol = solveOne(m, rs, lc);
  ASSERT_TRUE(sol.status.success) << sol.status.message;

  const double EA1 = E*A1, EA2 = E*A2, free1 = ALPHA*DT*L1;
  const double N = -free1 / (L1/EA1 + L2/EA2); // tension-positive
  const double uMid = N*L1/EA1 + free1;
  rel(nodeOf(sol,2).displacement[0], uMid, "mid-node displacement");
  rel(sol.results.elements[0].local_end_force[0], -N, "bar1 axial end force (local convention) matches -N");
  EXPECT_LT(sol.results.energy_error, 1e-8);
}

TEST(ThermalLoads, TwoBarsDifferentAlphaBothHeatedMatchesHandCompatibility) {
  constexpr double E = 2e8, A1 = 0.02, A2 = 0.01, L1 = 4.0, L2 = 3.0;
  constexpr double ALPHA1 = 1.2e-5, DT1 = 30.0, ALPHA2 = 1.0e-5, DT2 = 20.0;
  Model m;
  m.nodes = {{1,0,0,0},{2,L1,0,0},{3,L1+L2,0,0}};
  m.materials = {{1, E, 0.3, 0, ALPHA1}, {2, E, 0.3, 0, ALPHA2}};
  m.sections = {{1, A1, 1e-4, 1e-4, 1e-4}, {2, A2, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7,1,2,1,1,{0,1,0}}, {8,2,3,2,2,{0,1,0}}};
  std::map<Id,std::array<bool,6>> rs = {{1,{true,true,true,true,true,true}},{3,{true,true,true,true,true,true}}};
  LoadCase lc; lc.thermal = {MemberThermalLoad{7, DT1, 0,0, 0,0}, MemberThermalLoad{8, DT2, 0,0, 0,0}};
  auto sol = solveOne(m, rs, lc);
  ASSERT_TRUE(sol.status.success) << sol.status.message;

  const double EA1 = E*A1, EA2 = E*A2, free1 = ALPHA1*DT1*L1, free2 = ALPHA2*DT2*L2;
  const double N = -(free1 + free2) / (L1/EA1 + L2/EA2);
  const double uMid = N*L1/EA1 + free1;
  rel(nodeOf(sol,2).displacement[0], uMid, "mid-node displacement");
  EXPECT_LT(sol.results.energy_error, 1e-8);
}

TEST(ThermalLoads, InclinedMemberFreeExpansionMatchesGlobalDirection) {
  constexpr double E = 2e8, A = 0.02, ALPHA = 1.2e-5, DT = 25.0, L = 13.0;
  Eigen::Vector3d dir(3.0/13, 4.0/13, 12.0/13); // 3-4-12 Pythagorean-like, length 13
  Model m;
  m.nodes = {{1,0,0,0},{2, dir.x()*L, dir.y()*L, dir.z()*L}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, A, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7,1,2,1,1,{0,0,1}}};
  std::map<Id,std::array<bool,6>> rs = {{1,{true,true,true,true,true,true}}};
  LoadCase lc; lc.thermal = {MemberThermalLoad{7, DT, 0,0, 0,0}};
  auto sol = solveOne(m, rs, lc);
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  Eigen::Vector3d expected = ALPHA*DT*L*dir;
  const auto& tip = nodeOf(sol,2);
  for (int i = 0; i < 3; ++i) rel(tip.displacement[i], expected(i), "global tip displacement component");
}

TEST(ThermalLoads, FreeGradientCurvatureMatchesClassicalKinematics) {
  constexpr double E = 2e8, IY = 1e-4, L = 5.0, ALPHA = 1.0e-5, DTZ = 20.0, DEPTH = 1.0;
  Model m;
  m.nodes = {{1,0,0,0},{2,L,0,0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, 0.02, IY, 2e-4, 3e-4}};
  m.beams = {{7,1,2,1,1,{0,1,0}}};
  std::map<Id,std::array<bool,6>> rs = {{1,{true,true,true,true,true,true}}};
  LoadCase lc; lc.thermal = {MemberThermalLoad{7, 0, 0,0, DTZ, DEPTH}};
  auto sol = solveOne(m, rs, lc);
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  const double kappa = ALPHA*DTZ/DEPTH;
  const auto& tip = nodeOf(sol,2);
  rel(std::abs(tip.displacement[4]), kappa*L, "tip rotation magnitude == kappa*L");
  rel(std::abs(tip.displacement[2]), kappa*L*L/2.0, "tip deflection magnitude == kappa*L^2/2");
}

TEST(ThermalLoads, FullyRestrainedGradientGivesEIKappaMoment) {
  constexpr double E = 2e8, IY = 1e-4, L = 5.0, ALPHA = 1.0e-5, DTZ = 20.0, DEPTH = 1.0;
  Model m;
  m.nodes = {{1,0,0,0},{2,L,0,0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, 0.02, IY, 2e-4, 3e-4}};
  m.beams = {{7,1,2,1,1,{0,1,0}}};
  std::map<Id,std::array<bool,6>> rs = {{1,{true,true,true,true,true,true}},{2,{true,true,true,true,true,true}}};
  LoadCase lc; lc.thermal = {MemberThermalLoad{7, 0, 0,0, DTZ, DEPTH}};
  auto sol = solveOne(m, rs, lc);
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  const double expectedM = E*IY*(ALPHA*DTZ/DEPTH);
  rel(std::abs(sol.results.elements[0].local_end_force[4]), expectedM, "|M1| == E*Iy*kappa");
  rel(std::abs(sol.results.elements[0].local_end_force[10]), expectedM, "|M2| == E*Iy*kappa");
  EXPECT_LT(sol.results.energy_error, 1e-8);
}

TEST(ThermalLoads, RejectsGradientWithoutPositiveDepth) {
  constexpr double E = 2e8;
  Model m;
  m.nodes = {{1,0,0,0},{2,4,0,0}};
  m.materials = {{1, E, 0.3, 0, 1.0e-5}};
  m.sections = {{1, 0.02, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7,1,2,1,1,{0,1,0}}};
  std::map<Id,std::array<bool,6>> rs = {{1,{true,true,true,true,true,true}}};
  LoadCase lc; lc.thermal = {MemberThermalLoad{7, 0, 10.0, 0, 0, 0}}; // gradient_y_delta_t set, depth missing
  auto sol = solveOne(m, rs, lc);
  EXPECT_FALSE(sol.status.success);
}

TEST(ThermalLoads, CombinedWithNodalLoadSatisfiesGlobalEquilibrium) {
  constexpr double E = 2e8, A = 0.02, L = 4.0, ALPHA = 1.2e-5, DT = 15.0;
  Model m;
  m.nodes = {{1,0,0,0},{2,L,0,0}};
  m.materials = {{1, E, 0.3, 0, ALPHA}};
  m.sections = {{1, A, 1e-4, 1e-4, 1e-4}};
  m.beams = {{7,1,2,1,1,{0,1,0}}};
  std::map<Id,std::array<bool,6>> rs = {{1,{true,true,true,true,true,true}}};
  LoadCase lc;
  lc.thermal = {MemberThermalLoad{7, DT, 0,0, 10.0, 0.8}};
  lc.nodal = {kopruq::NodalLoad{2, (Eigen::Matrix<double,6,1>() << 5,0,-3,0,0,0).finished()}};
  auto sol = solveOne(m, rs, lc);
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  EXPECT_LT(sol.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.moment_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.energy_error, 1e-8);
}
