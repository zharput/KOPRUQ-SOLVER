// 2026-11-23 -- static + response-spectrum combination definitions (ModelJson.hpp
// SpectrumCombination; MIDAS LCOM-GEN with ANAL="RS" terms, verification model 100).
// Dedicated new file: importer mapping and rejections, native JSON round trip, results JSON.
#include <gtest/gtest.h>
#include <string>
#include "kopruq/io/midas/MidasJsonImporter.hpp"
#include "kopruq/io/ResultsJson.hpp"

using namespace kopruq::io;

namespace {
nlohmann::json model() {
  return nlohmann::json::parse(R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":0.0,"Y":0.0,"Z":5.0}},
    "MATL":{"1":{"TYPE":"STEEL","NAME":"S355","PARAM":[{"P_TYPE":1,"STANDARD":"EN05(S)","CODE":"","DB":"S355"}]}},
    "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"R","SECT_BEFORE":{"SHAPE":"SB","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[0.5,0.5]}}}},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}},
    "STLD":{"1":{"NO":1,"NAME":"A","TYPE":"D"},"2":{"NO":2,"NAME":"B","TYPE":"D"}},
    "CNLD":{"2":{"ITEMS":[{"ID":1,"LCNAME":"A","FX":10.0,"FY":0.0,"FZ":0.0,"MX":0.0,"MY":0.0,"MZ":0.0},
                          {"ID":2,"LCNAME":"B","FX":0.0,"FY":5.0,"FZ":0.0,"MX":0.0,"MY":0.0,"MZ":0.0}]}},
    "SPFC":{"1":{"NAME":"F","GRAV":9.806,"DRATIO":0.05,"aFUNC":[{"PERIOD":0.0,"VALUE":0.1},{"PERIOD":5.0,"VALUE":0.1}]}},
    "SPLC":{"1":{"NAME":"SPX","DIR":"XY","ANGLE":0.0,"COMTYPE":"CQC","aFUNCNAME":["F"]}},
    "LCOM-GEN":{
      "1":{"NO":1,"NAME":"C1","ACTIVE":"ACTIVE","iTYPE":0,"vCOMB":[{"ANAL":"ST","LCNAME":"A","FACTOR":1.0},{"ANAL":"ST","LCNAME":"B","FACTOR":2.0}]},
      "2":{"NO":2,"NAME":"EQX","ACTIVE":"ACTIVE","iTYPE":0,"vCOMB":[{"ANAL":"CB","LCNAME":"C1","FACTOR":0.5},{"ANAL":"ST","LCNAME":"A","FACTOR":1.0},{"ANAL":"RS","LCNAME":"SPX","FACTOR":1.5}]},
      "3":{"NO":3,"NAME":"SXY","ACTIVE":"ACTIVE","iTYPE":0,"vCOMB":[{"ANAL":"RS","LCNAME":"SPX","FACTOR":0.3}]},
      "4":{"NO":4,"NAME":"BAD1","ACTIVE":"ACTIVE","iTYPE":0,"vCOMB":[{"ANAL":"RS","LCNAME":"NOPE","FACTOR":1.0}]},
      "5":{"NO":5,"NAME":"BAD2","ACTIVE":"ACTIVE","iTYPE":0,"vCOMB":[{"ANAL":"CB","LCNAME":"EQX","FACTOR":1.0},{"ANAL":"RS","LCNAME":"SPX","FACTOR":1.0}]},
      "6":{"NO":6,"NAME":"BAD3","ACTIVE":"ACTIVE","iTYPE":0,"vCOMB":[{"ANAL":"CB","LCNAME":"EQX","FACTOR":1.0}]}}}})json");
}
bool rejected(const midas::MidasImportResult& r, const std::string& name) {
  for (const auto& m : r.report.rejected) if (m.find("('" + name + "')") != std::string::npos) return true;
  return false;
}
}

TEST(SpectrumCombination, MidasRsCombinationsImportedAsDefinitions) {
  const auto r = midas::importMidasJson(model());
  ASSERT_EQ(r.input.combinations.size(), 1u);          // C1 (static only) stays an ordinary combination
  ASSERT_EQ(r.input.spectrum_combinations.size(), 2u); // EQX, SXY
  EXPECT_EQ(r.report.spectrum_combinations, 2u);
  const auto* eqx = &r.input.spectrum_combinations[0];
  if (eqx->name != "EQX") eqx = &r.input.spectrum_combinations[1];
  ASSERT_EQ(eqx->name, "EQX");
  // static part: 0.5 * C1 (= 0.5 A + 1.0 B) + 1.0 A = 1.5 A + 1.0 B; spectrum part: 1.5 SPX
  ASSERT_EQ(eqx->static_terms.size(), 2u);
  for (const auto& [idx, f] : eqx->static_terms) {
    const std::string& n = r.input.load_cases.at(idx).name;
    if (n == "A") EXPECT_DOUBLE_EQ(f, 1.5); else { EXPECT_EQ(n, "B"); EXPECT_DOUBLE_EQ(f, 1.0); }
  }
  ASSERT_EQ(eqx->spectrum_terms.size(), 1u);
  EXPECT_EQ(r.input.response_spectra.at(eqx->spectrum_terms[0].first).name, "SPX");
  EXPECT_DOUBLE_EQ(eqx->spectrum_terms[0].second, 1.5);
  EXPECT_TRUE(rejected(r, "BAD1"));  // unknown spectrum
  EXPECT_TRUE(rejected(r, "BAD2"));  // CB to a spectrum combination
  EXPECT_TRUE(rejected(r, "BAD3"));  // ordinary combination referencing a spectrum combination
}

TEST(SpectrumCombination, NativeRoundTripAndResultsDefinitions) {
  const auto r = midas::importMidasJson(model());
  const auto j = writeModel(r.input);
  ASSERT_TRUE(j.contains("spectrum_combinations"));
  const auto back = readModel(j);
  ASSERT_EQ(back.spectrum_combinations.size(), 2u);
  for (std::size_t i = 0; i < 2; ++i) {
    EXPECT_EQ(back.spectrum_combinations[i].name, r.input.spectrum_combinations[i].name);
    EXPECT_EQ(back.spectrum_combinations[i].static_terms.size(), r.input.spectrum_combinations[i].static_terms.size());
    EXPECT_EQ(back.spectrum_combinations[i].spectrum_terms, r.input.spectrum_combinations[i].spectrum_terms);
  }
  auto sol = kopruq::LinearStaticSolver::solveCases(r.input.model, r.input.restraints, r.input.load_cases, r.input.combinations);
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  const auto res = writeResults(r.input, sol, "0");
  ASSERT_TRUE(res.contains("spectrum_combinations"));
  EXPECT_EQ(res["spectrum_combinations"].size(), 2u);
  // A model without such combinations writes neither key (unchanged output).
  auto plain = r.input; plain.spectrum_combinations.clear();
  EXPECT_FALSE(writeModel(plain).contains("spectrum_combinations"));
  EXPECT_FALSE(writeResults(plain, sol, "0").contains("spectrum_combinations"));
}

TEST(SpectrumCombination, NativeReaderRejectsUnknownReferences) {
  const auto r = midas::importMidasJson(model());
  auto j = writeModel(r.input);
  j["spectrum_combinations"][0]["spectrum_terms"] = nlohmann::json::array({{{"spectrum", "NOPE"}, {"factor", 1.0}}});
  EXPECT_THROW(readModel(j), ModelInputError);
  j = writeModel(r.input);
  j["spectrum_combinations"][0]["static_terms"] = nlohmann::json::array({{{"case", "NOPE"}, {"factor", 1.0}}});
  EXPECT_THROW(readModel(j), ModelInputError);
  j = writeModel(r.input);
  j["spectrum_combinations"][0]["name"] = "C1"; // clashes with an ordinary combination
  EXPECT_THROW(readModel(j), ModelInputError);
}
