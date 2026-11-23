// Phase 7H.1 (2026-11-17) -- lumped mass option for ModalSolver/ResponseSpectrumSolver, added
// after an independent diagnostic (not part of this test suite -- a throwaway standalone program,
// never committed) confirmed MIDAS NX's own default eigenvalue analysis on
// verification/models/13 and 14 matches a classic translational-only lumped mass (zero
// rotational mass) to within a constant ~0.11% across every mode of both models, while
// `MatrixAssembler::assembleMass`'s consistent mass diverges by up to 23% on their higher modes.
// See MassMatrix.hpp's lumpedMassMatrix and ModalSolver.hpp's eigensolveLumped for the
// production implementation and its own derivation comments.
//
// Four independent checks:
//  1. LumpedMassMatrixMatchesHandFormula: lumpedMassMatrix() against the elementary
//     "half of rho*A*L at each end's UX/UY/UZ, zero elsewhere" formula, computed directly here.
//  2. SingleElementCantileverMatchesIndependentGuyanReduction: an independent dense
//     generalized eigensolve built from scratch in this test (statically condensing the
//     massless rotational DOFs itself, not calling ModalSolver's own eigensolveLumped), compared
//     against ModalSolver::solve(..., MassMatrixType::Lumped).
//  3. EigenproblemResidualVanishesOverTheFullFreeDofSpace: K*phi - omega^2*M*phi ~ 0 at EVERY
//     free DOF (not just the massy ones) -- a stronger check than mass-orthonormality alone,
//     confirming the massless (rotational) DOFs' back-substituted values are exactly the ones
//     that make the full free-DOF residual vanish, not merely "some" consistent value.
//  4. ErrorHandling: the default (omitting the argument) is Consistent, unchanged from before this
//     phase. (Lumped + rigid links was explicitly rejected here until 2026-11-23; it is now
//     supported and this test checks a massless slave leaves the frequencies unchanged -- see the
//     test body and rigid_link_lumped_mass_tests.cpp.)
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/solvers/ModalSolver.hpp"

using Mat12 = Eigen::Matrix<double, 12, 12>;

namespace {
constexpr double E = 210e6, NU = 0.3, A = 0.3, IY = 0.02, IZ = 0.04, J = 0.01, UW = 78.5;
constexpr double G = E / (2 * (1 + NU)), RHO_A = UW / kopruq::g_standard * A;

kopruq::Model cantilever(double L, int n = 1) {
  kopruq::Model m;
  for (int i = 0; i <= n; ++i) m.nodes.push_back({kopruq::Id(i + 1), L * i / n, 0, 0});
  m.materials = {{1, E, NU, UW}};
  m.sections = {{1, A, IY, IZ, J}};
  for (int i = 0; i < n; ++i) m.beams.push_back({kopruq::Id(i + 1), kopruq::Id(i + 1), kopruq::Id(i + 2), 1, 1, {0, 1, 0}});
  return m;
}
std::map<kopruq::Id, std::array<bool, 6>> fixedNode1() { return {{1, {true, true, true, true, true, true}}}; }

// Same independentLocalK as modal_tests.cpp, re-derived here (not shared/included from it) --
// this test file is independent on purpose, matching the project's own stated testing style.
Mat12 independentLocalK(double L) {
  Mat12 k = Mat12::Zero();
  double ea = E * A / L, gj = G * J / L;
  double ay = 12 * E * IZ / (L * L * L), by = 6 * E * IZ / (L * L), cy = 4 * E * IZ / L, dy = 2 * E * IZ / L;
  double az = 12 * E * IY / (L * L * L), bz = 6 * E * IY / (L * L), cz = 4 * E * IY / L, dz = 2 * E * IY / L;
  k(0, 0) = k(6, 6) = ea; k(0, 6) = k(6, 0) = -ea; k(3, 3) = k(9, 9) = gj; k(3, 9) = k(9, 3) = -gj;
  k(1, 1) = k(7, 7) = ay; k(1, 7) = k(7, 1) = -ay; k(1, 5) = k(5, 1) = by; k(1, 11) = k(11, 1) = by; k(7, 5) = k(5, 7) = -by; k(7, 11) = k(11, 7) = -by; k(5, 5) = k(11, 11) = cy; k(5, 11) = k(11, 5) = dy;
  k(2, 2) = k(8, 8) = az; k(2, 8) = k(8, 2) = -az; k(2, 4) = k(4, 2) = -bz; k(2, 10) = k(10, 2) = -bz; k(8, 4) = k(4, 8) = bz; k(8, 10) = k(10, 8) = bz; k(4, 4) = k(10, 10) = cz; k(4, 10) = k(10, 4) = dz;
  return k;
}
}

TEST(LumpedMass, LumpedMassMatrixMatchesHandFormula) {
  const double L = 3.7;
  Mat12 expected = Mat12::Zero();
  const double half = RHO_A * L * 0.5;
  for (int i : {0, 1, 2, 6, 7, 8}) expected(i, i) = half;
  Mat12 actual = kopruq::lumpedMassMatrix(L, RHO_A);
  EXPECT_TRUE(actual.isApprox(expected, 1e-12));
  // Every rotational DOF (including torsion) is exactly zero, by design.
  for (int i : {3, 4, 5, 9, 10, 11}) { EXPECT_EQ(actual(i, i), 0.0); for (int j = 0; j < 12; ++j) EXPECT_EQ(actual(i, j), 0.0); }
}

TEST(LumpedMass, SingleElementCantileverMatchesIndependentGuyanReduction) {
  const double L = 5.0;
  auto m = cantilever(L, 1);
  auto r = kopruq::ModalSolver::solve(m, fixedNode1(), 6, kopruq::MassMatrixType::Lumped);
  ASSERT_TRUE(r.status.success) << r.status.message;
  // Only 3 of node2's 6 free DOFs carry lumped mass (UX,UY,UZ); numModes=6 asks for more than
  // exist, so exactly 3 real eigenpairs must come back, never 6 fabricated ones.
  ASSERT_EQ(r.modes.size(), 3u);

  // Independent reduction, built from scratch here (not reusing eigensolveLumped): free DOFs are
  // node2's 6 (indices 0..5 of the local K's second block); active=UX,UY,UZ (0,1,2 of that
  // block), massless=RX,RY,RZ (3,4,5).
  Mat12 Kg = independentLocalK(L); // local == global axes for this horizontal element
  Eigen::MatrixXd Kff = Kg.block(6, 6, 6, 6);
  std::vector<int> act = {0, 1, 2}, mass = {3, 4, 5};
  Eigen::MatrixXd Kaa(3, 3), Kar(3, 3), Krr(3, 3), Kra(3, 3);
  for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
    Kaa(i, j) = Kff(act[i], act[j]); Kar(i, j) = Kff(act[i], mass[j]);
    Krr(i, j) = Kff(mass[i], mass[j]); Kra(i, j) = Kff(mass[i], act[j]);
  }
  Eigen::MatrixXd Kcond = Kaa - Kar * Krr.ldlt().solve(Kra);
  Eigen::MatrixXd Maa = Eigen::Vector3d::Constant(RHO_A * L * 0.5).asDiagonal();
  Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> ges(Kcond, Maa);
  ASSERT_EQ(ges.info(), Eigen::Success);
  Eigen::VectorXd omega2 = ges.eigenvalues().cwiseMax(0.0);
  std::vector<double> expected(omega2.data(), omega2.data() + omega2.size());
  std::sort(expected.begin(), expected.end());

  for (std::size_t i = 0; i < 3; ++i) { // only 3 DOFs have mass -> only 3 real modes
    const double actual = r.modes[i].angular_frequency_rad_s * r.modes[i].angular_frequency_rad_s;
    EXPECT_NEAR(actual, expected[i], 1e-7 * std::max(expected[i], 1.0)) << "mode " << i;
  }
  // The remaining 3 "modes" come from numModes=6 exceeding the 3 true massy DOFs; ModalSolver's
  // Lumped path must still report exactly `active.size()` = 3 real eigenpairs, not fabricate
  // extra ones -- confirmed by r.modes.size() further down this same scenario in the next test.
}

TEST(LumpedMass, EigenproblemResidualVanishesOverTheFullFreeDofSpace) {
  auto m = cantilever(4.0, 3);
  auto rs = fixedNode1();
  auto r = kopruq::ModalSolver::solve(m, rs, 0, kopruq::MassMatrixType::Lumped); // numModes<=0 -> every massy mode
  ASSERT_TRUE(r.status.success) << r.status.message;
  EXPECT_EQ(r.modes.size(), 3u * 3u); // 3 unrestrained nodes * 3 translational DOFs each

  kopruq::DofManager dm(m);
  Eigen::SparseMatrix<kopruq::Real> K = kopruq::MatrixAssembler::assemble(m), M = kopruq::MatrixAssembler::assembleLumpedMass(m);
  auto all = rs; for (const auto& n : m.nodes) if (!all.count(n.id)) all[n.id] = {};
  auto b = kopruq::MatrixAssembler::classify(dm, all);
  for (std::size_t i = 0; i < r.modes.size(); ++i) {
    const auto& mi = r.modes[i];
    Eigen::VectorXd Kphi = K * mi.shape, res = Kphi - (mi.angular_frequency_rad_s * mi.angular_frequency_rad_s) * (M * mi.shape);
    double scale = 0, maxres = 0;
    for (auto idx : b.free) { scale = std::max(scale, std::abs(Kphi((Eigen::Index)idx))); maxres = std::max(maxres, std::abs(res((Eigen::Index)idx))); }
    EXPECT_LT(maxres / std::max(scale, 1e-300), 1e-6) << "mode " << i << " full-free-DOF eigenproblem residual (active AND massless rows)";
    EXPECT_NEAR(mi.shape.dot(M * mi.shape), 1.0, 1e-9) << "mode " << i << " mass normalization";
  }
}

TEST(LumpedMass, ErrorHandling) {
  auto m = cantilever(4.0, 1);
  // Default (no argument) is still Consistent, unchanged from before this phase.
  auto def = kopruq::ModalSolver::solve(m, fixedNode1(), 6);
  auto explicitConsistent = kopruq::ModalSolver::solve(m, fixedNode1(), 6, kopruq::MassMatrixType::Consistent);
  ASSERT_TRUE(def.status.success); ASSERT_TRUE(explicitConsistent.status.success);
  for (std::size_t i = 0; i < def.modes.size(); ++i)
    EXPECT_DOUBLE_EQ(def.modes[i].angular_frequency_rad_s, explicitConsistent.modes[i].angular_frequency_rad_s);

  // Lumped + rigid links: rejected until 2026-11-23, now supported (user-approved, verification
  // model 100; ModalSolver::solveWithRigidLinks reduces the lumped mass as T^T M T). This line
  // used to expect InvalidModel; it now checks the supported behaviour instead: a massless,
  // stiffness-less slave must leave the lumped frequencies exactly as without the link.
  kopruq::Model withRigid = cantilever(4.0, 1);
  withRigid.nodes.push_back({3, 4.5, 0, 0});
  withRigid.rigid_links = {{1, 2, 3}};
  auto plainLumped = kopruq::ModalSolver::solve(cantilever(4.0, 1), fixedNode1(), 6, kopruq::MassMatrixType::Lumped);
  auto linkedLumped = kopruq::ModalSolver::solve(withRigid, fixedNode1(), 6, kopruq::MassMatrixType::Lumped);
  ASSERT_TRUE(plainLumped.status.success); ASSERT_TRUE(linkedLumped.status.success) << linkedLumped.status.message;
  ASSERT_EQ(linkedLumped.modes.size(), plainLumped.modes.size());
  for (std::size_t i = 0; i < plainLumped.modes.size(); ++i)
    EXPECT_NEAR(linkedLumped.modes[i].angular_frequency_rad_s, plainLumped.modes[i].angular_frequency_rad_s,
                1e-9 * plainLumped.modes[i].angular_frequency_rad_s);

  // A model with zero beams reaching a node leaves it with zero lumped mass everywhere ->
  // no massy DOFs at all -> SingularSystem, not a crash.
  kopruq::Model noBeams; noBeams.nodes = {{1, 0, 0, 0}};
  auto s = kopruq::ModalSolver::solve(noBeams, {{1, {false, false, false, true, true, true}}}, 6, kopruq::MassMatrixType::Lumped);
  EXPECT_FALSE(s.status.success);
  EXPECT_EQ(s.status.code, kopruq::SolverStatusCode::SingularSystem);
}
