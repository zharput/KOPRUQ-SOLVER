// Phase 5 (2026-11-18) — single-node elastic support to ground (JointSpring.hpp; MIDAS NX "NSPR"
// Nodal Spring), the single-node counterpart of SpringLink.hpp's two-node ElasticLink. Three
// independent checks:
//  1. AxisAlignedStiffnessMatchesDenseFormula: local_x along global X, reference_y along global Y
//     (R = Identity, no transform needed) so the assembled global stiffness's 6x6 block at the
//     node can be compared directly, entry by entry, to a plain diag(k) built from nothing but
//     the given stiffness values.
//  2. SimpleAxialSpringMatchesHandAnalysis: one node, free only along its local x (other 5 DOFs
//     explicitly restrained), loaded along x: closed-form u = F/kx, independent of the production
//     code.
//  3. TiltedLoadedSpringSatisfiesEquilibrium: one node, NO explicit restraints at all (the joint
//     spring alone, with all six positive stiffnesses, makes the assembled K positive definite),
//     arbitrarily oriented, mixed stiffness, general nodal load. This is also the real test of
//     LinearStaticSolver.hpp's "fold an unrestrained joint-spring's force into the model-level
//     force/moment balance" logic: without it, force_equilibrium_error here would be large (the
//     applied load would appear unbalanced, since nothing else absorbs it) rather than near zero.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/elements/JointSpring.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::LoadCase;
using kopruq::NodalLoad;
using kopruq::JointSpringConnection;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
}

TEST(JointSpring, AxisAlignedStiffnessMatchesDenseFormula) {
  kopruq::SpringStiffness k{100, 200, 300, 10, 20, 30};
  Model m;
  m.nodes = {{1, 0, 0, 0}};
  m.joint_springs = {JointSpringConnection{1, 1, {1,0,0}, {0,1,0}, k}};
  auto Kd = Eigen::MatrixXd(kopruq::MatrixAssembler::assemble(m));
  ASSERT_EQ(Kd.rows(), 6); ASSERT_EQ(Kd.cols(), 6);
  for (int i = 0; i < 6; ++i)
    for (int j = 0; j < 6; ++j)
      rel(Kd(i,j), i == j ? k[i] : 0.0, "K(i,j)", 1e-12);
}

TEST(JointSpring, SimpleAxialSpringMatchesHandAnalysis) {
  constexpr double KX = 1234.5, F = 10.0;
  Model m;
  m.nodes = {{1, 0, 0, 0}};
  kopruq::SpringStiffness k{KX, 0, 0, 0, 0, 0};
  m.joint_springs = {JointSpringConnection{1, 1, {1,0,0}, {0,1,0}, k}};
  std::map<Id,std::array<bool,6>> rs = {{1, {false,true,true,true,true,true}}};
  LoadCase lc; lc.nodal = {NodalLoad{1, (Eigen::Matrix<double,6,1>() << F,0,0,0,0,0).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  double ux = 0; for (const auto& n : sol.results.nodes) if (n.node_id == 1) ux = n.displacement[0];
  rel(ux, F / KX, "displacement");
  ASSERT_EQ(sol.results.joint_springs.size(), 1u);
  rel(sol.results.joint_springs[0].global_end_force[0], F, "joint spring force == applied load (equilibrium at the one free DOF)");
}

TEST(JointSpring, TiltedLoadedSpringSatisfiesEquilibrium) {
  Model m;
  m.nodes = {{1, 5, 2, -1}};
  kopruq::SpringStiffness k{800, 300, 150, 40, 25, 60};
  m.joint_springs = {JointSpringConnection{1, 1, {1, 0.3, -0.5}, {0, 1, 0.2}, k}};
  // No DOF is actually restrained (all false) -- the joint spring alone must make K positive
  // definite. A literally empty restraints map is rejected by the solver as a generic
  // "no restraints" guard unrelated to joint springs, so an explicit all-false entry is used
  // instead to exercise that guard correctly while keeping every DOF free.
  std::map<Id,std::array<bool,6>> rs = {{1, {false,false,false,false,false,false}}};
  LoadCase lc; lc.nodal = {NodalLoad{1, (Eigen::Matrix<double,6,1>() << 5,-3,7,1,-2,0.5).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  EXPECT_LT(sol.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.moment_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.energy_error, 1e-9);
  ASSERT_EQ(sol.results.joint_springs.size(), 1u);
}
