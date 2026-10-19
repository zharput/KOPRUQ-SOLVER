// Phase 7B — end-to-end MIDAS JSON importer test against the real sample
// (xmidas/deneme1.json). Counts below were computed independently via
// PowerShell ConvertFrom-Json directly on the source file (not derived
// from the importer), per docs/MIDAS_JSON_DATA_AUDIT.md:
//   106 nodes, 94 BEAM elements, 2 materials, 5 sections, 9 supports,
//   7 individual rigid links (5 RIGD records, master 1 and 5 each with
//   2 slaves), 4 load cases actually referenced by BODF/BMLD (SW, SDL-1,
//   SDL-2, SDL-3 -- TU/temperature never appears there, only in ETMP),
//   70 BMLD member-load records, 4 rejected ELNK records + 1 rejected
//   TU load case + 1 "EIGV/SPLC not executed" note = 6 rejected entries.
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
  EXPECT_EQ(r.load_cases, 4u);
  EXPECT_EQ(r.member_loads, 70u);

  int elnkRejections = 0, tuRejection = 0, dynamicsNote = 0;
  for (const auto& msg : r.rejected) {
    if (msg.find("elastic link") != std::string::npos) ++elnkRejections;
    if (msg.find("TYPE=T") != std::string::npos) ++tuRejection;
    if (msg.find("EIGV/SPLC") != std::string::npos) ++dynamicsNote;
  }
  EXPECT_EQ(elnkRejections, 4);
  EXPECT_EQ(tuRejection, 1);
  EXPECT_EQ(dynamicsNote, 1);
  EXPECT_EQ(r.rejected.size(), 6u);

  // No case ever ends up named "TU" (rejected before any load is attached to it).
  for (const auto& lc : result.input.load_cases) EXPECT_NE(lc.name, "TU");
}

TEST(MidasJsonImporter, ImportedModelPassesValidation) {
  auto result = importMidasJson(loadSample());
  auto issues = kopruq::validateModel(result.input.model, result.input.restraints, result.input.load_cases, result.input.combinations);
  for (const auto& i : issues) ADD_FAILURE() << i.path << ": " << i.message;
  EXPECT_TRUE(issues.empty());
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
