// 2026-11-23 -- CLI command `inspect` (kopruq-model-view v1, schemas/model_view.schema.json).
// Dedicated new file. Runs the real command in-process through runCli with real files (UTF-8 folder).
#include <gtest/gtest.h>
#include <atomic>
#include <filesystem>
#include <random>
#include <fstream>
#include <set>
#include <sstream>
#include "kopruq/io/Cli.hpp"

using nlohmann::json;
namespace {
const std::string kDir = "kopruq_inspect_\xC3\xB6\xC5\x9F\xC4\xB1"; // ..._öşı
struct Dir {
  std::filesystem::path path; std::string utf8;
  // unique per instance: ctest runs the tests of this file as parallel processes
  static std::string unique() { static std::atomic<int> n{0}; std::random_device rd; return kDir + "_" + std::to_string(rd()) + "_" + std::to_string(n++); }
  Dir() { path = std::filesystem::temp_directory_path() / kopruq::io::utf8Path(unique()); std::filesystem::create_directories(path); auto u = path.u8string(); utf8.assign(u.begin(), u.end()); }
  ~Dir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
  std::string write(const std::string& name, const std::string& text) { std::ofstream(path / name, std::ios::binary) << text; return utf8 + "/" + name; }
};
const char* kNative = R"({"schema":"kopruq-model","schema_version":1,
  "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":4,"y":1,"z":3}],
  "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3}],
  "sections":[{"id":1,"area":0.02,"Iy":1e-4,"Iz":2e-4,"J":3e-4}],
  "beams":[{"id":7,"nodes":[1,2],"material":1,"section":1}],
  "supports":[{"node":1,"restraints":[true,true,true,true,true,true]}],
  "load_cases":[{"name":"c","nodal":[{"node":2,"values":[0,0,-1000,0,0,0]}]}]})";
const char* kMidas = R"json({"DBG":{
  "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
  "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":0.0,"Y":0.0,"Z":5.0},"3":{"X":4.0,"Y":0.0,"Z":5.0}},
  "MATL":{"1":{"TYPE":"STEEL","NAME":"S355","PARAM":[{"P_TYPE":1,"STANDARD":"EN05(S)","CODE":"","DB":"S355"}]}},
  "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"R","SECT_BEFORE":{"SHAPE":"SB","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[1.0,0.5]}}},
          "2":{"SECTTYPE":"DBUSER","SECT_NAME":"B","SECT_BEFORE":{"SHAPE":"B","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[1.0,0.8,0.1,0.1]}}}},
  "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0},
          "2":{"TYPE":"BEAM","MATL":1,"SECT":2,"NODE":[2,3,0,0,0,0,0,0],"ANGLE":0.0}},
  "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}}}})json";
int run(const std::vector<std::string>& a, std::string* out = nullptr, std::string* err = nullptr) {
  std::ostringstream o, e; const int rc = kopruq::io::runCli(a, o, e); if (out) *out = o.str(); if (err) *err = e.str(); return rc;
}
}

TEST(InspectCommand, NativeModelRoundTripsAndHasEmptyImportAndOutlines) {
  Dir d; const auto m = d.write("model.json", kNative);
  std::string out; ASSERT_EQ(run({"inspect", m, "-o", d.utf8 + "/view.json"}), 0);
  std::ifstream f(d.path / "view.json", std::ios::binary); const json v = json::parse(f);
  EXPECT_EQ(v["schema"], "kopruq-model-view"); EXPECT_EQ(v["schema_version"], 1); EXPECT_EQ(v["metadata"]["source_format"], "native");
  // the embedded model is exactly what the existing writer produces for the same input
  EXPECT_EQ(v["model"], kopruq::io::writeModel(kopruq::io::readModelText(kNative)));
  EXPECT_TRUE(v["section_outlines"].empty());
  EXPECT_TRUE(v["import"]["assumptions"].empty() && v["import"]["rejected"].empty());
  EXPECT_EQ(v["bounds"]["max"], json::array({4.0, 1.0, 3.0}));
}

TEST(InspectCommand, MidasModelCarriesOutlinesAndStructuredImportReport) {
  Dir d; const auto m = d.write("m.json", kMidas);
  std::string out; ASSERT_EQ(run({"inspect", m, "--format", "midas-nx-json"}, &out), 0);
  const json v = json::parse(out);
  EXPECT_EQ(v["metadata"]["source_format"], "midas-nx-json");
  ASSERT_EQ(v["section_outlines"].size(), 2u);
  const auto& box = v["section_outlines"][1];
  EXPECT_EQ(box["section"], 2); EXPECT_EQ(box["contours"].size(), 2u); // outer + hole
  EXPECT_EQ(v["section_outlines"][0]["contours"][0].size(), 4u);        // rectangle
  EXPECT_EQ(v["import"]["counts"]["beams"], 2); EXPECT_EQ(v["import"]["counts"]["nodes"], 3);
  EXPECT_FALSE(v["import"]["assumptions"].empty());                    // material assumptions are disclosed
  EXPECT_EQ(v["model"]["beams"].size(), 2u);
}

TEST(InspectCommand, DocumentMatchesTheSchemaKeys) {
  Dir d; const auto m = d.write("m.json", kMidas);
  std::string out; ASSERT_EQ(run({"inspect", m, "--format", "midas-nx-json"}, &out), 0);
  const json v = json::parse(out);
  std::ifstream sf(std::string(KOPRUQ_SOURCE_DIR) + "/schemas/model_view.schema.json"); ASSERT_TRUE(sf.good());
  const json s = json::parse(sf);
  std::set<std::string> props, keys, req;
  for (auto it = s["properties"].begin(); it != s["properties"].end(); ++it) props.insert(it.key());
  for (auto it = v.begin(); it != v.end(); ++it) keys.insert(it.key());
  for (const auto& r : s["required"]) req.insert(r.get<std::string>());
  for (const auto& k : keys) EXPECT_TRUE(props.count(k)) << k << " is not in the schema";
  for (const auto& k : req) EXPECT_TRUE(keys.count(k)) << k << " is required";
  EXPECT_EQ(v["metadata"].size(), s["properties"]["metadata"]["properties"].size());
}

TEST(InspectCommand, ExitCodesAndExistingCommandsUnchanged) {
  Dir d; const auto bad = d.write("bad.json", "{not json");
  EXPECT_EQ(run({"inspect", d.utf8 + "/missing.json"}), 4);
  EXPECT_EQ(run({"inspect", bad}), 3);
  EXPECT_EQ(run({"inspect", bad, "--format", "midas-nx-json"}), 3);
  EXPECT_EQ(run({"inspect"}), 2);
  EXPECT_EQ(run({"inspect", bad, "--bogus"}), 2);
  // `solve` on the same native model is unaffected by the new command
  const auto m = d.write("model.json", kNative); std::string out;
  ASSERT_EQ(run({"solve", m}, &out), 0);
  EXPECT_EQ(json::parse(out)["schema"], "kopruq-results");
}
