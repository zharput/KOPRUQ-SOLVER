// Phase 7B — MIDAS grade-named material resolution (MidasMaterialLibrary.hpp).
//
// Independent check: for C35/45 (fck=35 MPa), Ecm = 9.5*(35+8)^(1/3)*1e3 MPa
// is hand-computed here by explicit cube-root interpolation (43^(1/3)
// between 3.50^3=42.875 and 3.51^3=43.2336, linear interpolation giving
// ~3.5035), not by calling std::cbrt as the production code does -- a
// transcription-error check on the formula/exponent/multiplier, since the
// formula itself is an external (MIDAS-documented), not re-derivable,
// fact. Guard-condition tests (unsupported code name, non-positive fck)
// are independent in the ordinary sense.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasMaterialLibrary.hpp"

using namespace kopruq::io::midas;

TEST(MidasMaterialLibrary, C35_45ElasticModulusMatchesHandComputedCubeRoot) {
  auto r = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0);
  // 43^(1/3) interpolated between 3.50^3=42.875 and 3.51^3=43.2336 -> ~3.5035
  const double cbrt43 = 3.50 + 0.01 * (43.0 - 42.875) / (43.2336 - 42.875);
  const double expectedEcmMPa = 9.5 * cbrt43 * 1000.0;
  const double expectedE_kNm2 = expectedEcmMPa * 1000.0;
  EXPECT_NEAR(r.material.elastic_modulus, expectedE_kNm2, 2e-3 * expectedE_kNm2);
  EXPECT_EQ(r.material.id, 1u);
  EXPECT_DOUBLE_EQ(r.material.poisson_ratio, 0.2);
  EXPECT_DOUBLE_EQ(r.material.unit_weight, 25.0);
  EXPECT_DOUBLE_EQ(r.material.thermal_expansion, 1.0e-5);
  ASSERT_EQ(r.assumptions.size(), 4u);
}

TEST(MidasMaterialLibrary, C45_55GivesAHigherModulusThanC35_45) {
  auto a = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0);
  auto b = resolveConcreteGradeMaterial(2, "EN04(RC)", 45.0);
  EXPECT_GT(b.material.elastic_modulus, a.material.elastic_modulus);
}

TEST(MidasMaterialLibrary, RejectsUnsupportedCodeFamily) {
  EXPECT_THROW(resolveConcreteGradeMaterial(1, "ACI318(RC)", 35.0), std::invalid_argument);
  EXPECT_THROW(resolveConcreteGradeMaterial(1, "EN04(STEEL)", 35.0), std::invalid_argument);
}

TEST(MidasMaterialLibrary, RejectsNonPositiveOrNonFiniteStrength) {
  EXPECT_THROW(resolveConcreteGradeMaterial(1, "EN04(RC)", 0.0), std::invalid_argument);
  EXPECT_THROW(resolveConcreteGradeMaterial(1, "EN04(RC)", -5.0), std::invalid_argument);
  EXPECT_THROW(resolveConcreteGradeMaterial(1, "EN04(RC)", std::nan("")), std::invalid_argument);
}
