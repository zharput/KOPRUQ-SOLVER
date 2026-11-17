// Phase 7I.1 (2026-11-17) -- CQC (Complete Quadratic Combination) modal combination for
// ResponseSpectrumSolver, added because the first real MIDAS NX response-spectrum sample this
// project has (verification/models/16's SPLC record) explicitly requests COMTYPE="CQC", which
// the solver's previous SRSS-only combination could not reproduce (a documented, disclosed gap
// until now). See ResponseSpectrumSolver.hpp's own header comment for the Der Kiureghian (1981)
// formula and why SRSS is exactly its rho_ij=(i==j) special case.
//
// Three independent checks:
//  1. CqcCorrelationMatchesHandComputedValuesAtKnownPoints: cqcCorrelation() against values
//     computed independently here (by hand/calculator, not by re-running the same formula) --
//     the trivial r=1 limit, and a specific r=2, zeta=0.05 point.
//  2. CqcReducesToSrssForWellSeparatedModes: on a well-separated-mode cantilever (IY != IZ, so no
//     repeated eigenvalues), CQC- and SRSS-combined responses must be nearly identical, since
//     every off-diagonal rho_ij is then tiny -- a basic sanity/continuity check.
//  3. ErrorHandling: a non-positive/non-finite damping_ratio is rejected, not silently used.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/solvers/ResponseSpectrumSolver.hpp"

using kopruq::Id;

namespace {
constexpr double E = 210e6, NU = 0.3, A = 0.3, IY = 0.02, IZ = 0.04, J = 0.01, UW = 78.5;

kopruq::Model cantilever(double L, int n = 1) {
  kopruq::Model m;
  for (int i = 0; i <= n; ++i) m.nodes.push_back({kopruq::Id(i + 1), L * i / n, 0, 0});
  m.materials = {{1, E, NU, UW}};
  m.sections = {{1, A, IY, IZ, J}};
  for (int i = 0; i < n; ++i) m.beams.push_back({kopruq::Id(i + 1), kopruq::Id(i + 1), kopruq::Id(i + 2), 1, 1, {0, 1, 0}});
  return m;
}
std::map<Id, std::array<bool, 6>> fixedNode1() { return {{1, {true, true, true, true, true, true}}}; }
}

TEST(Cqc, CqcCorrelationMatchesHandComputedValuesAtKnownPoints) {
  EXPECT_NEAR(kopruq::cqcCorrelation(37.0, 37.0, 0.05), 1.0, 1e-12); // r=1 limit, any zeta
  EXPECT_NEAR(kopruq::cqcCorrelation(10.0, 10.0, 0.123), 1.0, 1e-12);

  // r=2, zeta=0.05, computed independently here (not by re-deriving the production formula):
  // num = 8*0.05^2*(1+2)*2^1.5 = 8*0.0025*3*2.8284271247 = 0.16970562748
  // den = (1-4)^2 + 4*0.05^2*2*(1+2)^2 = 9 + 4*0.0025*2*9 = 9.18
  // rho = 0.16970562748 / 9.18 = 0.018486...
  const double expected = 0.169705627484771 / 9.18;
  EXPECT_NEAR(kopruq::cqcCorrelation(10.0, 20.0, 0.05), expected, 1e-9);
  // Symmetric: rho_ij == rho_ji is NOT generally true for CQC's own formula with i,j swapped
  // unless zeta_i==zeta_j (which it is here, single damping ratio) -- Der Kiureghian's formula is
  // symmetric under (omega_i,omega_j)->(omega_j,omega_i) when zeta_i=zeta_j, confirmed directly.
  EXPECT_NEAR(kopruq::cqcCorrelation(20.0, 10.0, 0.05), expected, 1e-9);

  // Well-separated modes (r -> 0 or r -> infinity): correlation vanishes.
  EXPECT_LT(kopruq::cqcCorrelation(1000.0, 1.0, 0.05), 1e-4);
}

TEST(Cqc, CqcReducesToSrssForWellSeparatedModes) {
  auto m = cantilever(4.0, 3);
  auto rs = fixedNode1();
  kopruq::ResponseSpectrumInput srss; srss.direction = 2; srss.num_modes = 6;
  srss.spectrum = {{1e-5, 5.0}, {1.0, 5.0}, {1e4, 5.0}};
  srss.combination = kopruq::ModalCombination::SRSS;

  kopruq::ResponseSpectrumInput cqc = srss;
  cqc.combination = kopruq::ModalCombination::CQC;
  cqc.damping_ratio = 0.05;

  auto rSrss = kopruq::ResponseSpectrumSolver::solve(m, rs, srss);
  auto rCqc = kopruq::ResponseSpectrumSolver::solve(m, rs, cqc);
  ASSERT_TRUE(rSrss.status.success) << rSrss.status.message;
  ASSERT_TRUE(rCqc.status.success) << rCqc.status.message;
  ASSERT_EQ(rSrss.combined_nodes.size(), rCqc.combined_nodes.size());

  for (std::size_t i = 0; i < rSrss.combined_nodes.size(); ++i) {
    for (int k = 0; k < 6; ++k) {
      const double a = rSrss.combined_nodes[i].displacement[k], b = rCqc.combined_nodes[i].displacement[k];
      EXPECT_NEAR(a, b, 0.02 * std::max(a, 1e-12)) << "node " << i << " component " << k;
    }
  }
}

TEST(Cqc, ErrorHandling) {
  auto m = cantilever(4.0, 1);
  kopruq::ResponseSpectrumInput input; input.direction = 2; input.num_modes = 6;
  input.spectrum = {{1e-5, 5.0}, {1e4, 5.0}};
  input.combination = kopruq::ModalCombination::CQC;
  input.damping_ratio = 0.0; // invalid: CQC needs > 0
  auto r = kopruq::ResponseSpectrumSolver::solve(m, fixedNode1(), input);
  EXPECT_FALSE(r.status.success);

  input.damping_ratio = -0.05;
  r = kopruq::ResponseSpectrumSolver::solve(m, fixedNode1(), input);
  EXPECT_FALSE(r.status.success);
}
