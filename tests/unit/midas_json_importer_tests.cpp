// Phase 7B — MIDAS JSON importer tests.
//
// This file previously also end-to-end tested the importer against a real sample,
// xmidas/deneme1.json. That file was deleted from the user's own machine and, by explicit user
// decision, removed from git history going forward (see docs/DEVELOPMENT_STATUS.md); it is
// permanently and irrecoverably gone. The four tests that depended on it
// (ImportCountsMatchIndependentlyComputedTotals, ReportsElasticLinkRejectionCountAndNodeBoundingBox,
// ImportedModelPassesValidation, StaticSolveOfImportedModelDoesNotThrowAndReportsAStatus) were
// removed on 2026-11-19 by explicit user instruction, rather than left permanently failing --
// there is nothing left in the repository to restore them against. The remaining tests below use
// self-contained synthetic fixtures and are unaffected.
#include <gtest/gtest.h>
#include "kopruq/io/midas/MidasJsonImporter.hpp"

using namespace kopruq::io::midas;

TEST(MidasJsonImporter, EmptyNodeSetHasNoBoundingBox) {
  auto doc = nlohmann::json::parse(R"json({"DBG":{"UNIT":{"1":{"FORCE":"KN","DIST":"M"}},"NODE":{}}})json");
  auto empty = importMidasJson(doc);
  EXPECT_FALSE(empty.report.hasBoundingBox);
  EXPECT_EQ(empty.report.nodes, 0u);
}

namespace {
// A minimal, hand-built MIDAS-shaped JSON snippet (not the real sample) for testing ETMP/GTMP/
// BTMP handling in isolation, deterministically.
nlohmann::json minimalMidasModel(const char* extraRecords) {
  std::string text = std::string(R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":4.0,"Y":0.0,"Z":0.0}},
    "MATL":{"1":{"TYPE":"CONC","NAME":"C35/45"}},
    "MATD":{"1":{"TYPE":"CONC","NAME":"C35/45","DATA1":{"CODENAME":"EN04(RC)","DESIGN":{"C_FC":35000.0}}}},
    "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"S","SECT_BEFORE":{"SHAPE":"SB","SECT_I":{"vSIZE":[1.0,1.0]}}}},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}},
    "STLD":{"1":{"NO":1,"NAME":"TU","TYPE":"T"}})json") + extraRecords + "}}";
  return nlohmann::json::parse(text);
}
}

TEST(MidasJsonImporter, EtmpMapsToUniformDeltaT) {
  auto result = importMidasJson(minimalMidasModel(R"(,"ETMP":{"1":{"ITEMS":[{"ID":1,"LCNAME":"TU","GROUP_NAME":"","TEMP":25.0}]}})"));
  ASSERT_EQ(result.input.load_cases.size(), 1u);
  const auto& lc = result.input.load_cases[0];
  EXPECT_EQ(lc.name, "TU");
  ASSERT_EQ(lc.thermal.size(), 1u);
  EXPECT_EQ(lc.thermal[0].element, 1u);
  EXPECT_DOUBLE_EQ(lc.thermal[0].uniform_delta_t, 25.0);
  EXPECT_DOUBLE_EQ(lc.thermal[0].gradient_y_delta_t, 0.0);
  EXPECT_DOUBLE_EQ(lc.thermal[0].gradient_z_delta_t, 0.0);
}

TEST(MidasJsonImporter, GtmpIsRejectedExplicitlyWhenPresent) {
  auto result = importMidasJson(minimalMidasModel(R"(,"GTMP":{"1":{"ITEMS":[{"ID":1,"LCNAME":"TU","TZ":10.0,"TY":0.0,"TYPE":1}]}})"));
  bool found = false;
  for (const auto& msg : result.report.rejected) if (msg.rfind("GTMP", 0) == 0) found = true;
  EXPECT_TRUE(found);
  // No gradient thermal load was fabricated from the unmapped GTMP record.
  for (const auto& lc : result.input.load_cases)
    for (const auto& t : lc.thermal) { EXPECT_DOUBLE_EQ(t.gradient_y_delta_t, 0.0); EXPECT_DOUBLE_EQ(t.gradient_z_delta_t, 0.0); }
}

TEST(MidasJsonImporter, BtmpIsRejectedExplicitlyWhenPresent) {
  auto result = importMidasJson(minimalMidasModel(R"(,"BTMP":{"1":{"ITEMS":[{"ID":1,"LCNAME":"TU"}]}})"));
  bool found = false;
  for (const auto& msg : result.report.rejected) if (msg.rfind("BTMP", 0) == 0) found = true;
  EXPECT_TRUE(found);
}

TEST(MidasJsonImporter, NoGtmpBtmpMeansNoSuchRejection) {
  auto result = importMidasJson(minimalMidasModel(""));
  for (const auto& msg : result.report.rejected) {
    EXPECT_EQ(msg.rfind("GTMP", 0), std::string::npos);
    EXPECT_EQ(msg.rfind("BTMP", 0), std::string::npos);
  }
}
