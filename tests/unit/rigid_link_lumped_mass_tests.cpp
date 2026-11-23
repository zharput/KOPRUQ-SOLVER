// 2026-11-23 -- lumped mass together with rigid links (ModalSolver::solveWithRigidLinks with
// MassMatrixType::Lumped, T^T M T reduction) and the response-spectrum influence vector
// iota = T iota_r (slave masses included). Dedicated new file. Verification model 100.
//
// Hand-derived reference (no production code used for the expected values): vertical cantilever
// column node1 (0,0,0, fixed) -> node2 (0,0,L), Euler-Bernoulli, Iy = Iz = I. Node2 is the master
// of slaves node3 (0,0,L+h) and node4 (0,0,L+h+g), all six DOFs. A block beam node3 -> node4 carries
// mass only (both ends slaved to one master: rigid, no strain energy). Lumped translational masses
// (rho A L / 2 per end, rho = unit_weight / 9.80665): m2 = rho_c A_c L / 2, m3 = m4 = rho_b A_b g / 2.
// Bending in X-Z (u = UX2, t = RY2, slave u_s = u + t e_s with e3 = h, e4 = h + g):
//   M = [[S0, S1], [S1, S2]], S0 = m2 + m3 + m4, S1 = m3 e3 + m4 e4, S2 = m3 e3^2 + m4 e4^2
//   K = E I [[12/L^3, -6/L^2], [-6/L^2, 4/L]]
//   det(K - lambda M) = 0 -> two frequencies; the Y-Z plane gives the same pair (Iy = Iz).
// Axial: lambda = E A_c / (L S0). Torsion: no rotational inertia (masses on the axis) -> massless,
// condensed. Free-DOF modes with mass: 5.
// Spectrum (direction X): total mass = S0 (m2 + both slave masses); with all 5 modes the effective
// masses sum to S0 (mass participation 1).
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <vector>
#include "kopruq/solvers/ResponseSpectrumSolver.hpp"

using kopruq::Id;
using kopruq::Model;

namespace {
constexpr double E = 3e7, NU = 0.2, UW = 25.0, GRAV = 9.80665;
constexpr double L = 5.0, H = 1.0, G = 2.0, AC = 1.0, I = 0.1, JC = 0.15, AB = 2.0;
Model columnWithRigidBlock() {
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, 0, 0, L}, {3, 0, 0, L + H}, {4, 0, 0, L + H + G}};
  m.materials = {{1, E, NU, UW}};
  m.sections = {{1, AC, I, I, JC}, {2, AB, 0.5, 0.5, 0.6}};
  m.beams = {{1, 1, 2, 1, 1, {0, 1, 0}}, {2, 3, 4, 1, 2, {0, 1, 0}}};
  m.rigid_links = {{1, 2, 3}, {2, 2, 4}};
  return m;
}
std::map<Id, std::array<bool,6>> fixedBase() { return {{1, {true, true, true, true, true, true}}}; }
struct Masses { double m2, m3, m4, S0, S1, S2; };
Masses masses() {
  Masses s;
  s.m2 = UW / GRAV * AC * L / 2; s.m3 = s.m4 = UW / GRAV * AB * G / 2;
  const double e3 = H, e4 = H + G;
  s.S0 = s.m2 + s.m3 + s.m4; s.S1 = s.m3 * e3 + s.m4 * e4; s.S2 = s.m3 * e3 * e3 + s.m4 * e4 * e4;
  return s;
}
std::vector<double> expectedOmegas() {
  const auto s = masses();
  const double K11 = 12 * E * I / (L * L * L), K12 = -6 * E * I / (L * L), K22 = 4 * E * I / L;
  const double a = s.S0 * s.S2 - s.S1 * s.S1;
  const double b = -(K11 * s.S2 + K22 * s.S0 - 2 * K12 * s.S1);
  const double c = K11 * K22 - K12 * K12;
  const double disc = std::sqrt(b * b - 4 * a * c);
  const double l1 = (-b - disc) / (2 * a), l2 = (-b + disc) / (2 * a), lax = E * AC / (L * s.S0);
  std::vector<double> w = {std::sqrt(l1), std::sqrt(l1), std::sqrt(l2), std::sqrt(l2), std::sqrt(lax)};
  std::sort(w.begin(), w.end());
  return w;
}
}

TEST(RigidLinkLumpedMass, FrequenciesMatchHandDerivedTwoDofSolution) {
  auto r = kopruq::ModalSolver::solve(columnWithRigidBlock(), fixedBase(), 0, kopruq::MassMatrixType::Lumped);
  ASSERT_TRUE(r.status.success) << r.status.message;
  const auto w = expectedOmegas();
  ASSERT_EQ(r.modes.size(), w.size()); // 5 massy free DOFs (RZ of the master is massless)
  for (std::size_t i = 0; i < w.size(); ++i)
    EXPECT_NEAR(r.modes[i].angular_frequency_rad_s, w[i], 1e-9 * w[i]) << "mode " << i + 1;
}

TEST(RigidLinkLumpedMass, ModeShapesFollowRigidKinematicsAndAreMassNormalised) {
  const auto m = columnWithRigidBlock();
  auto r = kopruq::ModalSolver::solve(m, fixedBase(), 0, kopruq::MassMatrixType::Lumped);
  ASSERT_TRUE(r.status.success) << r.status.message;
  kopruq::DofManager dm(m);
  const auto M = kopruq::MatrixAssembler::assembleLumpedMass(m);
  for (const auto& mode : r.modes) {
    EXPECT_NEAR(mode.shape.dot(M * mode.shape), 1.0, 1e-9) << "phi^T M phi over the full DOF space";
    const auto d2 = dm.node_dofs(2), d4 = dm.node_dofs(4);
    // slave 4 at e = (0, 0, H + G): u_x = u + t_y e_z, u_y = v - t_x e_z
    EXPECT_NEAR(mode.shape(d4[0]), mode.shape(d2[0]) + mode.shape(d2[4]) * (H + G), 1e-9);
    EXPECT_NEAR(mode.shape(d4[1]), mode.shape(d2[1]) - mode.shape(d2[3]) * (H + G), 1e-9);
  }
}

TEST(RigidLinkLumpedMass, SpectrumParticipationIncludesSlaveMasses) {
  kopruq::ResponseSpectrumInput in; in.direction = 0; in.num_modes = 0;
  in.spectrum = {{1e-4, 2.0}, {1e4, 2.0}};
  const auto s = masses();
  for (auto mt : {kopruq::MassMatrixType::Lumped, kopruq::MassMatrixType::Consistent}) {
    auto r = kopruq::ResponseSpectrumSolver::solve(columnWithRigidBlock(), fixedBase(), in, mt);
    ASSERT_TRUE(r.status.success) << r.status.message;
    if (mt == kopruq::MassMatrixType::Lumped) {
      EXPECT_NEAR(r.total_mass, s.S0, 1e-9 * s.S0) << "m2 + m3 + m4 (the slave masses count)";
      EXPECT_NEAR(r.total_mass_participation_ratio, 1.0, 1e-9) << "all modes: effective masses sum to the total";
    } else {
      // Consistent mass: the rigid block's translational mass is still fully counted; the column's
      // share at node 2 differs from L/2 (consistent distribution), so only the participation sum
      // is checked, plus that the total exceeds the slaves' own mass.
      EXPECT_GT(r.total_mass, s.m3 + s.m4);
      EXPECT_NEAR(r.total_mass_participation_ratio, 1.0, 1e-6);
    }
  }
}
