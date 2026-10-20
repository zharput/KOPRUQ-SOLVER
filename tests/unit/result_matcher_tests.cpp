// GUI-5: ResultMatcher tests. Builds a results.json fragment directly (the real shape produced
// by kopruq::io::writeResults, include/kopruq/io/ResultsJson.hpp -- not re-derived, just
// reproduced as test fixture data) and ReferenceTable values directly in memory (no Excel I/O
// needed here; ExcelReferenceReader's own tests already cover the file-reading path).
#include <gtest/gtest.h>
#include "ResultMatcher.h"

using namespace kopruq::verification;
using Json = nlohmann::json;

namespace {
CellValue num(double v) { CellValue c; c.isEmpty = false; c.isNumber = true; c.number = v; c.text = std::to_string(v); return c; }
CellValue txt(std::string s) { CellValue c; c.isEmpty = false; c.text = std::move(s); return c; }

Json sampleResults() {
  return Json::parse(R"({
    "load_cases": [
      {
        "name": "SW",
        "status": {"success": true},
        "nodes": [
          {"id": 1, "displacement": [0.1000, 0.0, 0.0, 0.0, 0.0, 0.0], "reaction": [10.0, 0.0, 0.0, 0.0, 0.0, 0.0]}
        ],
        "elements": [
          {"id": 5, "local_end_forces": [100.0,0,0,0,0,0, 90.0,0,0,0,0,0]}
        ]
      },
      {"name": "TU-FAILED", "status": {"success": false}}
    ]
  })");
}

ReferenceTable displacementsTable() {
  ReferenceTable t;
  t.kind = ReferenceFileKind::Displacements;
  t.columns = {{"Node", ""}, {"Load", ""}, {"DX", "(m)"}, {"DY", "(m)"}, {"DZ", "(m)"}, {"RX", "([rad])"}, {"RY", "([rad])"}, {"RZ", "([rad])"}};
  // Row 0: matches node 1 / case "SW" closely (within default tolerance) -> PASS.
  t.rows.push_back({num(1), txt("SW"), num(0.10005), num(0.0), num(0.0), num(0.0), num(0.0), num(0.0)});
  // Row 1: same node/case, DY grossly different -> FAIL.
  t.rows.push_back({num(1), txt("SW"), num(0.1), num(5.0), num(0.0), num(0.0), num(0.0), num(0.0)});
  // Row 2: a node id that was never solved in this load case -> N/A.
  t.rows.push_back({num(999), txt("SW"), num(0.1), num(0.0), num(0.0), num(0.0), num(0.0), num(0.0)});
  // Row 3: a load case that failed to solve -> N/A (results.json lists it but without "nodes").
  t.rows.push_back({num(1), txt("TU-FAILED"), num(0.1), num(0.0), num(0.0), num(0.0), num(0.0), num(0.0)});
  // Row 4: a load case not present in results.json at all -> N/A, with a diagnostic.
  t.rows.push_back({num(1), txt("GHOST-CASE"), num(0.1), num(0.0), num(0.0), num(0.0), num(0.0), num(0.0)});
  return t;
}
}

TEST(ResultMatcher, DisplacementsMatchByNodeAndLoadCaseProducesCorrectPassFail) {
  auto cmp = matchDisplacements(sampleResults(), displacementsTable(), ToleranceSettings{});
  ASSERT_EQ(cmp.rows.size(), 5u * 6u); // 5 rows x 6 components each

  auto find = [&](int rowIndex, const char* component) -> const ComparisonRow& {
    for (int i = 0; i < 6; ++i) {
      const auto& r = cmp.rows[static_cast<std::size_t>(rowIndex * 6 + i)];
      if (r.component == component) return r;
    }
    throw std::runtime_error("not found");
  };

  EXPECT_EQ(find(0, "DX").result.status, ComparisonStatus::Pass);
  EXPECT_EQ(find(1, "DY").result.status, ComparisonStatus::Fail);
  EXPECT_EQ(find(2, "DX").result.status, ComparisonStatus::NotApplicable);
  EXPECT_EQ(find(3, "DX").result.status, ComparisonStatus::NotApplicable);
  EXPECT_EQ(find(4, "DX").result.status, ComparisonStatus::NotApplicable);

  bool sawGhostDiagnostic = false;
  for (const auto& d : cmp.diagnostics) if (d.find("GHOST-CASE") != std::string::npos) sawGhostDiagnostic = true;
  EXPECT_TRUE(sawGhostDiagnostic);
}

TEST(ResultMatcher, BeamForcesMatchesIAndJEndsFromThePartColumn) {
  ReferenceTable t;
  t.kind = ReferenceFileKind::BeamForce;
  t.columns = {{"Elem", ""}, {"Load", ""}, {"Part", ""}, {"Axial", "(kN)"}, {"Shear-y", "(kN)"}, {"Shear-z", "(kN)"},
               {"Torsion", "(kNm)"}, {"Moment-y", "(kNm)"}, {"Moment-z", "(kNm)"}};
  t.rows.push_back({num(5), txt("SW"), txt("I[1]"), num(100.0), num(0), num(0), num(0), num(0), num(0)});
  t.rows.push_back({num(5), txt("SW"), txt("J[1]"), num(90.0), num(0), num(0), num(0), num(0), num(0)});

  auto cmp = matchBeamForces(sampleResults(), t, ToleranceSettings{});
  ASSERT_EQ(cmp.rows.size(), 12u);
  EXPECT_EQ(cmp.rows[0].location, "I");
  EXPECT_EQ(cmp.rows[0].result.status, ComparisonStatus::Pass); // local_end_forces[0] == 100.0 exactly
  EXPECT_EQ(cmp.rows[6].location, "J");
  EXPECT_EQ(cmp.rows[6].result.status, ComparisonStatus::Pass); // local_end_forces[6] == 90.0 exactly
}

TEST(ResultMatcher, ElasticLinksAreAlwaysReportedUnsupportedWithAnExplanatoryDiagnostic) {
  ReferenceTable t;
  t.kind = ReferenceFileKind::ElasticLink;
  t.columns = {{"No.", ""}, {"Load", ""}, {"Node", ""}};
  t.rows.push_back({num(1), txt("SW"), num(26)});
  t.rows.push_back({num(1), txt("SW"), num(11)});

  auto cmp = markElasticLinksUnsupported(t);
  ASSERT_EQ(cmp.rows.size(), 2u);
  for (const auto& r : cmp.rows) EXPECT_EQ(r.result.status, ComparisonStatus::Unsupported);
  ASSERT_FALSE(cmp.diagnostics.empty());
  EXPECT_NE(cmp.diagnostics.front().find("ELNK"), std::string::npos);
}

TEST(ResultMatcher, EmptyReferenceTableProducesNoRowsAndNoDiagnostics) {
  ReferenceTable t;
  t.kind = ReferenceFileKind::ElasticLink;
  t.columns = {{"No.", ""}, {"Load", ""}, {"Node", ""}};
  auto cmp = markElasticLinksUnsupported(t);
  EXPECT_TRUE(cmp.rows.empty());
  EXPECT_TRUE(cmp.diagnostics.empty());
}
