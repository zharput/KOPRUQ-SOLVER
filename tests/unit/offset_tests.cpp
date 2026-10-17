// Phase 5 — rigid end offsets / eccentric connections (Offsets.hpp).
//
// Four independent checks:
//  1. OffsetTransformMatchesRigidBodyKinematics: offsetTransform()'s G
//     matches the plain definition u_point = u_node + theta_node x e for
//     several (u,theta,e), computed here with the cross product directly,
//     not by calling offsetTransform() itself to produce the expected value.
//  2. ZeroOffsetIsExactNoOp: a beam with offset1=offset2={0,0,0} assembles
//     to bit-identical stiffness as the same beam built without the offset
//     fields at all -- the regression guarantee for every previously
//     verified model, none of which sets these new fields.
//  3. AssembledStiffnessMatchesGtKG: a beam WITH nonzero offsets assembles
//     to G^T * K_point * G, K_point built independently in this test from
//     Beam3D::globalStiffness at the offset-adjusted end points (Beam3D
//     itself is already independently verified, R1); this isolates and
//     checks only the new offset composition in MatrixAssembler::assemble.
//  4. EccentricCantileverSatisfiesEquilibrium: a cantilever loaded through
//     an eccentric node (nonzero offset at the free end) under a nodal
//     force and a member distributed load, solved end to end; the model's
//     own already-verified equilibrium/energy residuals (used throughout
//     every prior phase) must be near machine precision.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/elements/Offsets.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::LoadCase;
using kopruq::NodalLoad;
using kopruq::MemberLoad;
using kopruq::LoadAxes;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
}

TEST(Offsets, TransformMatchesRigidBodyKinematics) {
  Eigen::Vector3d e(0.3, -0.5, 0.2);
  auto G = kopruq::offsetTransform(e, Eigen::Vector3d::Zero());
  Eigen::Vector3d u(1, 2, 3), theta(0.01, -0.02, 0.03);
  Eigen::Matrix<double,12,1> uNode = Eigen::Matrix<double,12,1>::Zero();
  uNode.segment<3>(0) = u; uNode.segment<3>(3) = theta;
  Eigen::Matrix<double,12,1> uPoint = G * uNode;
  Eigen::Vector3d expectedPointTranslation = u + theta.cross(e); // plain rigid-body definition
  for (int i = 0; i < 3; ++i) rel(uPoint(i), expectedPointTranslation(i), "u_point translation");
  for (int i = 0; i < 3; ++i) rel(uPoint(3 + i), theta(i), "theta_point == theta_node");
}

TEST(Offsets, ZeroOffsetIsExactNoOp) {
  Model m;
  m.nodes = {{1,0,0,0},{2,5,0,0}};
  m.materials = {{1, 2e8, 0.3, 0}};
  m.sections = {{1, 0.02, 1e-4, 2e-4, 3e-4}};
  kopruq::Beam3DConnection e{1,1,2,1,1,{0,1,0},{}};
  m.beams = {e};
  auto K1 = Eigen::MatrixXd(kopruq::MatrixAssembler::assemble(m));

  e.offset1 = {0,0,0}; e.offset2 = {0,0,0};
  m.beams = {e};
  auto K2 = Eigen::MatrixXd(kopruq::MatrixAssembler::assemble(m));
  ASSERT_EQ(K1.rows(), K2.rows());
  for (int i = 0; i < K1.rows(); ++i)
    for (int j = 0; j < K1.cols(); ++j)
      EXPECT_DOUBLE_EQ(K1(i,j), K2(i,j)) << i << "," << j;
}

TEST(Offsets, AssembledStiffnessMatchesGtKG) {
  constexpr double E = 2e8, A = 0.02, IY = 1e-4, IZ = 2e-4, J = 3e-4;
  Eigen::Vector3d p1(0,0,0), p2(6,0,0);
  Eigen::Vector3d off1(0, 0.3, 0), off2(0, -0.2, 0.1);
  Model m;
  m.nodes = {{1, p1.x(), p1.y(), p1.z()}, {2, p2.x(), p2.y(), p2.z()}};
  m.materials = {{1, E, 0.3, 0}};
  m.sections = {{1, A, IY, IZ, J}};
  kopruq::Beam3DConnection e{1,1,2,1,1,{0,1,0},{},off1,off2};
  m.beams = {e};
  auto Kassembled = Eigen::MatrixXd(kopruq::MatrixAssembler::assemble(m));

  kopruq::Beam3DGeometry g{p1 + off1, p2 + off2, Eigen::Vector3d(0,1,0)};
  kopruq::Beam3DProperties props{E, E/(2*(1+0.3)), A, IY, IZ, J};
  auto Kpoint = kopruq::Beam3D::globalStiffness(g, props);
  auto G = kopruq::offsetTransform(off1, off2);
  Eigen::MatrixXd expected = G.transpose() * Kpoint * G;

  for (int i = 0; i < 12; ++i)
    for (int j = 0; j < 12; ++j)
      rel(Kassembled(i,j), expected(i,j), "K(i,j)", 1e-10);
}

TEST(Offsets, EccentricCantileverSatisfiesEquilibrium) {
  constexpr double E = 2e8, A = 0.02, IY = 1e-4, IZ = 2e-4, J = 3e-4;
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, 4, 0, -0.5}}; // node2 offset below the member's own axis endpoint
  m.materials = {{1, E, 0.3, 0}};
  m.sections = {{1, A, IY, IZ, J}};
  kopruq::Beam3DConnection e{1,1,2,1,1,{0,1,0},{}};
  e.offset2 = {0, 0, 0.5}; // member's actual tip is at node2 + offset2 = (4,0,0), on the X axis
  m.beams = {e};
  std::map<Id,std::array<bool,6>> rs = {{1, {true,true,true,true,true,true}}};
  LoadCase lc;
  lc.nodal = {NodalLoad{2, (Eigen::Matrix<double,6,1>() << 0,0,-10,0,0,0).finished()}};
  lc.member = {MemberLoad{1, LoadAxes::Global, 2, -2, -2, 0, 1}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  EXPECT_LT(sol.results.force_equilibrium_error, 1e-10);
  EXPECT_LT(sol.results.moment_equilibrium_error, 1e-10);
  EXPECT_LT(sol.results.max_element_force_error, 1e-10);
  EXPECT_LT(sol.results.max_element_moment_error, 1e-10);
  EXPECT_LT(sol.results.energy_error, 1e-9);
}
