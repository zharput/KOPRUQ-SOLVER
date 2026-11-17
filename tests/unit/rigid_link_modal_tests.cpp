// Phase 7H — rigid-link support in ModalSolver/ResponseSpectrumSolver (independent of the
// already-verified `rigid_link_tests.cpp`, which covers LinearStaticSolver only).
//
// Model used throughout: the exact same cantilever-with-rigid-arm geometry as
// `RigidLink.CantileverWithRigidArmMatchesClosedFormAndKinematics` (beam node1-node2, rigid link
// master=node2/slave=node3 at offset e), but node3 carries NO beam of its own -- so it has
// exactly zero rows/columns in the assembled mass matrix M (MatrixAssembler::assembleMass only
// contributes mass through `m.beams`). This gives a clean, model-independent prediction: adding
// this massless, stiffness-less rigid-link slave must change NOTHING about the free-vibration
// problem on the master's own DOFs -- T^T K T and T^T M T reduce to exactly the unmodified
// (no-rigid-link) Kff/Mff, since every row/column belonging to node3 in K and M is identically
// zero before the T-transform is even applied. This is checked directly against a baseline model
// with no rigid link at all (test 1), not merely asserted.
//
// Three independent checks per solver:
//  1. Frequencies/eigenvalues identical to the rigid-link-free baseline (RigidLinkFrequencies...).
//  2. Every mode shape's slave-node DOFs satisfy u_slave = u_master + theta_master x e and
//     theta_slave = theta_master exactly (the same rigid kinematics `RigidLink` test file already
//     verified for RigidLinkReduction::expand() in isolation -- here checked end-to-end through
//     ModalSolver's own eigensolve+expand).
//  3. Mass-normalization/orthonormality (phi_i^T M_full phi_i = 1, cross-orthogonal) holds over
//     the FULL DOF space including the slave's rows, against the independently assembled M.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/solvers/ResponseSpectrumSolver.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::NodalLoad;

namespace {
constexpr double E = 2e8, NU = 0.3, UW = 78.5, A = 0.02, IY = 1e-4, IZ = 2e-4, J = 3e-4, L = 4.0, D = 1.5;

// node1 fixed, node2 = beam tip (master). `withSlave`: also adds node3, rigidly linked to node2
// at offset e=(D,0,0) along the beam axis, carrying no beam/mass of its own.
Model cantileverModel(bool withSlave) {
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L, 0, 0}};
  m.materials = {{1, E, NU, UW}};
  m.sections = {{1, A, IY, IZ, J}};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}};
  if (withSlave) {
    m.nodes.push_back({3, L + D, 0, 0});
    m.rigid_links = {{1, /*master=*/2, /*slave=*/3}};
  }
  return m;
}
std::map<Id, std::array<bool, 6>> fixedNode1() { return {{1, {true, true, true, true, true, true}}}; }

const kopruq::NodeResults& nodeOf(const kopruq::AnalysisResults& r, Id id) {
  for (const auto& n : r.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
}

TEST(RigidLinkModal, FrequenciesMatchBaselineWithoutRigidLink) {
  auto base = kopruq::ModalSolver::solve(cantileverModel(false), fixedNode1(), 6);
  auto withSlave = kopruq::ModalSolver::solve(cantileverModel(true), fixedNode1(), 6);
  ASSERT_TRUE(base.status.success) << base.status.message;
  ASSERT_TRUE(withSlave.status.success) << withSlave.status.message;
  ASSERT_EQ(base.modes.size(), withSlave.modes.size());
  ASSERT_EQ(withSlave.free_dofs, 6u);  // only node2's (master's) DOFs -- node3 eliminated
  for (std::size_t i = 0; i < base.modes.size(); ++i)
    EXPECT_NEAR(withSlave.modes[i].angular_frequency_rad_s, base.modes[i].angular_frequency_rad_s,
                1e-9 * std::max(base.modes[i].angular_frequency_rad_s, 1.0))
        << "mode " << i;
}

TEST(RigidLinkModal, ModeShapesSatisfyRigidKinematicsAtSlave) {
  auto m = cantileverModel(true);
  auto r = kopruq::ModalSolver::solve(m, fixedNode1(), 6);
  ASSERT_TRUE(r.status.success) << r.status.message;
  kopruq::DofManager dm(m);
  auto d2 = dm.node_dofs(2), d3 = dm.node_dofs(3);
  const Eigen::Vector3d e(D, 0, 0);
  for (std::size_t i = 0; i < r.modes.size(); ++i) {
    const auto& phi = r.modes[i].shape;
    Eigen::Vector3d uMaster(phi(d2[0]), phi(d2[1]), phi(d2[2])), thetaMaster(phi(d2[3]), phi(d2[4]), phi(d2[5]));
    Eigen::Vector3d expectedSlaveU = uMaster + thetaMaster.cross(e);
    for (int k = 0; k < 3; ++k) EXPECT_NEAR(phi(d3[k]), expectedSlaveU(k), 1e-9 * std::max(uMaster.norm(), 1e-9)) << "mode " << i << " slave translation";
    for (int k = 0; k < 3; ++k) EXPECT_NEAR(phi(d3[3 + k]), thetaMaster(k), 1e-12) << "mode " << i << " slave rotation == master rotation";
  }
}

TEST(RigidLinkModal, MassOrthonormalityHoldsOverFullDofSpaceIncludingSlave) {
  auto m = cantileverModel(true);
  auto r = kopruq::ModalSolver::solve(m, fixedNode1(), 6);
  ASSERT_TRUE(r.status.success) << r.status.message;
  Eigen::SparseMatrix<kopruq::Real> M = kopruq::MatrixAssembler::assembleMass(m);  // independent of ModalSolver's own use of M
  for (std::size_t i = 0; i < r.modes.size(); ++i) {
    EXPECT_NEAR(r.modes[i].shape.dot(M * r.modes[i].shape), 1.0, 1e-9) << "mode " << i << " mass normalization (full DOF space)";
    for (std::size_t j = i + 1; j < r.modes.size(); ++j)
      EXPECT_NEAR(r.modes[j].shape.dot(M * r.modes[i].shape), 0.0, 1e-7) << "modes " << i << "," << j << " mass-orthogonality";
  }
}

TEST(RigidLinkModal, ErrorHandling) {
  auto m = cantileverModel(true);
  // Slave node (3) cannot carry its own restraints -- already validated for LinearStaticSolver;
  // here checked for ModalSolver's own rigid-link branch.
  auto rsWithSlaveRestrained = fixedNode1();
  rsWithSlaveRestrained[3] = {true, true, true, true, true, true};
  EXPECT_EQ(kopruq::ModalSolver::solve(m, rsWithSlaveRestrained, 6).status.code, kopruq::SolverStatusCode::InvalidModel);

  // No restraints at all -- same diagnostic as the no-rigid-link path.
  EXPECT_EQ(kopruq::ModalSolver::solve(m, {}, 6).status.code, kopruq::SolverStatusCode::NoRestraints);

  // Weightless material -- still a singular (non-positive-definite) free-DOF mass matrix.
  kopruq::Model weightless = m; weightless.materials[0].unit_weight = 0;
  auto s = kopruq::ModalSolver::solve(weightless, fixedNode1(), 6);
  EXPECT_FALSE(s.status.success);
  EXPECT_EQ(s.status.code, kopruq::SolverStatusCode::SingularSystem);
}

// --- ResponseSpectrumSolver: same model, same two independent predictions (iota/total_mass
// unaffected by a massless slave; per-mode recovered displacement at the slave still satisfies
// rigid kinematics relative to the master -- checked BEFORE the final SRSS combination, since
// SRSS is nonlinear and does not preserve a linear kinematic relationship between two already-
// combined quantities; that per-mode consistency is exactly what the production code relies on). ---

TEST(RigidLinkResponseSpectrum, TotalMassMatchesBaselineWithoutRigidLink) {
  kopruq::ResponseSpectrumInput input; input.direction = 2; input.num_modes = 6;
  input.spectrum = {{1e-5, 5.0}, {1.0, 5.0}, {1e4, 5.0}};  // flat spectrum; only total_mass is checked here
  auto base = kopruq::ResponseSpectrumSolver::solve(cantileverModel(false), fixedNode1(), input);
  auto withSlave = kopruq::ResponseSpectrumSolver::solve(cantileverModel(true), fixedNode1(), input);
  ASSERT_TRUE(base.status.success) << base.status.message;
  ASSERT_TRUE(withSlave.status.success) << withSlave.status.message;
  EXPECT_NEAR(withSlave.total_mass, base.total_mass, 1e-9 * std::max(base.total_mass, 1e-9));
}

TEST(RigidLinkResponseSpectrum, PerModeDisplacementSatisfiesRigidKinematicsAtSlave) {
  auto m = cantileverModel(true);
  kopruq::ResponseSpectrumInput input; input.direction = 2; input.num_modes = 6;
  input.spectrum = {{1e-5, 5.0}, {1.0, 5.0}, {1e4, 5.0}};
  auto r = kopruq::ResponseSpectrumSolver::solve(m, fixedNode1(), input);
  ASSERT_TRUE(r.status.success) << r.status.message;
  ASSERT_FALSE(r.modes.empty());
  const Eigen::Vector3d e(D, 0, 0);
  for (std::size_t i = 0; i < r.modes.size(); ++i) {
    const auto& master = nodeOf(r.modes[i].results, 2);
    const auto& slave = nodeOf(r.modes[i].results, 3);
    Eigen::Vector3d uMaster(master.displacement[0], master.displacement[1], master.displacement[2]);
    Eigen::Vector3d thetaMaster(master.displacement[3], master.displacement[4], master.displacement[5]);
    Eigen::Vector3d expectedSlaveU = uMaster + thetaMaster.cross(e);
    for (int k = 0; k < 3; ++k) EXPECT_NEAR(slave.displacement[k], expectedSlaveU(k), 1e-9 * std::max(uMaster.norm(), 1e-9)) << "mode " << i;
    for (int k = 0; k < 3; ++k) EXPECT_NEAR(slave.displacement[3 + k], thetaMaster(k), 1e-12) << "mode " << i;
  }
}

TEST(RigidLinkResponseSpectrum, RejectsSlaveRestraintsConsistentlyWithModal) {
  auto m = cantileverModel(true);
  auto rs = fixedNode1(); rs[3] = {true, false, false, false, false, false};
  kopruq::ResponseSpectrumInput input; input.direction = 2; input.num_modes = 6;
  input.spectrum = {{1e-5, 5.0}, {1.0, 5.0}, {1e4, 5.0}};
  auto r = kopruq::ResponseSpectrumSolver::solve(m, rs, input);
  EXPECT_FALSE(r.status.success);
}
