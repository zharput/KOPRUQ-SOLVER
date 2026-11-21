// 2026-11-21 -- torsion constant of MIDAS DBUSER "H" sections with web-to-flange fillets r1.
// Dedicated new file. MIDAS's own J, back-calculated per element (J = T L / (G dtheta)) from its
// results of verification model 10 rebuilt with filleted user H sections (3D model):
//   [0.93218, 0.4191, 0.02032, 0.03429, 0, 0, r1 0.02413]   J 1.4548661 .. 1.4548672 e-5 (4 elements)
//   [0.35052, 0.37338, 0.015621, 0.015621, 0, 0, r1 0.017653] J 1.6694107 .. 1.6694110 e-6
//   [0.9, 1.0, 0.03, 0.03, 1.0, 0.03, r1 0]                  J 2.5830000 e-5 (midline thin-strip sum)
// Expected literals: independent Python evaluation of Roark's filleted I-section formula
// (El Darwish & Johnston) and of the strip sum.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol) {
  EXPECT_NEAR(actual, expected, tol * std::abs(expected)) << what;
}
}

TEST(MidasUserHFilletTorsion, W36x247DimensionsMatchRoarkAndMidas) {
  const auto p = resolveDbUserSection("H", {0.93218, 0.4191, 0.02032, 0.03429, 0.0, 0.0, 0.02413, 0.0});
  rel(p.J, 1.4548666938937491e-05, "Roark filleted I (independent Python)", 1e-12);
  rel(p.J, 1.45486665e-05, "MIDAS back-calc (1.4548661..672e-5)", 1e-6);
}

TEST(MidasUserHFilletTorsion, Hp14x89DimensionsMatchRoarkAndMidas) {
  const auto p = resolveDbUserSection("H", {0.35052, 0.37338, 0.015621, 0.015621, 0.0, 0.0, 0.017653, 0.0});
  rel(p.J, 1.6694108175190197e-06, "Roark filleted I (independent Python)", 1e-12);
  rel(p.J, 1.66941085e-06, "MIDAS back-calc (1.6694107..110e-6)", 1e-6);
}

TEST(MidasUserHFilletTorsion, NoFilletKeepsTheStripSum) {
  const auto p = resolveDbUserSection("H", {0.9, 1.0, 0.03, 0.03, 1.0, 0.03, 0.0, 0.0});
  rel(p.J, (2 * 1.0 * 0.03 * 0.03 * 0.03 + (0.9 - 0.03) * 0.03 * 0.03 * 0.03) / 3, "midline strip sum", 1e-12);
  rel(p.J, 2.5830000e-05, "MIDAS back-calc", 1e-6);
}

TEST(MidasUserHFilletTorsion, UnequalFlangesWithFilletsAreRejected) {
  EXPECT_THROW(resolveDbUserSection("H", {1.0, 0.5, 0.02, 0.03, 0.6, 0.05, 0.01, 0.0}), std::invalid_argument);
}
