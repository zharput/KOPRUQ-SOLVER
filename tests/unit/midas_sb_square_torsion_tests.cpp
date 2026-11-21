// 2026-11-21 -- torsion constant of a square DBUSER "SB" section: MIDAS NX uses the classical
// closed form J = 2.25 a^4 (a = half side), Roark's formula only for non-square rectangles.
// Dedicated new file. Evidence: verification model 19, SB 0.5 x 0.5 -- MIDAS's J back-calculated
// per element (J = T L / (G dtheta)) from its own results: 0.0087890580 .. 0.0087890635.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol) {
  EXPECT_NEAR(actual, expected, tol * std::abs(expected)) << what;
}
}

TEST(MidasSbSquareTorsion, Model19SquareMatchesClassicalFormAndMidas) {
  const auto p = resolveDbUserSection("SB", {0.5, 0.5});
  rel(p.J, 0.0087890625, "2.25 * 0.25^4", 1e-14);
  rel(p.J, 0.00878906, "MIDAS back-calc (0.0087890580 .. 0.0087890635)", 1e-6);
}

TEST(MidasSbSquareTorsion, NonSquareKeepsRoark) {
  // Model 21 SB 1.0 x 0.4: Roark a b^3 (1/3 - 0.21 (b/a)(1 - b^4/(12 a^4))) = 0.015968...; MIDAS 0.015969.
  const auto p = resolveDbUserSection("SB", {1.0, 0.4});
  const double a = 1.0, b = 0.4;
  rel(p.J, a * b * b * b * (1.0 / 3 - 0.21 * (b / a) * (1 - std::pow(b / a, 4) / 12)), "Roark", 1e-14);
  rel(p.J, 0.015969, "MIDAS back-calc", 5e-5);
}
