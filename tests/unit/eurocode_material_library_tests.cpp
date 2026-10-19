// Phase 7M — Eurocode concrete material library (EurocodeMaterialLibrary.hpp).
//
// Independent check: results compared against EN 1992-1-1 Table 3.1's own
// published values (not against the production formula re-derived), with
// a tolerance that accounts for the standard table rounding its own
// unrounded formula results to the nearest 100-1000 MPa:
//   C25/30: fcm=33, fctm=2.6, Ecm=31000 MPa
//   C30/37: fcm=38, fctm=2.9, Ecm=33000 MPa
//   C35/45: fcm=43, fctm=3.2, Ecm=34000 MPa (also the classic 34077 MPa
//            unrounded figure quoted across Eurocode references)
//   C40/50: fcm=48, fctm=3.5, Ecm=35000 MPa
//   C45/55: fcm=53, fctm=3.8, Ecm=36000 MPa
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/materials/EurocodeMaterialLibrary.hpp"

using namespace kopruq::materials;

namespace {
void rel(double actual, double expected, const char* what, double tol) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
}

TEST(EurocodeMaterialLibrary, MatchesPublishedTable3_1_C25_30) {
  auto p = getConcrete("C25/30");
  EXPECT_DOUBLE_EQ(p.fck, 25.0); EXPECT_DOUBLE_EQ(p.fck_cube, 30.0);
  rel(p.fcm, 33.0, "fcm", 1e-9);
  rel(p.fctm, 2.6, "fctm", 0.02);
  rel(p.Ecm, 31000.0, "Ecm", 0.03);
}

TEST(EurocodeMaterialLibrary, MatchesPublishedTable3_1_C30_37) {
  auto p = getConcrete("C30/37");
  rel(p.fcm, 38.0, "fcm", 1e-9);
  rel(p.fctm, 2.9, "fctm", 0.02);
  rel(p.Ecm, 33000.0, "Ecm", 0.03);
}

TEST(EurocodeMaterialLibrary, MatchesPublishedTable3_1_C35_45) {
  auto p = getConcrete("EN-C35/45"); // with the "EN-" prefix the task's example uses
  rel(p.fcm, 43.0, "fcm", 1e-9);
  rel(p.fctm, 3.2, "fctm", 0.02);
  rel(p.Ecm, 34000.0, "Ecm", 0.03);
  EXPECT_DOUBLE_EQ(p.poisson_ratio, 0.2);
  EXPECT_DOUBLE_EQ(p.thermal_expansion_per_K, 1.0e-5);
  EXPECT_DOUBLE_EQ(p.unit_weight, 25.0);
  rel(p.G(), p.Ecm / (2 * 1.2), "derived shear modulus", 1e-9);
}

TEST(EurocodeMaterialLibrary, MatchesPublishedTable3_1_C40_50AndC45_55) {
  auto a = getConcrete("C40/50");
  rel(a.fcm, 48.0, "fcm", 1e-9); rel(a.fctm, 3.5, "fctm", 0.02); rel(a.Ecm, 35000.0, "Ecm", 0.03);
  auto b = getConcrete("C45/55");
  rel(b.fcm, 53.0, "fcm", 1e-9); rel(b.fctm, 3.8, "fctm", 0.02); rel(b.Ecm, 36000.0, "Ecm", 0.03);
}

TEST(EurocodeMaterialLibrary, HighStrengthClassUsesTheAboveC50FctmFormula) {
  // C70/85: fck=70 > 50, so fctm = 2.12*ln(1+fcm/10), fcm=78. Checked against the
  // formula computed independently here (catches a transcription error in the
  // coefficient/divisor), not against a from-memory published table value for this
  // specific high-strength class.
  auto p = getConcrete("C70/85");
  const double expectedFctm = 2.12 * std::log(1.0 + 78.0/10.0);
  rel(p.fctm, expectedFctm, "fctm (above-C50 formula)", 1e-9);
  EXPECT_GT(p.fctm, 4.0); EXPECT_LT(p.fctm, 5.0); // sanity band
}

TEST(EurocodeMaterialLibrary, ToMaterialConvertsMPaToCanonicalKNPerM2) {
  auto p = getConcrete("C35/45");
  auto m = toMaterial(7, p);
  EXPECT_EQ(m.id, 7u);
  rel(m.elastic_modulus, p.Ecm * 1000.0, "elastic_modulus MPa->kN/m^2", 1e-9);
  EXPECT_DOUBLE_EQ(m.poisson_ratio, p.poisson_ratio);
  EXPECT_DOUBLE_EQ(m.unit_weight, p.unit_weight);
  m.validate(); // must be a valid kopruq::Material
}

TEST(EurocodeMaterialLibrary, RejectsMalformedOrInconsistentGrades) {
  EXPECT_THROW(getConcrete("S355"), std::invalid_argument);
  EXPECT_THROW(getConcrete("C35"), std::invalid_argument);
  EXPECT_THROW(getConcrete("C45/35"), std::invalid_argument); // cube strength must exceed cylinder strength
  EXPECT_THROW(getConcrete("Cabc/def"), std::invalid_argument);
  EXPECT_THROW(getConcrete("C0/10"), std::invalid_argument);
}
