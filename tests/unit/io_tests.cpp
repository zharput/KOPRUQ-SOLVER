// Phase 4.2 — JSON model input, results output and CLI.
//
// The numerical solver is verified elsewhere; these tests verify that the
// I/O layer neither loses nor alters information: every field and default is
// read as specified, write -> read round trips are exact, JSON solves are
// bit-identical to direct API solves, results serialize doubles exactly,
// output is deterministic, every invalid input is reported with its JSON
// path, and the published schema matches the keys the reader accepts.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include "kopruq/io/Cli.hpp"

using kopruq::io::json;
using kopruq::io::ModelInput;
using kopruq::io::ModelInputError;

namespace {

std::string readFile(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
std::filesystem::path source(const char* rel) { return std::filesystem::path(KOPRUQ_SOURCE_DIR) / rel; }

json baseModel() {
  return json::parse(R"({
    "schema": "kopruq-model", "schema_version": 1,
    "units": {"length": "m", "force": "kN", "angle": "rad"},
    "nodes": [{"id": 1, "x": 0, "y": 0, "z": 0}, {"id": 2, "x": 4, "y": 0, "z": 0}],
    "materials": [{"id": 1, "elastic_modulus": 210e6, "poisson_ratio": 0.3, "unit_weight": 78.5}],
    "sections": [{"id": 1, "area": 0.3, "Iy": 0.02, "Iz": 0.04, "J": 0.01}],
    "beams": [{"id": 1, "nodes": [1, 2], "material": 1, "section": 1, "reference_y": [0, 0, 1]}],
    "supports": [{"node": 1, "restraints": [true, true, true, true, true, true]}],
    "load_cases": [
      {"name": "A", "member": [{"element": 1, "axes": "local", "direction": "y", "w1": -1}],
                    "nodal": [{"node": 2, "values": [0, 0, -1, 0, 0, 0]}]},
      {"name": "B", "self_weight": 1}],
    "combinations": [{"name": "C", "terms": [{"case": "A", "factor": 1.5}]}]
  })");
}

std::vector<kopruq::io::InputIssue> issuesOf(const json& doc) {
  try { kopruq::io::readModel(doc); }
  catch (const ModelInputError& e) { return e.issues; }
  return {};
}

std::vector<std::string> sorted(std::vector<std::string> v) { std::sort(v.begin(), v.end()); return v; }

ModelInput apiModel() {
  using kopruq::LoadAxes; using kopruq::MemberLoad;
  ModelInput in;
  in.model.nodes = {{1, 0, 0, 0}, {2, 3, 0, 4}, {3, 7, 1, 4}};
  in.model.materials = {{1, 32e6, 0.2, 25.0}};
  in.model.sections = {{1, 0.16, 0.0021, 0.0034, 0.0036}, {2, 0.18, 0.00135, 0.0054, 0.0037}};
  in.model.beams = {{1, 1, 2, 1, 1, {0, 1, 0}}, {2, 2, 3, 1, 2, {0, 0, 1}}};
  in.restraints = {{1, {true, true, true, true, true, true}}, {3, {false, true, true, false, false, false}}};
  kopruq::LoadCase g; g.name = "G"; g.self_weight = 1.0;
  kopruq::LoadCase q; q.name = "Q";
  q.member = {MemberLoad{2, LoadAxes::Global, 2, -12.5, -7.25, 0.1, 0.9, true}, MemberLoad{1, LoadAxes::Local, 1, 3.0, 3.0, 0, 1, false}};
  q.nodal = {{2, (Eigen::Matrix<double, 6, 1>() << 1.5, -2, 0, 0.3, 0, -0.7).finished()}};
  in.load_cases = {g, q};
  in.combinations = {{"ULS", {{0, 1.35}, {1, 1.5}}}};
  return in;
}

} // namespace

TEST(Sha256, KnownVectors) {
  using kopruq::io::sha256Hex;
  EXPECT_EQ(sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  EXPECT_EQ(sha256Hex(std::string(1000000, 'a')), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(ModelJson, ReadsAllFieldsAndDefaults) {
  auto in = kopruq::io::readModel(baseModel());
  ASSERT_EQ(in.model.nodes.size(), 2u);
  EXPECT_EQ(in.model.nodes[1].id, 2u); EXPECT_EQ(in.model.nodes[1].x, 4.0);
  ASSERT_EQ(in.model.materials.size(), 1u);
  EXPECT_EQ(in.model.materials[0].elastic_modulus, 210e6); EXPECT_EQ(in.model.materials[0].unit_weight, 78.5);
  ASSERT_EQ(in.model.sections.size(), 1u); EXPECT_EQ(in.model.sections[0].Iz, 0.04);
  ASSERT_EQ(in.model.beams.size(), 1u);
  EXPECT_EQ(in.model.beams[0].node2, 2u); EXPECT_EQ(in.model.beams[0].reference_y, Eigen::Vector3d(0, 0, 1));
  ASSERT_EQ(in.restraints.count(1), 1u); EXPECT_TRUE(in.restraints.at(1)[5]);
  ASSERT_EQ(in.load_cases.size(), 2u);
  const auto& ml = in.load_cases[0].member.at(0);
  EXPECT_EQ(ml.axes, kopruq::LoadAxes::Local); EXPECT_EQ(ml.direction, 1);
  EXPECT_EQ(ml.w1, -1.0); EXPECT_EQ(ml.w2, -1.0);           // w2 defaults to w1
  EXPECT_EQ(ml.start, 0.0); EXPECT_EQ(ml.end, 1.0); EXPECT_FALSE(ml.projected);
  EXPECT_EQ(in.load_cases[0].nodal.at(0).values(2), -1.0);
  EXPECT_EQ(in.load_cases[0].self_weight, 0.0);              // default
  EXPECT_EQ(in.load_cases[1].self_weight, 1.0);
  ASSERT_EQ(in.combinations.size(), 1u);
  EXPECT_EQ(in.combinations[0].terms.at(0).first, 0u); EXPECT_EQ(in.combinations[0].terms.at(0).second, 1.5);
  // Defaults when optional keys are absent.
  auto doc = baseModel();
  doc.erase("units"); doc.erase("load_cases"); doc.erase("combinations");
  doc["materials"][0].erase("unit_weight"); doc["beams"][0].erase("reference_y");
  auto d = kopruq::io::readModel(doc);
  EXPECT_EQ(d.model.materials[0].unit_weight, 0.0);
  EXPECT_EQ(d.model.beams[0].reference_y, Eigen::Vector3d(0, 1, 0));
  EXPECT_TRUE(d.load_cases.empty()); EXPECT_TRUE(d.combinations.empty());
}

TEST(ModelJson, WriteReadRoundTripIsExact) {
  auto in = apiModel();
  json first = kopruq::io::writeModel(in);
  auto back = kopruq::io::readModelText(first.dump());
  EXPECT_EQ(kopruq::io::writeModel(back), first);
  EXPECT_EQ(back.model.materials[0].unit_weight, 25.0);
  EXPECT_TRUE(back.load_cases[1].member[0].projected);
  EXPECT_EQ(back.load_cases[1].member[0].w2, -7.25);
}

TEST(ModelJson, ReportsEveryIssueOfInvalidFileWithPaths) {
  try {
    kopruq::io::readModelText(readFile(source("tests/data/invalid_model.json")));
    FAIL() << "expected ModelInputError";
  } catch (const ModelInputError& e) {
    ASSERT_EQ(e.issues.size(), 3u) << e.what();
    EXPECT_EQ(e.issues[0].path, "$.nodes[1]");                EXPECT_NE(e.issues[0].message.find("'z'"), std::string::npos);
    EXPECT_EQ(e.issues[1].path, "$.materials[0].unit_weigth"); EXPECT_EQ(e.issues[1].message, "unknown key");
    EXPECT_EQ(e.issues[2].path, "$.beams[0].nodes[1]");       EXPECT_NE(e.issues[2].message.find("unknown node 3"), std::string::npos);
  }
}

TEST(ModelJson, RejectsInvalidValuesWithPaths) {
  struct Case { const char* name; std::function<void(json&)> mutate; const char* path; const char* text; };
  const std::vector<Case> cases = {
    {"schema name", [](json& d){ d["schema"] = "other"; }, "$.schema", "kopruq-model"},
    {"schema version", [](json& d){ d["schema_version"] = 2; }, "$.schema_version", "unsupported version"},
    {"units", [](json& d){ d["units"]["length"] = "mm"; }, "$.units.length", "no unit conversion"},
    {"unknown top key", [](json& d){ d["foo"] = 1; }, "$.foo", "unknown key"},
    {"no nodes", [](json& d){ d["nodes"] = json::array(); d["beams"] = json::array(); d["supports"] = json::array(); d.erase("load_cases"); d.erase("combinations"); }, "$.nodes", "at least one node"},
    {"number type", [](json& d){ d["nodes"][0]["x"] = "0"; }, "$.nodes[0].x", "must be a number"},
    {"zero id", [](json& d){ d["nodes"][0]["id"] = 0; }, "$.nodes[0].id", "positive integer"},
    {"fractional id", [](json& d){ d["nodes"][0]["id"] = 1.5; }, "$.nodes[0].id", "positive integer"},
    {"negative id", [](json& d){ d["nodes"][0]["id"] = -1; }, "$.nodes[0].id", "positive integer"},
    {"duplicate node", [](json& d){ d["nodes"][1]["id"] = 1; }, "$.nodes[1].id", "duplicate node id"},
    {"missing key", [](json& d){ d["nodes"][0].erase("z"); }, "$.nodes[0]", "missing required key 'z'"},
    {"typo key", [](json& d){ d["materials"][0]["unit_weigth"] = 1; }, "$.materials[0].unit_weigth", "unknown key"},
    {"poisson", [](json& d){ d["materials"][0]["poisson_ratio"] = 0.5; }, "$.materials[0].poisson_ratio", "poisson_ratio < 0.5"},
    {"unit weight", [](json& d){ d["materials"][0]["unit_weight"] = -1; }, "$.materials[0].unit_weight", ">= 0"},
    {"section", [](json& d){ d["sections"][0]["Iy"] = 0; }, "$.sections[0].Iy", "greater than 0"},
    {"unknown material", [](json& d){ d["beams"][0]["material"] = 9; }, "$.beams[0].material", "unknown material 9"},
    {"same node", [](json& d){ d["beams"][0]["nodes"] = json::array({1, 1}); }, "$.beams[0].nodes", "two different nodes"},
    {"one node", [](json& d){ d["beams"][0]["nodes"] = json::array({1}); }, "$.beams[0].nodes", "array of 2"},
    {"parallel reference", [](json& d){ d["beams"][0]["reference_y"] = json::array({1, 0, 0}); }, "$.beams[0]", "invalid geometry"},
    {"restraints length", [](json& d){ d["supports"][0]["restraints"] = json::array({true, true, true, true, true}); }, "$.supports[0].restraints", "6 booleans"},
    {"duplicate support", [](json& d){ d["supports"].push_back(d["supports"][0]); }, "$.supports[1].node", "duplicate support"},
    {"axes", [](json& d){ d["load_cases"][0]["member"][0]["axes"] = "loc"; }, "$.load_cases[0].member[0].axes", "\"local\" or \"global\""},
    {"direction", [](json& d){ d["load_cases"][0]["member"][0]["direction"] = "w"; }, "$.load_cases[0].member[0].direction", "\"x\", \"y\" or \"z\""},
    {"segment", [](json& d){ auto& m = d["load_cases"][0]["member"][0]; m["start"] = 0.6; m["end"] = 0.6; }, "$.load_cases[0].member[0]", "0 <= start < end <= 1"},
    {"projected local", [](json& d){ d["load_cases"][0]["member"][0]["projected"] = true; }, "$.load_cases[0].member[0].projected", "only allowed"},
    {"unknown beam", [](json& d){ d["load_cases"][0]["member"][0]["element"] = 9; }, "$.load_cases[0].member[0].element", "unknown beam 9"},
    {"nodal values", [](json& d){ d["load_cases"][0]["nodal"][0]["values"] = json::array({0, 0, 0, 0, 0}); }, "$.load_cases[0].nodal[0].values", "array of 6"},
    {"duplicate case", [](json& d){ d["load_cases"][1]["name"] = "A"; }, "$.load_cases[1].name", "duplicate load case name"},
    {"unknown case", [](json& d){ d["combinations"][0]["terms"][0]["case"] = "X"; }, "$.combinations[0].terms[0].case", "unknown load case"},
    {"empty terms", [](json& d){ d["combinations"][0]["terms"] = json::array(); }, "$.combinations[0].terms", "non-empty"},
  };
  ASSERT_TRUE(issuesOf(baseModel()).empty());
  for (const auto& c : cases) {
    json doc = baseModel();
    c.mutate(doc);
    auto issues = issuesOf(doc);
    bool found = std::any_of(issues.begin(), issues.end(), [&](const auto& i) {
      return i.path == c.path && i.message.find(c.text) != std::string::npos; });
    std::string all; for (const auto& i : issues) all += "\n  " + i.path + ": " + i.message;
    EXPECT_TRUE(found) << c.name << ": expected " << c.path << " ~ '" << c.text << "', got:" << all;
  }
}

TEST(ModelJson, MalformedJsonIsReported) {
  try { kopruq::io::readModelText("{\"schema\": \"kopruq-model\", "); FAIL(); }
  catch (const ModelInputError& e) {
    ASSERT_EQ(e.issues.size(), 1u);
    EXPECT_EQ(e.issues[0].path, "$");
    EXPECT_NE(e.issues[0].message.find("invalid JSON"), std::string::npos);
  }
}

TEST(ModelJson, SchemaFileMatchesReaderKeys) {
  json schema = json::parse(readFile(source("schemas/model.schema.json")));
  auto check = [&](const json& node, const kopruq::io::KeySpec& spec, const std::string& name) {
    std::vector<std::string> props, req;
    for (auto it = node.at("properties").begin(); it != node.at("properties").end(); ++it) props.push_back(it.key());
    for (const auto& r : node.at("required")) req.push_back(r.get<std::string>());
    std::vector<std::string> all = spec.required; all.insert(all.end(), spec.optional.begin(), spec.optional.end());
    EXPECT_EQ(sorted(props), sorted(all)) << name << " properties";
    EXPECT_EQ(sorted(req), sorted(spec.required)) << name << " required";
    EXPECT_EQ(node.at("additionalProperties"), false) << name;
  };
  namespace k = kopruq::io::model_keys;
  check(schema, k::top, "top");
  const std::vector<std::pair<const char*, const kopruq::io::KeySpec*>> defs = {
    {"units", &k::units}, {"node", &k::node}, {"material", &k::material}, {"section", &k::section}, {"beam", &k::beam},
    {"support", &k::support}, {"load_case", &k::load_case}, {"nodal_load", &k::nodal_load}, {"member_load", &k::member_load},
    {"combination", &k::combination}, {"combination_term", &k::combination_term}, {"point_load", &k::point_load}};
  EXPECT_EQ(schema.at("$defs").size(), defs.size());
  for (const auto& [name, spec] : defs) check(schema.at("$defs").at(name), *spec, name);
  EXPECT_EQ(schema.at("properties").at("schema_version").at("const"), kopruq::io::model_schema_version);
  json results = json::parse(readFile(source("schemas/results.schema.json")));
  EXPECT_EQ(results.at("properties").at("schema_version").at("const"), kopruq::io::results_schema_version);
}

TEST(ResultsJson, JsonPipelineIsBitIdenticalToDirectApi) {
  auto in = apiModel();
  auto direct = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
  ASSERT_TRUE(direct.status.success) << direct.status.message;
  auto viaJson = kopruq::io::readModelText(kopruq::io::writeModel(in).dump());
  auto r = kopruq::LinearStaticSolver::solveCases(viaJson.model, viaJson.restraints, viaJson.load_cases, viaJson.combinations);
  ASSERT_TRUE(r.status.success);
  ASSERT_EQ(r.cases.size(), direct.cases.size()); ASSERT_EQ(r.combinations.size(), direct.combinations.size());
  for (size_t i = 0; i < r.cases.size(); ++i) EXPECT_EQ((r.cases[i].global - direct.cases[i].global).cwiseAbs().maxCoeff(), 0.0) << "case " << i;
  for (size_t i = 0; i < r.combinations.size(); ++i) EXPECT_EQ((r.combinations[i].reactions - direct.combinations[i].reactions).cwiseAbs().maxCoeff(), 0.0);
}

TEST(ResultsJson, ExampleModelsSolveAndSerializeExactly) {
  for (const char* file : {"examples/cantilever.json", "examples/portal_frame.json"}) {
    SCOPED_TRACE(file);
    const std::string text = readFile(source(file));
    auto in = kopruq::io::readModelText(text);
    auto r = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
    ASSERT_TRUE(r.status.success) << r.status.message;
    json out = json::parse(kopruq::io::writeResults(in, r, kopruq::io::sha256Hex(text)).dump(2));
    EXPECT_EQ(out["schema"], "kopruq-results");
    EXPECT_EQ(out["metadata"]["model_sha256"], kopruq::io::sha256Hex(text));
    EXPECT_EQ(out["metadata"]["solver_version"], kopruq::version);
    ASSERT_EQ(out["load_cases"].size(), r.cases.size());
    ASSERT_EQ(out["combinations"].size(), r.combinations.size());
    auto compare = [&](const json& js, const kopruq::StaticSolution& s) {
      EXPECT_TRUE(js["status"]["success"].get<bool>());
      EXPECT_LT(s.results.force_equilibrium_error, 1e-12);
      EXPECT_LT(s.results.moment_equilibrium_error, 1e-12);
      for (size_t n = 0; n < s.results.nodes.size(); ++n)
        for (int d = 0; d < 6; ++d) {
          EXPECT_EQ(js["nodes"][n]["displacement"][d].get<double>(), s.results.nodes[n].displacement[d]);
          EXPECT_EQ(js["nodes"][n]["reaction"][d].get<double>(), s.results.nodes[n].reaction[d]);
        }
      for (size_t e = 0; e < s.results.elements.size(); ++e)
        for (int d = 0; d < 12; ++d) EXPECT_EQ(js["elements"][e]["global_end_forces"][d].get<double>(), s.results.elements[e].global_end_force[d]);
      EXPECT_EQ(js["diagnostics"]["relative_residual"].get<double>(), s.relative_residual);
    };
    for (size_t i = 0; i < r.cases.size(); ++i) compare(out["load_cases"][i], r.cases[i]);
    for (size_t i = 0; i < r.combinations.size(); ++i) compare(out["combinations"][i], r.combinations[i]);
  }
}

TEST(ResultsJson, OutputIsDeterministic) {
  const std::string text = readFile(source("examples/portal_frame.json"));
  auto once = [&] {
    auto in = kopruq::io::readModelText(text);
    auto r = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
    return kopruq::io::writeResults(in, r, kopruq::io::sha256Hex(text)).dump(2);
  };
  EXPECT_EQ(once(), once());
}

TEST(ResultsJson, FailedCaseCarriesOnlyStatus) {
  auto doc = baseModel();
  doc["supports"] = json::array();
  auto in = kopruq::io::readModel(doc);
  auto r = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
  json out = kopruq::io::writeResults(in, r, "0");
  EXPECT_EQ(out["status"]["code"], "NoRestraints");
  EXPECT_EQ(out["load_cases"][0]["status"]["success"], false);
  EXPECT_FALSE(out["load_cases"][0].contains("nodes"));
  EXPECT_FALSE(out["load_cases"][0].contains("diagnostics"));
}

TEST(Cli, ExitCodesAndOutputs) {
  auto run = [](std::vector<std::string> args, std::string* outText = nullptr, std::string* errText = nullptr) {
    std::ostringstream out, err;
    int code = kopruq::io::runCli(args, out, err);
    if (outText) *outText = out.str();
    if (errText) *errText = err.str();
    return code;
  };
  std::string out, err;
  EXPECT_EQ(run({"--version"}, &out), 0); EXPECT_EQ(out, std::string("KOPRUQ-SOLVER ") + kopruq::version + "\n");
  EXPECT_EQ(run({"--help"}, &out), 0);    EXPECT_NE(out.find("solve <model.json>"), std::string::npos);
  EXPECT_EQ(run({}, &out), 0);
  EXPECT_EQ(run({"bogus"}, nullptr, &err), 2); EXPECT_EQ(err, "Unknown command: bogus\n");
  EXPECT_EQ(run({"solve"}, nullptr, &err), 2);
  EXPECT_EQ(run({"solve", "a.json", "b.json"}), 2);
  EXPECT_EQ(run({"solve", "a.json", "-o"}), 2);
  EXPECT_EQ(run({"solve", source("does/not/exist.json").string()}, nullptr, &err), 4);

  EXPECT_EQ(run({"solve", source("tests/data/invalid_model.json").string()}, nullptr, &err), 3);
  EXPECT_NE(err.find("(3 issues)"), std::string::npos) << err;
  EXPECT_NE(err.find("$.materials[0].unit_weigth: unknown key"), std::string::npos) << err;

  const auto model = source("examples/cantilever.json");
  ASSERT_EQ(run({"solve", model.string()}, &out, &err), 0) << err;
  json results = json::parse(out);
  EXPECT_EQ(results["metadata"]["model_sha256"], kopruq::io::sha256Hex(readFile(model)));
  EXPECT_EQ(results["status"]["success"], true);

  const auto file = std::filesystem::temp_directory_path() / "kopruq_cli_test_results.json";
  std::filesystem::remove(file);
  ASSERT_EQ(run({"solve", model.string(), "-o", file.string()}, &out, &err), 0) << err;
  EXPECT_TRUE(out.empty());
  EXPECT_EQ(readFile(file), json::parse(readFile(file)).dump(2) + "\n");
  EXPECT_EQ(json::parse(readFile(file)), results);
  std::filesystem::remove(file);

  // A model that reads fine but cannot be analysed: results are still written, exit code 1.
  auto doc = baseModel(); doc["supports"] = json::array();
  const auto unsupported = std::filesystem::temp_directory_path() / "kopruq_cli_unsupported.json";
  { std::ofstream f(unsupported, std::ios::binary); f << doc.dump(); }
  EXPECT_EQ(run({"solve", unsupported.string()}, &out, &err), 1);
  EXPECT_EQ(json::parse(out)["status"]["code"], "NoRestraints");
  EXPECT_NE(err.find("analysis failed"), std::string::npos);
  std::filesystem::remove(unsupported);
}

TEST(ModelJson, AcceptsSignedPositiveIntegersBuiltInCxx) {
  // Regression: integers parsed from text are unsigned in nlohmann::json, but
  // values assigned from C++ ints are signed; both must be accepted as ids.
  json doc = baseModel();
  doc["schema_version"] = int{1};
  doc["nodes"][0]["id"] = int{1}; doc["nodes"][1]["id"] = int{2};
  doc["beams"][0]["nodes"] = json::array({int{1}, int{2}});
  doc["beams"][0]["material"] = int{1};
  doc["load_cases"][0]["member"][0]["element"] = int{1};
  ASSERT_TRUE(doc["nodes"][0]["id"].is_number_integer() && !doc["nodes"][0]["id"].is_number_unsigned());
  auto fromCxx = kopruq::io::readModel(doc);
  auto fromText = kopruq::io::readModelText(baseModel().dump());
  EXPECT_EQ(kopruq::io::writeModel(fromCxx), kopruq::io::writeModel(fromText));
}
