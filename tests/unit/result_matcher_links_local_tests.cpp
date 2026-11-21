// 2026-11-21 -- ResultMatcher: MIDAS "Reaction(Local)" (matchLocalReactions) and "Elastic Link"
// (matchElasticLinks). Dedicated new file. Fixture data reproduce the shapes written by
// io::solutionJson (node "reaction_local" only for nodes with local axes; "springs" with 12 link
// end forces) and the MIDAS export layouts (verification model 23). The link-5 end forces are
// KOPRUQ's own model-23 values (UNIFORM: node2 end -6370.5, -786.4, -16828.6, -5.317, +2.663, +1.509)
// rounded to MIDAS's printed precision.
#include <gtest/gtest.h>
#include "ResultMatcher.h"

using namespace kopruq::verification;
using Json = nlohmann::json;

namespace {
CellValue num(double v) { CellValue c; c.isEmpty = false; c.isNumber = true; c.number = v; c.text = std::to_string(v); return c; }
CellValue txt(std::string s) { CellValue c; c.isEmpty = false; c.text = std::move(s); return c; }
const ComparisonRow& rowOf(const CategoryComparison& c, const std::string& id, const std::string& comp, const std::string& loc = "") {
  for (const auto& r : c.rows) if (r.id == id && r.component == comp && r.location == loc) return r;
  throw std::runtime_error("row not found");
}
Json results() {
  return Json::parse(R"({"load_cases":[{"name":"UNIFORM","status":{"success":true},
    "nodes":[{"id":63,"displacement":[0,0,0,0,0,0],"reaction":[1424.4065,0,10760.076,0,22790.504,0],
              "reaction_local":[1007.2075,-1007.2075,10760.076,16115.321,16115.321,0]},
             {"id":1,"displacement":[0,0,0,0,0,0],"reaction":[0,0,0,-517.87,0,0]}],
    "elements":[],
    "springs":[{"id":5,"local_end_forces":[6370.24,786.35,16827.86,5.31,-2.66,-1.51, -6370.24,-786.35,-16827.86,-5.31,2.66,1.51],
                "global_end_forces":[0,0,0,0,0,0,0,0,0,0,0,0]}]}]})");
}
}

TEST(ResultMatcherLinksLocal, LocalReactionsUseReactionLocalOrFallBackToGlobal) {
  ReferenceTable t;
  t.kind = ReferenceFileKind::ReactionLocal;
  t.columns = {{"Node", ""}, {"Load", ""}, {"Fx", "(kN)"}, {"Fy", "(kN)"}, {"Fz", "(kN)"}, {"Mx", "(kN*m)"}, {"My", "(kN*m)"}, {"Mz", "(kN*m)"}};
  t.rows.push_back({num(63), txt("UNIFORM"), num(1007.2075625), num(-1007.2075625), num(10760.076), num(16115.321), num(16115.321), num(0)});
  t.rows.push_back({num(1), txt("UNIFORM"), num(0), num(0), num(0), num(-517.8729), num(0), num(0)});
  const auto c = matchLocalReactions(results(), t, ToleranceSettings{});
  EXPECT_EQ(rowOf(c, "63", "Fx").result.status, ComparisonStatus::Pass);
  EXPECT_EQ(rowOf(c, "63", "Fy").result.status, ComparisonStatus::Pass);
  EXPECT_EQ(rowOf(c, "63", "Mx").result.status, ComparisonStatus::Pass);   // local, not the global MX = 0
  EXPECT_EQ(rowOf(c, "1", "Mx").result.status, ComparisonStatus::Pass);    // no local axes -> global reaction
  // The global matcher on the same table would compare the global FX 1424 with the local 1007 and fail.
  ReferenceTable g = t;
  g.kind = ReferenceFileKind::Reaction;
  g.columns = {{"Node", ""}, {"Load", ""}, {"FX", "(kN)"}, {"FY", "(kN)"}, {"FZ", "(kN)"}, {"MX", "(kN*m)"}, {"MY", "(kN*m)"}, {"MZ", "(kN*m)"}};
  EXPECT_EQ(rowOf(matchReactions(results(), g, ToleranceSettings{}), "63", "FX").result.status, ComparisonStatus::Fail);
}

TEST(ResultMatcherLinksLocal, ElasticLinkUsesNode2ForceWithMomentYFlipped) {
  // Model 23 link 5, case UNIFORM: MIDAS -6370.24 / -786.35 / -16827.86 / -5.31 / -2.66 / 1.51 on both rows.
  ReferenceTable t;
  t.kind = ReferenceFileKind::ElasticLink;
  t.columns = {{"No.", ""}, {"Load", ""}, {"Node", ""}, {"Axial", "(kN)"}, {"Shear-y", "(kN)"}, {"Shear-z", "(kN)"},
               {"Torsion", "(kN*m)"}, {"Moment-y", "(kN*m)"}, {"Moment-z", "(kN*m)"}};
  t.rows.push_back({num(5), txt("UNIFORM"), num(92), num(-6370.24), num(-786.35), num(-16827.86), num(-5.31), num(-2.66), num(1.51)});
  t.rows.push_back({num(5), txt("UNIFORM"), num(41), num(-6370.24), num(-786.35), num(-16827.86), num(-5.31), num(-2.66), num(1.51)});
  t.rows.push_back({num(9), txt("UNIFORM"), num(1), num(1.0), num(0), num(0), num(0), num(0), num(0)}); // no such link
  const auto c = matchElasticLinks(results(), t, ToleranceSettings{});
  for (const char* comp : {"Axial", "Shear-y", "Shear-z", "Torsion", "Moment-y", "Moment-z"}) {
    EXPECT_EQ(rowOf(c, "5", comp, "92").result.status, ComparisonStatus::Pass) << comp;
    EXPECT_EQ(rowOf(c, "5", comp, "41").result.status, ComparisonStatus::Pass) << comp;
  }
  EXPECT_DOUBLE_EQ(rowOf(c, "5", "Moment-y", "92").result.solverValue, -2.66); // node2 end +2.66, flipped
  EXPECT_EQ(rowOf(c, "9", "Axial", "1").result.status, ComparisonStatus::NotApplicable);
}
