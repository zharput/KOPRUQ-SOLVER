// Phase 6.2b — JSON model input and CLI support for response spectrum analysis.
//
// Verifies the "response_spectra" model section round-trips exactly, that
// the JSON pipeline solve is bit-identical to the direct C++ API solve,
// that the writer loses no per-mode/combined-response information, that
// the published schema's key sets match the writer's and reader's actual
// behaviour, and the CLI's exit codes and files for the new "spectrum"
// command.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "kopruq/io/Cli.hpp"

using kopruq::io::json;

namespace {

std::string readFile(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
std::filesystem::path source(const char* rel) { return std::filesystem::path(KOPRUQ_SOURCE_DIR) / rel; }

json baseModel() {
  return json::parse(R"({
    "schema": "kopruq-model", "schema_version": 1,
    "nodes": [{"id": 1, "x": 0, "y": 0, "z": 0}, {"id": 2, "x": 4, "y": 0, "z": 0}, {"id": 3, "x": 8, "y": 0, "z": 0}],
    "materials": [{"id": 1, "elastic_modulus": 210e6, "poisson_ratio": 0.3, "unit_weight": 78.5}],
    "sections": [{"id": 1, "area": 0.3, "Iy": 0.02, "Iz": 0.04, "J": 0.01}],
    "beams": [{"id": 1, "nodes": [1, 2], "material": 1, "section": 1}, {"id": 2, "nodes": [2, 3], "material": 1, "section": 1}],
    "supports": [{"node": 1, "restraints": [true, true, true, true, true, true]}],
    "response_spectra": [
      {"name": "EQZ", "direction": "z", "num_modes": 4,
       "spectrum": [{"period": 1e-6, "sa": 9.81}, {"period": 1e6, "sa": 9.81}]}
    ]
  })");
}

} // namespace

TEST(ResponseSpectrumModelJson, ReadsAllFieldsAndDefaults) {
  auto in = kopruq::io::readModel(baseModel());
  ASSERT_EQ(in.response_spectra.size(), 1u);
  const auto& rc = in.response_spectra[0];
  EXPECT_EQ(rc.name, "EQZ");
  EXPECT_EQ(rc.input.direction, 2);
  EXPECT_EQ(rc.input.num_modes, 4);
  ASSERT_EQ(rc.input.spectrum.size(), 2u);
  EXPECT_EQ(rc.input.spectrum[0].period_s, 1e-6);
  EXPECT_EQ(rc.input.spectrum[1].spectral_acceleration, 9.81);

  // num_modes defaults to 6 when omitted.
  auto doc = baseModel();
  doc["response_spectra"][0].erase("num_modes");
  auto d = kopruq::io::readModel(doc);
  EXPECT_EQ(d.response_spectra[0].input.num_modes, 6);
}

TEST(ResponseSpectrumModelJson, WriteReadRoundTripIsExact) {
  auto in = kopruq::io::readModel(baseModel());
  json first = kopruq::io::writeModel(in);
  auto back = kopruq::io::readModelText(first.dump());
  EXPECT_EQ(kopruq::io::writeModel(back), first);
}

TEST(ResponseSpectrumModelJson, RejectsInvalidValuesWithPaths) {
  auto issues = [&](auto mutate) {
    json d = baseModel(); mutate(d);
    try { kopruq::io::readModel(d); } catch (const kopruq::io::ModelInputError& e) { return e.issues; }
    return std::vector<kopruq::io::InputIssue>{};
  };
  auto has = [](const std::vector<kopruq::io::InputIssue>& v, const std::string& path, const std::string& text) {
    return std::any_of(v.begin(), v.end(), [&](const auto& i) { return i.path == path && i.message.find(text) != std::string::npos; });
  };
  EXPECT_TRUE(has(issues([](json& d) { d["response_spectra"][0]["direction"] = "w"; }), "$.response_spectra[0].direction", "\"x\", \"y\" or \"z\""));
  EXPECT_TRUE(has(issues([](json& d) { d["response_spectra"][0]["name"] = ""; }), "$.response_spectra[0].name", "must not be empty"));
  EXPECT_TRUE(has(issues([](json& d) { d["response_spectra"][0]["num_modes"] = 0; }), "$.response_spectra[0].num_modes", "positive integer"));
  EXPECT_TRUE(has(issues([](json& d) { d["response_spectra"][0]["num_modes"] = 1.5; }), "$.response_spectra[0].num_modes", "positive integer"));
  EXPECT_TRUE(has(issues([](json& d) { d["response_spectra"][0]["spectrum"] = json::array({{{"period",1},{"sa",1}}}); }), "$.response_spectra[0].spectrum", "at least 2 points"));
  EXPECT_TRUE(has(issues([](json& d) { d["response_spectra"][0]["spectrum"][0]["period"] = -1; }), "$.response_spectra[0].spectrum[0].period", "greater than 0"));
  EXPECT_TRUE(has(issues([](json& d) { d["response_spectra"][0]["spectrum"][0]["sa"] = -1; }), "$.response_spectra[0].spectrum[0].sa", ">= 0"));
  EXPECT_TRUE(has(issues([](json& d) { d["response_spectra"][0]["spectrum"][0]["period"] = 1e6; d["response_spectra"][0]["spectrum"][1]["period"] = 1e-6; }),
                  "$.response_spectra[0].spectrum", "strictly increasing"));
  EXPECT_TRUE(has(issues([](json& d) { d["response_spectra"].push_back(d["response_spectra"][0]); }), "$.response_spectra[1].name", "duplicate"));
}

TEST(ResponseSpectrumResultsJson, JsonPipelineIsBitIdenticalToDirectApi) {
  auto in = kopruq::io::readModel(baseModel());
  const auto& rc = in.response_spectra[0];
  auto direct = kopruq::ResponseSpectrumSolver::solve(in.model, in.restraints, rc.input);
  ASSERT_TRUE(direct.status.success) << direct.status.message;
  auto viaJson = kopruq::io::readModelText(kopruq::io::writeModel(in).dump());
  auto r = kopruq::ResponseSpectrumSolver::solve(viaJson.model, viaJson.restraints, viaJson.response_spectra[0].input);
  ASSERT_TRUE(r.status.success);
  EXPECT_EQ(r.total_mass, direct.total_mass);
  ASSERT_EQ(r.combined_nodes.size(), direct.combined_nodes.size());
  for (size_t i = 0; i < r.combined_nodes.size(); ++i)
    for (int k = 0; k < 6; ++k) EXPECT_EQ(r.combined_nodes[i].displacement[k], direct.combined_nodes[i].displacement[k]);
}

TEST(ResponseSpectrumResultsJson, SerializesEveryValueExactly) {
  auto in = kopruq::io::readModel(baseModel());
  auto r = kopruq::ResponseSpectrumSolver::solve(in.model, in.restraints, in.response_spectra[0].input);
  ASSERT_TRUE(r.status.success) << r.status.message;
  json out = kopruq::io::writeResponseSpectrumResults({{"EQZ", r}}, "deadbeef");
  EXPECT_EQ(out["schema"], "kopruq-response-spectrum-results");
  EXPECT_EQ(out["metadata"]["model_sha256"], "deadbeef");
  ASSERT_EQ(out["cases"].size(), 1u);
  const auto& cj = out["cases"][0];
  EXPECT_EQ(cj["name"], "EQZ");
  EXPECT_EQ(cj["total_mass"].get<double>(), r.total_mass);
  EXPECT_EQ(cj["total_mass_participation_ratio"].get<double>(), r.total_mass_participation_ratio);
  ASSERT_EQ(cj["modes"].size(), r.modes.size());
  for (size_t i = 0; i < r.modes.size(); ++i) {
    EXPECT_EQ(cj["modes"][i]["period_s"].get<double>(), r.modes[i].period_s);
    EXPECT_EQ(cj["modes"][i]["participation_factor"].get<double>(), r.modes[i].participation_factor);
    EXPECT_EQ(cj["modes"][i]["mass_participation_ratio"].get<double>(), r.modes[i].mass_participation_ratio);
  }
  ASSERT_EQ(cj["combined_nodes"].size(), r.combined_nodes.size());
  for (size_t i = 0; i < r.combined_nodes.size(); ++i) {
    EXPECT_EQ(cj["combined_nodes"][i]["id"], r.combined_nodes[i].node_id);
    for (int k = 0; k < 6; ++k) EXPECT_EQ(cj["combined_nodes"][i]["displacement"][k].get<double>(), r.combined_nodes[i].displacement[k]);
  }
  ASSERT_EQ(cj["combined_elements"].size(), r.combined_elements.size());
  for (size_t i = 0; i < r.combined_elements.size(); ++i)
    for (int k = 0; k < 12; ++k) EXPECT_EQ(cj["combined_elements"][i]["global_end_forces"][k].get<double>(), r.combined_elements[i].global_end_force[k]);
}

TEST(ResponseSpectrumResultsJson, FailedCaseHasEmptyArraysAndAggregateStatusReflectsIt) {
  auto in = kopruq::io::readModel(baseModel());
  auto failed = kopruq::ResponseSpectrumSolver::solve(in.model, {}, in.response_spectra[0].input); // no restraints
  EXPECT_FALSE(failed.status.success);
  json out = kopruq::io::writeResponseSpectrumResults({{"EQZ", failed}}, "0");
  EXPECT_EQ(out["cases"][0]["status"]["success"], false);
  EXPECT_TRUE(out["cases"][0]["modes"].empty());
  EXPECT_FALSE(out["cases"][0].contains("total_mass"));
  EXPECT_EQ(out["status"]["success"], false);
  EXPECT_EQ(out["status"]["code"], "NoRestraints");
}

TEST(ResponseSpectrumResultsJson, SchemaKeysMatchWriterOutput) {
  auto in = kopruq::io::readModel(baseModel());
  auto r = kopruq::ResponseSpectrumSolver::solve(in.model, in.restraints, in.response_spectra[0].input);
  ASSERT_TRUE(r.status.success);
  json out = kopruq::io::writeResponseSpectrumResults({{"EQZ", r}}, "x");
  json schema = json::parse(readFile(source("schemas/response_spectrum_results.schema.json")));
  auto keysOf = [](const json& obj) { std::vector<std::string> ks; for (auto it = obj.begin(); it != obj.end(); ++it) ks.push_back(it.key()); std::sort(ks.begin(), ks.end()); return ks; };
  auto propsOf = [](const json& def) { std::vector<std::string> ks; for (auto it = def.at("properties").begin(); it != def.at("properties").end(); ++it) ks.push_back(it.key()); std::sort(ks.begin(), ks.end()); return ks; };
  EXPECT_EQ(keysOf(out), propsOf(schema));
  EXPECT_EQ(keysOf(out["cases"][0]), propsOf(schema.at("$defs").at("case")));
  EXPECT_EQ(keysOf(out["cases"][0]["modes"][0]), propsOf(schema.at("$defs").at("mode_summary")));
  EXPECT_EQ(schema.at("properties").at("schema_version").at("const"), kopruq::io::response_spectrum_results_schema_version);
}

TEST(Cli, SpectrumCommand) {
  auto run = [](std::vector<std::string> args, std::string* o = nullptr, std::string* e = nullptr) {
    std::ostringstream out, err; int code = kopruq::io::runCli(args, out, err);
    if (o) *o = out.str(); if (e) *e = err.str(); return code;
  };
  std::string out, err;
  EXPECT_NE(std::string(kopruq::io::cli_usage).find("spectrum <model.json>"), std::string::npos);

  EXPECT_EQ(run({"spectrum"}, nullptr, &err), 2);
  EXPECT_EQ(run({"spectrum", source("does/not/exist.json").string()}, nullptr, &err), 4);
  EXPECT_EQ(run({"spectrum", source("tests/data/invalid_model.json").string()}, nullptr, &err), 3);

  const auto model = std::filesystem::temp_directory_path() / "kopruq_cli_spectrum_model.json";
  { std::ofstream f(model, std::ios::binary); f << baseModel().dump(); }
  ASSERT_EQ(run({"spectrum", model.string()}, &out, &err), 0) << err;
  json results = json::parse(out);
  EXPECT_EQ(results["schema"], "kopruq-response-spectrum-results");
  EXPECT_EQ(results["metadata"]["model_sha256"], kopruq::io::sha256Hex(readFile(model)));
  EXPECT_EQ(results["status"]["success"], true);
  ASSERT_EQ(results["cases"].size(), 1u);
  EXPECT_EQ(results["cases"][0]["name"], "EQZ");

  const auto file = std::filesystem::temp_directory_path() / "kopruq_cli_spectrum_results.json";
  std::filesystem::remove(file);
  ASSERT_EQ(run({"spectrum", model.string(), "-o", file.string()}, &out, &err), 0) << err;
  EXPECT_TRUE(out.empty());
  EXPECT_EQ(readFile(file), json::parse(readFile(file)).dump(2) + "\n");
  std::filesystem::remove(file);

  // A model with no response_spectra entries: empty "cases", exit 0.
  auto doc = baseModel(); doc.erase("response_spectra");
  const auto noSpectra = std::filesystem::temp_directory_path() / "kopruq_cli_no_spectra.json";
  { std::ofstream f(noSpectra, std::ios::binary); f << doc.dump(); }
  ASSERT_EQ(run({"spectrum", noSpectra.string()}, &out, &err), 0) << err;
  EXPECT_TRUE(json::parse(out)["cases"].empty());
  std::filesystem::remove(noSpectra);
  std::filesystem::remove(model);
}
