// Phase 7B — MIDAS grade-named material resolution (MidasMaterialLibrary.hpp).
//
// Expected Ecm (2026-11-20, user decision: EN1992-1-1:2004 formula for every grade, not the
// Table 3.1 rounded values): Ecm = 22*(fcm/10)^0.3 GPa, fcm = fck+8. For C35/45,
// 22000*4.3^0.3 = 34077.146199... MPa, computed independently (Python, outside this code base)
// and written here as a literal -- a transcription-error check on the formula/exponent/
// multiplier; it also equals the E back-calculated from MIDAS's own model-18/21 results
// (34077 MPa). Supersedes the earlier 9.5*(fck+8)^(1/3) cube-root expectation (a different,
// older ENV-1992 curve, 33.28 GPa). Guard-condition tests are independent in the ordinary sense.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasMaterialLibrary.hpp"

using namespace kopruq::io::midas;

TEST(MidasMaterialLibrary, C35_45ElasticModulusMatchesEN1992Formula) {
  auto r = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0);
  const double expectedE_kNm2 = 34077.14619918933 * 1000.0; // 22000*(43/10)^0.3 MPa -> kN/m^2
  EXPECT_NEAR(r.material.elastic_modulus, expectedE_kNm2, 1e-9 * expectedE_kNm2);
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

// 2026-11-20 (user decision, supersedes the 2026-11-16 Table 3.1 lookup): a named grade uses
// the same EN1992-1-1 formula as any other, NOT the table's rounded value (C35/45: 34077.1 MPa,
// not 34000 MPa) -- see MidasMaterialLibrary.hpp's header comment for the MIDAS back-calculation
// evidence.
TEST(MidasMaterialLibrary, NamedGradeUsesFormulaNotTheRoundedTableValue) {
  auto r = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0, "C35/45");
  EXPECT_NEAR(r.material.elastic_modulus, 34077.14619918933e3, 1e-3);
  EXPECT_NE(r.material.elastic_modulus, 34.0e6); // explicitly not the Table 3.1 rounded 34 GPa
  ASSERT_EQ(r.assumptions.size(), 4u);
  EXPECT_NE(r.assumptions[0].source.find("22*(fcm/10)^0.3"), std::string::npos);
}

TEST(MidasMaterialLibrary, GradeNameDoesNotChangeTheModulus) {
  // Ecm depends on fck alone: any grade name (or none) gives the identical formula value.
  auto withoutTable = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0, "NOT-A-TABLE-GRADE");
  auto noGradeAtAll = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0);
  auto named = resolveConcreteGradeMaterial(1, "EN04(RC)", 35.0, "C35/45");
  EXPECT_DOUBLE_EQ(withoutTable.material.elastic_modulus, noGradeAtAll.material.elastic_modulus);
  EXPECT_DOUBLE_EQ(named.material.elastic_modulus, noGradeAtAll.material.elastic_modulus);
  EXPECT_NE(withoutTable.material.elastic_modulus, 34.0e6);
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
