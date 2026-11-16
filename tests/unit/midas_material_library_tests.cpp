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

// Phase 7B requalification (2026-11-16): named grades in EN1992-1-1 Table 3.1 use that
// table's fixed/rounded Ecm, not the continuous fck-based formula -- see
// MidasMaterialLibrary.hpp's header comment for the empirical cross-check against real
// MIDAS reference results that motivated this (verification/models/04, user-approved).
TEST(MidasMaterialLibrary, NamedGradeInTable3_1UsesFixedEcmNotTheFormula) {
  auto r = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0, "C35/45");
  EXPECT_DOUBLE_EQ(r.material.elastic_modulus, 34.0e6); // 34 GPa -> kN/m^2
  ASSERT_EQ(r.assumptions.size(), 4u);
  EXPECT_NE(r.assumptions[0].source.find("Table 3.1"), std::string::npos);
}

TEST(MidasMaterialLibrary, GradeNotInTableFallsBackToFormula) {
  // Same fck as the C35/45 case above, but a grade name not in the Table 3.1 lookup --
  // must reproduce the pre-existing formula-derived value, not the table's 34 GPa.
  auto withoutTable = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0, "NOT-A-TABLE-GRADE");
  auto noGradeAtAll = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0);
  EXPECT_DOUBLE_EQ(withoutTable.material.elastic_modulus, noGradeAtAll.material.elastic_modulus);
  EXPECT_NE(withoutTable.material.elastic_modulus, 34.0e6);
  EXPECT_NE(withoutTable.assumptions[0].source.find("not in the EN1992-1-1 Table 3.1"), std::string::npos);
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
