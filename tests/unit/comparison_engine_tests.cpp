#include <gtest/gtest.h>
#include <limits>
#include "ComparisonEngine.h"

using namespace kopruq::verification;

TEST(ComparisonEngine, PassWhenWithinCombinedAbsoluteRelativeTolerance) {
  ToleranceSettings tol; // displacementAbsTol = 1e-4 m, relativeTol = 1e-2
  // |0.10005 - 0.1000| = 5e-5 <= 1e-4 + 1e-2*0.1 = 1.1e-3 -> PASS
  auto r = compareValues(0.10005, 0.1000, QuantityCategory::Displacement, tol);
  EXPECT_EQ(r.status, ComparisonStatus::Pass);
  EXPECT_DOUBLE_EQ(r.signedDiff, 0.10005 - 0.1000);
}

TEST(ComparisonEngine, FailWhenOutsideCombinedTolerance) {
  ToleranceSettings tol;
  // |0.2 - 0.1| = 0.1, floor = 1e-4 + 1e-2*0.1 = 1.1e-3 -> FAIL
  auto r = compareValues(0.2, 0.1, QuantityCategory::Displacement, tol);
  EXPECT_EQ(r.status, ComparisonStatus::Fail);
}

TEST(ComparisonEngine, NearZeroReferenceFallsBackToAbsoluteToleranceFloor) {
  ToleranceSettings tol; // forceAbsTol = 1e-2 kN
  // reference essentially zero: relative term vanishes, only the absolute floor matters.
  auto pass = compareValues(0.005, 0.0, QuantityCategory::Force, tol);
  EXPECT_EQ(pass.status, ComparisonStatus::Pass);
  auto fail = compareValues(0.5, 0.0, QuantityCategory::Force, tol);
  EXPECT_EQ(fail.status, ComparisonStatus::Fail);
}

TEST(ComparisonEngine, DifferentCategoriesUseDifferentAbsoluteTolerances) {
  ToleranceSettings tol;
  // Same raw difference (5e-3), near-zero reference: displacement (floor 1e-4) fails,
  // force (floor 1e-2) passes -- proving the tolerance is never shared across quantity types.
  auto disp = compareValues(5e-3, 0.0, QuantityCategory::Displacement, tol);
  auto force = compareValues(5e-3, 0.0, QuantityCategory::Force, tol);
  EXPECT_EQ(disp.status, ComparisonStatus::Fail);
  EXPECT_EQ(force.status, ComparisonStatus::Pass);
}

TEST(ComparisonEngine, AbsoluteToleranceScaleMultipliesEveryCategoryUniformly) {
  ToleranceSettings tight;
  ToleranceSettings relaxed;
  relaxed.absoluteToleranceScale = 100.0;
  auto failsTight = compareValues(5e-3, 0.0, QuantityCategory::Displacement, tight);
  auto passesRelaxed = compareValues(5e-3, 0.0, QuantityCategory::Displacement, relaxed);
  EXPECT_EQ(failsTight.status, ComparisonStatus::Fail);
  EXPECT_EQ(passesRelaxed.status, ComparisonStatus::Pass);
}

TEST(ComparisonEngine, NonFiniteValueIsNotApplicableNotFabricatedPassOrFail) {
  ToleranceSettings tol;
  auto r = compareValues(std::numeric_limits<double>::quiet_NaN(), 0.1, QuantityCategory::Moment, tol);
  EXPECT_EQ(r.status, ComparisonStatus::NotApplicable);
}

TEST(ComparisonEngine, StatusNamesMatchApprovedUiLabels) {
  EXPECT_STREQ(comparisonStatusName(ComparisonStatus::Pass), "PASS");
  EXPECT_STREQ(comparisonStatusName(ComparisonStatus::Fail), "FAIL");
  EXPECT_STREQ(comparisonStatusName(ComparisonStatus::NotApplicable), "N/A");
  EXPECT_STREQ(comparisonStatusName(ComparisonStatus::Unsupported), "UNSUPPORTED");
}
