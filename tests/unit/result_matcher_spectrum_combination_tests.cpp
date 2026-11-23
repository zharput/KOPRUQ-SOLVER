// 2026-11-23 -- ResultMatcher::appendSpectrumCombinations: static + response-spectrum
// combinations (MIDAS LCOM-GEN with RS terms). Dedicated new file (GUI build). The rule is MIDAS's,
// read from its own results of verification model 100 (EQX = SW + SDL-1 + SDL-2 + |SPX|,
// SXY = 0.5 |SPX| + 0.3 |SPY|, beam forces in MIDAS's internal-force convention); the expected
// values below are hand sums of the fixture numbers.
#include <gtest/gtest.h>
#include "ResultMatcher.h"

using namespace kopruq::verification;
using Json = nlohmann::json;

namespace {
CellValue num(double v) { CellValue c; c.isEmpty = false; c.isNumber = true; c.number = v; c.text = std::to_string(v); return c; }
CellValue txt(std::string s) { CellValue c; c.isEmpty = false; c.text = std::move(s); return c; }
Json results() {
  return Json::parse(R"({"load_cases":[
    {"name":"SW","status":{"success":true},
     "nodes":[{"id":3,"displacement":[0.001,0,-0.002,0,0.0005,0],"reaction":[0,0,0,0,0,0]}],
     "elements":[{"id":7,"local_end_forces":[-100,0,20,0,30,0, 100,0,-20,0,10,0]}]},
    {"name":"SDL","status":{"success":true},
     "nodes":[{"id":3,"displacement":[0.0005,0,-0.001,0,0.0002,0],"reaction":[0,0,0,0,0,0]}],
     "elements":[{"id":7,"local_end_forces":[-50,0,10,0,15,0, 50,0,-10,0,5,0]}]}],
    "combinations":[],
    "spectrum_combinations":[
      {"name":"EQX","static_terms":[{"case":"SW","factor":1.0},{"case":"SDL","factor":2.0}],"spectrum_terms":[{"spectrum":"SPX","factor":1.0}]},
      {"name":"SXY","static_terms":[],"spectrum_terms":[{"spectrum":"SPX","factor":0.5},{"spectrum":"SPY","factor":0.3}]},
      {"name":"MISSING","static_terms":[{"case":"NOPE","factor":1.0}],"spectrum_terms":[]}]})");
}
Json spectrum() {
  return Json::parse(R"({"cases":[
    {"name":"SPX","status":{"success":true},
     "combined_nodes":[{"id":3,"displacement":[0.0004,0.0001,0.00002,0,0.00003,0],"reaction":[0,0,0,0,0,0]}],
     "combined_elements":[{"id":7,"local_end_forces":[40,5,6,1,8,2, 40,5,6,1,3,2],"global_end_forces":[0,0,0,0,0,0,0,0,0,0,0,0]}]},
    {"name":"SPY","status":{"success":true},
     "combined_nodes":[{"id":3,"displacement":[0.0001,0.002,0,0.0001,0,0.00005],"reaction":[0,0,0,0,0,0]}],
     "combined_elements":[{"id":7,"local_end_forces":[10,30,1,4,2,9, 10,30,1,4,1,9],"global_end_forces":[0,0,0,0,0,0,0,0,0,0,0,0]}]}]})");
}
const ComparisonRow& rowOf(const CategoryComparison& c, const std::string& lc, const std::string& comp, const std::string& loc = "") {
  for (const auto& r : c.rows) if (r.loadCase == lc && r.component == comp && r.location == loc) return r;
  throw std::runtime_error("row not found");
}
}

TEST(ResultMatcherSpectrumCombination, NodeValuesAreStaticPlusSpectrumMagnitude) {
  Json r = results();
  appendSpectrumCases(r, spectrum());
  appendSpectrumCombinations(r);
  ReferenceTable d;
  d.kind = ReferenceFileKind::Displacements;
  d.columns = {{"Node", ""}, {"Load", ""}, {"DX", "(m)"}, {"DY", "(m)"}, {"DZ", "(m)"}, {"RX", "([rad])"}, {"RY", "([rad])"}, {"RZ", "([rad])"}};
  // EQX: DX = 0.001 + 2*0.0005 + 0.0004 = 0.0024; DZ = -0.002 + 2*(-0.001) + 0.00002 = -0.00398;
  //      RY = 0.0005 + 2*0.0002 + 0.00003 = 0.00093. SXY: DY = 0.5*0.0001 + 0.3*0.002 = 0.00065.
  d.rows.push_back({num(3), txt("EQX"), num(0.0024), num(0.0001), num(-0.00398), num(0), num(0.00093), num(0)});
  d.rows.push_back({num(3), txt("SXY"), num(0.00023), num(0.00065), num(0.00001), num(0.00003), num(0.000015), num(0.000015)});
  d.rows.push_back({num(3), txt("MISSING"), num(1), num(0), num(0), num(0), num(0), num(0)});
  const auto c = matchDisplacements(r, d, ToleranceSettings{});
  for (const char* comp : {"DX", "DY", "DZ", "RX", "RY", "RZ"}) {
    EXPECT_EQ(rowOf(c, "EQX", comp).result.status, ComparisonStatus::Pass) << "EQX " << comp;
    EXPECT_EQ(rowOf(c, "SXY", comp).result.status, ComparisonStatus::Pass) << "SXY " << comp;
  }
  EXPECT_NEAR(rowOf(c, "EQX", "DZ").result.solverValue, -0.00398, 1e-15) << "signed static + |RS|, not a magnitude";
  EXPECT_EQ(rowOf(c, "MISSING", "DX").result.status, ComparisonStatus::NotApplicable);
}

TEST(ResultMatcherSpectrumCombination, BeamForcesAddMagnitudeInMidasConvention) {
  Json r = results();
  appendSpectrumCases(r, spectrum());
  appendSpectrumCombinations(r);
  ReferenceTable b;
  b.kind = ReferenceFileKind::BeamForce;
  b.columns = {{"Elem", ""}, {"Load", ""}, {"Part", ""}, {"Axial", "(kN)"}, {"Shear-y", "(kN)"}, {"Shear-z", "(kN)"},
               {"Torsion", "(kN*m)"}, {"Moment-y", "(kN*m)"}, {"Moment-z", "(kN*m)"}};
  // MIDAS convention of the static parts (flip at I for Axial/Shear/Torsion/Moment-z, at J for
  // Moment-y only): I end Axial = -(-100) + 2*(-(-50)) = 200, Shear-z = -(20) + 2*(-(10)) = -40,
  // Moment-y = 30 + 2*15 = 60; J end Axial = 100 + 2*50 = 200, Shear-z = -20 + 2*(-10) = -40,
  // Moment-y = -(10) + 2*(-(5)) = -20. EQX adds |SPX| to each:
  // I: Axial 240, Shear-z -34, Moment-y 68; J: Axial 240, Shear-z -34, Moment-y -17.
  b.rows.push_back({num(7), txt("EQX"), txt("I[3]"), num(240), num(5), num(-34), num(1), num(68), num(2)});
  b.rows.push_back({num(7), txt("EQX"), txt("J[4]"), num(240), num(5), num(-34), num(1), num(-17), num(2)});
  // SXY: 0.5|SPX| + 0.3|SPY| -> I: Axial 23, Shear-y 11.5, Moment-y 4.6; J: Moment-y 1.8.
  b.rows.push_back({num(7), txt("SXY"), txt("I[3]"), num(23), num(11.5), num(3.3), num(1.7), num(4.6), num(3.7)});
  b.rows.push_back({num(7), txt("SXY"), txt("J[4]"), num(23), num(11.5), num(3.3), num(1.7), num(1.8), num(3.7)});
  const auto c = matchBeamForces(r, b, ToleranceSettings{});
  for (const auto& row : c.rows) EXPECT_EQ(row.result.status, ComparisonStatus::Pass) << row.loadCase << " " << row.location << " " << row.component
                                                                                    << " solver " << row.result.solverValue << " midas " << row.result.referenceValue;
  EXPECT_EQ(c.rows.size(), 24u);
}
