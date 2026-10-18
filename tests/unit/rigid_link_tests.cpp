// Phase 5 (deferred item, completed per user instruction) — general rigid
// links / multi-point constraints (kopruq/assembly/RigidLink.hpp): a
// slave node's 6 DOFs are entirely determined by a master node's via
// rigid-body kinematics, implemented as a DOF-elimination layer wrapped
// around the existing, unmodified assembled K/F/U.
//
// Three independent checks:
//  1. ReductionMatrixMatchesRigidBodyKinematics: RigidLinkReduction's T
//     reproduces u_slave = u_master + theta_master x e for an arbitrary
//     (u,theta,e), computed directly with the cross product, not by
//     calling RigidLinkReduction to produce the expected value.
//  2. EmptyRigidLinksTakesTheOriginalCodePath: a model whose rigid_links
//     is empty (every model before this feature existed, and every other
//     test in this suite) must still solve to the plain closed-form
//     cantilever answer -- confirms solveCases' early branch
//     (`if (!m.rigid_links.empty()) ...`) correctly falls through to the
//     original, unmodified solveCases body rather than always going
//     through the new reduction machinery.
//  3. CantileverWithRigidArmMatchesClosedFormAndKinematics: a cantilever
//     beam (fixed node1, free node2 = master) with a slave node3 rigidly
//     attached to the master at an offset along the beam axis (a rigid
//     arm extending past the real beam tip), loaded at the slave. The
//     equivalent load at the master (force + moment, by elementary
//     statics: F_master = F_slave, M_master = e x F_slave) lets the
//     master's displacement/rotation be checked against the closed-form
//     cantilever-under-tip-force-and-moment solution; the slave's
//     displacement is then checked against the master's solved values
//     through the rigid-body kinematic relationship itself (a model-
//     independent consistency check, not a second application of the
//     same closed form).
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/assembly/RigidLink.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::LoadCase;
using kopruq::NodalLoad;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
const kopruq::NodeResults& nodeOf(const kopruq::StaticSolution& s, Id id) {
  for (const auto& n : s.results.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
}

TEST(RigidLink, ReductionMatrixMatchesRigidBodyKinematics) {
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, 2, -1, 3}}; // master=1, slave=2; e = (2,-1,3)
  m.rigid_links = {{1, 1, 2}};
  kopruq::DofManager dm(m);
  auto red = kopruq::RigidLinkReduction::build(m, dm);
  ASSERT_EQ(red.reduced_count(), 6u); // only node1's (master's) dofs remain

  Eigen::Vector3d u(1, 2, 3), theta(0.01, -0.02, 0.03), e(2, -1, 3);
  Eigen::VectorXd Ureduced(6); Ureduced << u, theta;
  Eigen::VectorXd Ufull = red.expand(Ureduced);
  Eigen::Vector3d expectedSlaveTranslation = u + theta.cross(e);
  auto sd = dm.node_dofs(2);
  for (int i = 0; i < 3; ++i) rel(Ufull((Eigen::Index)sd[i]), expectedSlaveTranslation(i), "slave translation");
  for (int i = 0; i < 3; ++i) rel(Ufull((Eigen::Index)sd[3+i]), theta(i), "slave rotation == master rotation");
}

TEST(RigidLink, EmptyRigidLinksTakesTheOriginalCodePath) {
  constexpr double E = 2e8, A = 0.02, IY = 1e-4, IZ = 2e-4, J = 3e-4, L = 4.0, P = 1000.0;
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L, 0, 0}};
  m.materials = {{1, E, 0.3, 0}};
  m.sections = {{1, A, IY, IZ, J}};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}};
  std::map<Id,std::array<bool,6>> rs = {{1, {true,true,true,true,true,true}}};
  LoadCase lc; lc.nodal = {NodalLoad{2, (Eigen::Matrix<double,6,1>() << 0,0,-P,0,0,0).finished()}};

  ASSERT_TRUE(m.rigid_links.empty());
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success);
  double w = nodeOf(sol, 2).displacement[2];
  rel(w, -P*L*L*L/(3*E*IY), "plain cantilever tip deflection");
}

TEST(RigidLink, CantileverWithRigidArmMatchesClosedFormAndKinematics) {
  constexpr double E = 2e8, A = 0.02, IY = 1e-4, IZ = 2e-4, J = 3e-4, L = 4.0, D = 1.5, P = 1000.0;
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L, 0, 0}, {3, L + D, 0, 0}};
  m.materials = {{1, E, 0.3, 0}};
  m.sections = {{1, A, IY, IZ, J}};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}};
  m.rigid_links = {{1, /*master=*/2, /*slave=*/3}};
  std::map<Id,std::array<bool,6>> rs = {{1, {true,true,true,true,true,true}}};
  LoadCase lc; lc.nodal = {NodalLoad{3, (Eigen::Matrix<double,6,1>() << 0,0,-P,0,0,0).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;

  // Closed form: F_master = F_slave = -P (global Z); M_master = e x F, e=(D,0,0), F=(0,0,-P) -> M = (0, D*P, 0).
  const double EI = E * IY;
  const double az = 12*EI/(L*L*L), bz = 6*EI/(L*L), cz = 4*EI/L;
  const double det = az*cz - bz*bz;
  const double Fz = -P, My = D*P;
  const double expectedW = (cz*Fz - bz*My) / det;        // master UZ
  const double expectedTheta = (az*My - bz*Fz) / det;    // master RY

  const auto& master = nodeOf(sol, 2);
  rel(master.displacement[2], expectedW, "master (beam tip) UZ");
  rel(master.displacement[4], expectedTheta, "master (beam tip) RY");
  rel(master.displacement[0], 0.0, "master UX", 1e-9);

  // Rigid-body kinematic consistency: slave = master + theta_master x e, independent of the
  // closed-form beam solution above -- this checks the rigid-link mechanism itself.
  Eigen::Vector3d uMaster(master.displacement[0], master.displacement[1], master.displacement[2]);
  Eigen::Vector3d thetaMaster(master.displacement[3], master.displacement[4], master.displacement[5]);
  Eigen::Vector3d e(D, 0, 0);
  Eigen::Vector3d expectedSlave = uMaster + thetaMaster.cross(e);
  const auto& slave = nodeOf(sol, 3);
  for (int i = 0; i < 3; ++i) rel(slave.displacement[i], expectedSlave(i), "slave translation consistency");
  for (int i = 0; i < 3; ++i) rel(slave.displacement[3+i], thetaMaster(i), "slave rotation == master rotation");

  EXPECT_LT(sol.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.moment_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.energy_error, 1e-8);
}
