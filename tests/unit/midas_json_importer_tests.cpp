// Phase 7B — end-to-end MIDAS JSON importer test against the real sample
// (xmidas/deneme1.json). Counts below were computed independently via
// PowerShell ConvertFrom-Json directly on the source file (not derived
// from the importer), per docs/MIDAS_JSON_DATA_AUDIT.md:
//   106 nodes, 94 BEAM elements, 2 materials, 5 sections, 9 supports,
//   7 individual rigid links (5 RIGD records, master 1 and 5 each with
//   2 slaves), 70 BMLD member-load records, 40 ETMP thermal-load records
//   (all TEMP=40.0, case "TU"), 5 load cases (SW, SDL-1, SDL-2, SDL-3,
//   TU -- now that Phase 7T exists, ETMP maps directly onto
//   MemberThermalLoad::uniform_delta_t, so "TU" is imported, not
//   rejected), 4 rejected ELNK records + 1 "EIGV/SPLC not executed" note
//   = 5 rejected entries (GTMP/BTMP are not present in this sample, so
//   their own rejection path is not exercised here).
#include <gtest/gtest.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include "kopruq/io/midas/MidasJsonImporter.hpp"
#include "kopruq/core/Validate.hpp"

using namespace kopruq::io::midas;

namespace {
nlohmann::json loadSample() {
  std::ifstream in(std::string(KOPRUQ_SOURCE_DIR) + "/xmidas/deneme1.json", std::ios::binary);
  if (!in) throw std::runtime_error("xmidas/deneme1.json not found relative to KOPRUQ_SOURCE_DIR");
  std::stringstream ss; ss << in.rdbuf();
  return nlohmann::json::parse(ss.str());
}
}

TEST(MidasJsonImporter, ImportCountsMatchIndependentlyComputedTotals) {
  auto result = importMidasJson(loadSample());
  const auto& r = result.report;
  EXPECT_EQ(r.nodes, 106u);
  EXPECT_EQ(r.beams, 94u);
  EXPECT_EQ(r.materials, 2u);
  EXPECT_EQ(r.sections, 5u);
  EXPECT_EQ(r.supports, 9u);
  EXPECT_EQ(r.rigid_links, 7u);
  EXPECT_EQ(r.load_cases, 5u);
  EXPECT_EQ(r.member_loads, 70u);
  EXPECT_EQ(r.thermal_loads, 40u);

  int elnkRejections = 0, dynamicsNote = 0;
  for (const auto& msg : r.rejected) {
    if (msg.find("elastic link") != std::string::npos) ++elnkRejections;
    if (msg.find("EIGV/SPLC") != std::string::npos) ++dynamicsNote;
  }
  EXPECT_EQ(elnkRejections, 4);
  EXPECT_EQ(dynamicsNote, 1);
  EXPECT_EQ(r.rejected.size(), 5u);

  // "TU" is now imported (ETMP -> MemberThermalLoad), with all 40 uniform_delta_t == 40.0.
  bool foundTu = false;
  for (const auto& lc : result.input.load_cases) {
    if (lc.name != "TU") continue;
    foundTu = true;
    ASSERT_EQ(lc.thermal.size(), 40u);
    for (const auto& t : lc.thermal) EXPECT_DOUBLE_EQ(t.uniform_delta_t, 40.0);
  }
  EXPECT_TRUE(foundTu);
}

TEST(MidasJsonImporter, ImportedModelPassesValidation) {
  auto result = importMidasJson(loadSample());
  auto issues = kopruq::validateModel(result.input.model, result.input.restraints, result.input.load_cases, result.input.combinations);
  for (const auto& i : issues) ADD_FAILURE() << i.path << ": " << i.message;
  EXPECT_TRUE(issues.empty());
}

namespace {
// A minimal, hand-built MIDAS-shaped JSON snippet (not the real sample) for testing ETMP/GTMP/
// BTMP handling in isolation, deterministically.
nlohmann::json minimalMidasModel(const char* extraRecords) {
  std::string text = std::string(R"({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":4.0,"Y":0.0,"Z":0.0}},
    "MATL":{"1":{"TYPE":"CONC","NAME":"C35/45"}},
    "MATD":{"1":{"TYPE":"CONC","NAME":"C35/45","DATA1":{"CODENAME":"EN04(RC)","DESIGN":{"C_FC":35000.0}}}},
    "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"S","SECT_BEFORE":{"SHAPE":"SB","SECT_I":{"vSIZE":[1.0,1.0]}}}},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}},
    "STLD":{"1":{"NO":1,"NAME":"TU","TYPE":"T"}})") + extraRecords + "}}";
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

TEST(MidasJsonImporter, StaticSolveOfImportedModelDoesNotThrowAndReportsAStatus) {
  auto result = importMidasJson(loadSample());
  auto sol = kopruq::LinearStaticSolver::solveCases(result.input.model, result.input.restraints, result.input.load_cases, result.input.combinations);
  // Deliberately not asserting success here: whether this real bridge model's subset of
  // restraints (9 fixed supports plus 7 rigid links) leaves every DOF non-singular is a
  // genuine structural question about THIS model, not yet independently confirmed by hand
  // -- the status message is what matters for diagnosing it, not a hard pass/fail guess.
  if (!sol.status.success) std::cerr << "[info] static solve status: " << sol.status.message << '\n';
  SUCCEED();
}
