// Phase 5 — truss elements (Truss3D.hpp): a pin-ended 3D truss member
// implemented as a Beam3D with every rotational DOF released at both ends.
//
// Two independent checks, neither calling the production stiffness/release
// code to produce its expected values:
//  1. TrussStiffnessMatchesClassicalFormula: the assembled global 6x6
//     (translational-DOF) stiffness of one truss member equals the textbook
//     3D truss stiffness (EA/L)*[[c c^T,-c c^T],[-c c^T,c c^T]], c the unit
//     direction vector, built here from plain coordinates only.
//  2. ABraceTrussMatchesHandAnalysis: a classic statically-determinate
//     two-bar A-frame (apex loaded, two pinned bases) solved end to end
//     through LinearStaticSolver, compared against values derived by hand
//     from method-of-joints statics and the unit-load (virtual work)
//     theorem for the displacement -- exact rational numbers, not computed
//     by calling the solver.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include <map>
#include "kopruq/elements/Truss3D.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::LoadCase;
using kopruq::NodalLoad;

namespace {

void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}

const kopruq::NodeResults& nodeOf(const kopruq::AnalysisResults& r, Id id) {
  for (const auto& n : r.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}

} // namespace

TEST(Truss, StiffnessMatchesClassicalFormula) {
  // Arbitrary oblique member, 3-4-5-like direction so the reference is exact.
  Eigen::Vector3d p1(1, 2, 0), p2(1 + 3, 2 + 4, 0 + 12); // length 13 (3-4-12 Pythagorean-like: 9+16+144=169=13^2)
  constexpr double E = 2e8, A = 5e-3, L = 13.0;
  Model m;
  m.nodes = {{1, p1.x(), p1.y(), p1.z()}, {2, p2.x(), p2.y(), p2.z()}};
  m.materials = {{1, E, 0.3, 0}};
  m.sections = {{1, A, 1e-6, 1e-6, 1e-6}};
  m.beams = {kopruq::makeTrussConnection(1, 1, 2, 1, 1, p1, p2)};

  auto K = kopruq::MatrixAssembler::assemble(m); // 12x12 dense check (6 nodes * ... here 2 nodes -> 12 dofs)
  Eigen::MatrixXd Kd = Eigen::MatrixXd(K);

  Eigen::Vector3d c = (p2 - p1) / L; // independent of Beam3D::axes
  double k = E * A / L;
  Eigen::Matrix3d cc = c * c.transpose();
  // Global dof layout: node1 = 0..5 (UX,UY,UZ,RX,RY,RZ), node2 = 6..11.
  Eigen::MatrixXd expected = Eigen::MatrixXd::Zero(12, 12);
  expected.block<3,3>(0,0) = k * cc;
  expected.block<3,3>(0,6) = -k * cc;
  expected.block<3,3>(6,0) = -k * cc;
  expected.block<3,3>(6,6) = k * cc;

  for (int i = 0; i < 12; ++i)
    for (int j = 0; j < 12; ++j)
      rel(Kd(i, j), expected(i, j), "K(i,j)", 1e-10);
}

TEST(Truss, ABraceMatchesHandAnalysis) {
  constexpr double E = 2e8, A = 1e-3;     // kN/m^2, m^2
  constexpr double P = 100.0;             // kN, downward at apex
  // Node 1, 2: bases at (-4,0,0) and (4,0,0); node 3: apex at (0,0,3). Bar length = 5 (3-4-5).
  Model m;
  m.nodes = {{1, -4, 0, 0}, {2, 4, 0, 0}, {3, 0, 0, 3}};
  m.materials = {{1, E, 0.3, 0}};
  m.sections = {{1, A, 1e-6, 1e-6, 1e-6}};
  m.beams = {
    kopruq::makeTrussConnection(1, 1, 3, 1, 1, {-4,0,0}, {0,0,3}),
    kopruq::makeTrussConnection(2, 2, 3, 1, 1, {4,0,0}, {0,0,3}),
  };
  // Pure-truss model: no element supplies rotational stiffness anywhere, so every
  // node's RX/RY/RZ must be restrained (otherwise Kff is singular on those rows).
  std::map<Id, std::array<bool,6>> rs = {
    {1, {true,true,true,true,true,true}},
    {2, {true,true,true,true,true,true}},
    {3, {false,false,false,true,true,true}},
  };
  LoadCase lc; lc.nodal = {NodalLoad{3, (Eigen::Matrix<double,6,1>() << 0,0,-P,0,0,0).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;

  // Hand analysis (method of joints + unit-load/virtual-work theorem):
  //   N (axial force, each bar, tension +) = -250/3 kN (compression)
  //   apex vertical displacement = 1/288 m (downward)
  //   base reactions: Rx = +-200/3 kN, Rz = 50 kN each (upward)
  const double N = -250.0 / 3.0;
  const double Uz = -1.0 / 288.0;

  const auto& apex = nodeOf(sol.results, 3);
  rel(apex.displacement[0], 0.0, "apex Ux", 1e-9);
  rel(apex.displacement[1], 0.0, "apex Uy", 1e-9);
  rel(apex.displacement[2], Uz, "apex Uz");

  const auto& base1 = nodeOf(sol.results, 1);
  const auto& base2 = nodeOf(sol.results, 2);
  rel(base1.reaction[0], 200.0/3.0, "base1 Rx");
  rel(base1.reaction[2], 50.0, "base1 Rz");
  rel(base2.reaction[0], -200.0/3.0, "base2 Rx");
  rel(base2.reaction[2], 50.0, "base2 Rz");

  // Axial force recovered from the element's own end-displacement elongation,
  // independent of whatever local_end_force sign convention the recovery uses:
  // elongation = apex displacement projected on the base->apex unit vector.
  Eigen::Vector3d dApex(apex.displacement[0], apex.displacement[1], apex.displacement[2]);
  Eigen::Vector3d dir1 = Eigen::Vector3d(4, 0, 3) / 5.0; // base1 -> apex
  double elong1 = dApex.dot(dir1);
  double Nfrom1 = E * A / 5.0 * elong1;
  rel(Nfrom1, N, "bar1 axial force from elongation");
  Eigen::Vector3d dir2 = Eigen::Vector3d(-4, 0, 3) / 5.0; // base2 -> apex
  double elong2 = dApex.dot(dir2);
  double Nfrom2 = E * A / 5.0 * elong2;
  rel(Nfrom2, N, "bar2 axial force from elongation");

  EXPECT_LT(sol.results.force_equilibrium_error, 1e-10);
  EXPECT_LT(sol.results.energy_error, 1e-9);
}
