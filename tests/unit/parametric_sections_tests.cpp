// Phase 7S — hollow circle (annulus) closed-form section properties
// (ParametricSections.hpp). Expected values hand-computed independently.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/sections/ParametricSections.hpp"

using namespace kopruq::sections;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-9)) << what;
}
}

TEST(ParametricSections, HollowCircleMatchesHandComputation) {
  auto p = hollowCircle(1.0, 0.6);
  rel(p.area, kPi/4.0 * (1.0 - 0.36), "area");
  rel(p.Iyy, kPi/64.0 * (1.0 - 0.1296), "Iyy");
  rel(p.Izz, p.Iyy, "Izz == Iyy (annulus symmetry)", 1e-12);
  rel(p.J, 2.0 * p.Iyy, "J == 2*I (annulus, same as solid circle)", 1e-12);
}

TEST(ParametricSections, ZeroInnerDiameterReducesToSolidCircle) {
  auto p = hollowCircle(1.2, 0.0);
  rel(p.area, kPi/4.0 * 1.2*1.2, "area == solid circle area");
  rel(p.Iyy, kPi/64.0 * std::pow(1.2, 4), "Iyy == solid circle Iyy");
}

TEST(ParametricSections, RejectsInvalidDiameters) {
  EXPECT_THROW(hollowCircle(0.0, 0.0), std::invalid_argument);
  EXPECT_THROW(hollowCircle(1.0, 1.0), std::invalid_argument);   // Di must be < Do
  EXPECT_THROW(hollowCircle(1.0, 1.2), std::invalid_argument);
  EXPECT_THROW(hollowCircle(1.0, -0.1), std::invalid_argument);
}
