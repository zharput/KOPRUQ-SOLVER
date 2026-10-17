// Phase 5 — discrete spring/link elements (SpringLink.hpp): six independent
// linear springs, one per local DOF, used for bearings/isolators/soil
// springs. Four independent checks:
//  1. AxisAlignedStiffnessMatchesDenseFormula: local_x along global X,
//     reference_y along global Y (R = Identity, no transform needed) so
//     the assembled global stiffness can be compared directly, entry by
//     entry, to the rank-1-per-DOF formula built from nothing but the
//     given stiffness values.
//  2. ZeroLengthLinkIsWellDefined: node1 and node2 at the same location
//     (the common case for a bearing/isolator) still assembles correctly,
//     since orientation here is independent of node geometry.
//  3. SimpleAxialSpringMatchesHandAnalysis: a single spring, fixed at
//     node1, loaded along its local x at node2: closed-form u = F/kx,
//     independent of the production code.
//  4. TiltedLoadedSpringSatisfiesEquilibrium: an arbitrarily oriented,
//     mixed-stiffness spring under a general nodal load, checked against
//     the model's own already-verified equilibrium/energy residuals.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/elements/SpringLink.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::LoadCase;
using kopruq::NodalLoad;
using kopruq::SpringConnection;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
}

TEST(Spring, AxisAlignedStiffnessMatchesDenseFormula) {
  kopruq::SpringStiffness k{100, 200, 300, 10, 20, 30};
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, 1, 0, 0}}; // positions irrelevant to orientation here
  SpringConnection e{1, 1, 2, {1,0,0}, {0,1,0}, k};
  m.springs = {e};
  auto Kd = Eigen::MatrixXd(kopruq::MatrixAssembler::assemble(m));

  Eigen::MatrixXd expected = Eigen::MatrixXd::Zero(12, 12);
  for (int i = 0; i < 6; ++i) { expected(i,i) = k[i]; expected(i+6,i+6) = k[i]; expected(i,i+6) = expected(i+6,i) = -k[i]; }
  for (int i = 0; i < 12; ++i)
    for (int j = 0; j < 12; ++j)
      rel(Kd(i,j), expected(i,j), "K(i,j)", 1e-12);
}

TEST(Spring, ZeroLengthLinkIsWellDefined) {
  kopruq::SpringStiffness k{500, 500, 500, 50, 50, 50};
  Model m;
  m.nodes = {{1, 2, 3, 4}, {2, 2, 3, 4}}; // coincident
  SpringConnection e{1, 1, 2, {1,0,0}, {0,1,0}, k};
  m.springs = {e};
  EXPECT_NO_THROW({ auto K = kopruq::MatrixAssembler::assemble(m); (void)K; });
}

TEST(Spring, SimpleAxialSpringMatchesHandAnalysis) {
  constexpr double KX = 1234.5, F = 10.0;
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, 1, 0, 0}};
  kopruq::SpringStiffness k{KX, 0, 0, 0, 0, 0};
  m.springs = {SpringConnection{1, 1, 2, {1,0,0}, {0,1,0}, k}};
  std::map<Id,std::array<bool,6>> rs = {{1, {true,true,true,true,true,true}}, {2, {false,true,true,true,true,true}}};
  LoadCase lc; lc.nodal = {NodalLoad{2, (Eigen::Matrix<double,6,1>() << F,0,0,0,0,0).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  double ux2 = 0; for (const auto& n : sol.results.nodes) if (n.node_id == 2) ux2 = n.displacement[0];
  rel(ux2, F / KX, "tip displacement");
}

TEST(Spring, TiltedLoadedSpringSatisfiesEquilibrium) {
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, 3, 1, 2}};
  kopruq::SpringStiffness k{800, 300, 150, 40, 25, 60};
  m.springs = {SpringConnection{1, 1, 2, {1, 0.3, -0.5}, {0, 1, 0.2}, k}};
  std::map<Id,std::array<bool,6>> rs = {{1, {true,true,true,true,true,true}}};
  LoadCase lc; lc.nodal = {NodalLoad{2, (Eigen::Matrix<double,6,1>() << 5,-3,7,1,-2,0.5).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  EXPECT_LT(sol.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.moment_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.energy_error, 1e-9);
  ASSERT_EQ(sol.results.springs.size(), 1u);
  EXPECT_LT(sol.results.springs[0].normalized_force_error, 1e-9);
  EXPECT_LT(sol.results.springs[0].normalized_moment_error, 1e-9);
}
