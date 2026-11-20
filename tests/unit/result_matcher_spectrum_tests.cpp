// 2026-11-20 -- ResultMatcher: response-spectrum cases (verification::appendSpectrumCases).
// Dedicated new file. A spectrum results document (the shape written by
// io::writeResponseSpectrumResults -- reproduced here as fixture data) is merged into a static
// results document under MIDAS's own "<name>(RS)" load-case names; CQC/SRSS peaks are magnitudes,
// so the matchers compare |KOPRUQ| with |MIDAS| and apply no beam-force sign convention there,
// while ordinary load cases keep their signed comparison.
#include <gtest/gtest.h>
#include "ResultMatcher.h"

using namespace kopruq::verification;
using Json = nlohmann::json;

namespace {
CellValue num(double v) { CellValue c; c.isEmpty = false; c.isNumber = true; c.number = v; c.text = std::to_string(v); return c; }
CellValue txt(std::string s) { CellValue c; c.isEmpty = false; c.text = std::move(s); return c; }

Json staticResults() {
  return Json::parse(R"({"load_cases":[{"name":"DL","status":{"success":true},
    "nodes":[{"id":1,"displacement":[0.01,0,0,0,0,0],"reaction":[5,0,0,0,0,0]}],
    "elements":[{"id":7,"local_end_forces":[-100,0,0,0,0,0, 100,0,0,0,0,0]}]}]})");
}
Json spectrumResults() {
  return Json::parse(R"({"cases":[
    {"name":"SPECX","status":{"success":true},
     "combined_nodes":[{"id":1,"displacement":[0.02,0.001,0,0,0,0.0005],"reaction":[40,3,0,0,0,0]}],
     "combined_elements":[{"id":7,"local_end_forces":[50,8,0,0,0,30, 50,8,0,0,0,12],"global_end_forces":[0,0,0,0,0,0,0,0,0,0,0,0]}]},
    {"name":"SPECY","status":{"success":false}}]})");
}
const ComparisonRow& rowOf(const CategoryComparison& c, const std::string& lc, const std::string& comp, const std::string& loc = "") {
  for (const auto& r : c.rows) if (r.loadCase == lc && r.component == comp && r.location == loc) return r;
  throw std::runtime_error("row not found");
}
}

TEST(ResultMatcherSpectrum, AppendsSuccessfulCasesUnderMidasRsNamesOnce) {
  Json results = staticResults();
  appendSpectrumCases(results, spectrumResults());
  appendSpectrumCases(results, spectrumResults()); // idempotent
  ASSERT_EQ(results["load_cases"].size(), 2u);    // DL + SPECX(RS); the failed SPECY is not added
  EXPECT_EQ(results["load_cases"][1]["name"], "SPECX(RS)");
  EXPECT_TRUE(results["load_cases"][1]["peak_magnitude"].get<bool>());
}

TEST(ResultMatcherSpectrum, NodeQuantitiesCompareMagnitudes) {
  Json results = staticResults();
  appendSpectrumCases(results, spectrumResults());
  ReferenceTable d;
  d.kind = ReferenceFileKind::Displacements;
  d.columns = {{"Node", ""}, {"Load", ""}, {"DX", "(m)"}, {"DY", "(m)"}, {"DZ", "(m)"}, {"RX", "([rad])"}, {"RY", "([rad])"}, {"RZ", "([rad])"}};
  d.rows.push_back({num(1), txt("SPECX(RS)"), num(0.02), num(-0.001), num(0), num(0), num(0), num(0.0005)});
  d.rows.push_back({num(1), txt("SPECY(RS)"), num(0.02), num(0), num(0), num(0), num(0), num(0)});
  d.rows.push_back({num(1), txt("DL"), num(-0.01), num(0), num(0), num(0), num(0), num(0)}); // signed case: sign matters
  const auto c = matchDisplacements(results, d, ToleranceSettings{});
  EXPECT_EQ(rowOf(c, "SPECX(RS)", "DX").result.status, ComparisonStatus::Pass);
  EXPECT_EQ(rowOf(c, "SPECX(RS)", "DY").result.status, ComparisonStatus::Pass); // |-0.001| vs 0.001
  EXPECT_EQ(rowOf(c, "SPECY(RS)", "DX").result.status, ComparisonStatus::NotApplicable);
  EXPECT_EQ(rowOf(c, "DL", "DX").result.status, ComparisonStatus::Fail); // 0.01 vs -0.01 stays a FAIL

  ReferenceTable r;
  r.kind = ReferenceFileKind::Reaction;
  r.columns = {{"Node", ""}, {"Load", ""}, {"FX", "(kN)"}, {"FY", "(kN)"}, {"FZ", "(kN)"}, {"MX", "(kN*m)"}, {"MY", "(kN*m)"}, {"MZ", "(kN*m)"}};
  r.rows.push_back({num(1), txt("SPECX(RS)"), num(40), num(3.5), num(0), num(0), num(0), num(0)});
  const auto rc = matchReactions(results, r, ToleranceSettings{});
  EXPECT_EQ(rowOf(rc, "SPECX(RS)", "FX").result.status, ComparisonStatus::Pass);
  EXPECT_EQ(rowOf(rc, "SPECX(RS)", "FY").result.status, ComparisonStatus::Fail); // 3 vs 3.5 is a real difference
}

TEST(ResultMatcherSpectrum, BeamForcesCompareMagnitudesWithoutSignConvention) {
  Json results = staticResults();
  appendSpectrumCases(results, spectrumResults());
  ReferenceTable b;
  b.kind = ReferenceFileKind::BeamForce;
  b.columns = {{"Elem", ""}, {"Load", ""}, {"Part", ""}, {"Axial", "(kN)"}, {"Shear-y", "(kN)"}, {"Shear-z", "(kN)"},
               {"Torsion", "(kN*m)"}, {"Moment-y", "(kN*m)"}, {"Moment-z", "(kN*m)"}};
  b.rows.push_back({num(7), txt("SPECX(RS)"), txt("I[1]"), num(50), num(8), num(0), num(0), num(0), num(30)});
  b.rows.push_back({num(7), txt("SPECX(RS)"), txt("J[2]"), num(50), num(8), num(0), num(0), num(0), num(12)});
  b.rows.push_back({num(7), txt("DL"), txt("I[1]"), num(100), num(0), num(0), num(0), num(0), num(0)}); // tension, signed rule
  const auto c = matchBeamForces(results, b, ToleranceSettings{});
  for (const char* comp : {"Axial", "Shear-y", "Moment-z"}) {
    EXPECT_EQ(rowOf(c, "SPECX(RS)", comp, "I").result.status, ComparisonStatus::Pass) << comp;
    EXPECT_EQ(rowOf(c, "SPECX(RS)", comp, "J").result.status, ComparisonStatus::Pass) << comp;
  }
  // The ordinary case still goes through the MIDAS sign convention (f_I = -N): -100 -> +100 PASS.
  EXPECT_EQ(rowOf(c, "DL", "Axial", "I").result.status, ComparisonStatus::Pass);
}
