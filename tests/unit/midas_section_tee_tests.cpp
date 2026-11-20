// 2026-11-20 -- MIDAS NX DBUSER "T" section convention (user decision: match MIDAS). Dedicated
// new file (CLAUDE.md: never overwrite or truncate an existing test file to add tests).
//
// Expected literals: independent Python evaluation of Roark's T-section torsion formula and of
// the shear-area conventions (see MidasSectionLibrary.hpp, tSection). MIDAS's own values for the
// [2,2,1,1] cap beam of models 17/18/22, back-calculated per element from MIDAS's results:
// J 1.052851615 (model 22), AsH 2.0000 (model 22), AsB 1.666666-1.666669 (model 18).
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::abs(expected)) << what;
}
}

TEST(MidasTeeSection, BaslikMatchesRoarkFormulaAndMidas) {
  const auto p = tSection(2.0, 2.0, 1.0, 1.0);
  rel(p.J, 1.0528515625, "J: K1 0.45776041666666667 + K2 0.22888020833333333 + aD4 0.3662109375");
  rel(p.J, 1.052851615, "J vs MIDAS back-calc", 1e-7);
  rel(p.AsH, 2.0, "AsH = tw*H");
  rel(p.AsB, 1.6666666666666667, "AsB = 5/6*B*tf");
  rel(p.AsB, 1.6666675, "AsB vs MIDAS back-calc", 2e-6);
  // Geometry is unchanged by the convention.
  rel(p.area, 3.0, "A");
  rel(p.IH, 0.9166666666666666, "IH");
  rel(p.IB, 0.75, "IB");
}

// A second, unequal-proportion geometry (H 1.2, B 1.5, tw 0.3, tf 0.25): formula evaluated
// independently in Python (no MIDAS data for this one).
TEST(MidasTeeSection, SecondGeometryMatchesIndependentFormulaEvaluation) {
  const auto p = tSection(1.2, 1.5, 0.3, 0.25);
  rel(p.J, 0.016362204298234182, "J = K1 0.006992240246431327 + K2 0.0076995440518028545 + aD4 0.00167042");
  rel(p.AsH, 0.36, "AsH = tw*H");
  rel(p.AsB, 0.3125, "AsB = 5/6*B*tf");
}

TEST(MidasTeeSection, DispatchUsesTheConvention) {
  const auto p = resolveDbUserSection("T", {2.0, 2.0, 1.0, 1.0});
  rel(p.J, 1.0528515625, "J via dispatch");
  rel(p.extentH, 2.0, "overall H");
  rel(p.extentB, 2.0, "overall B");
}
