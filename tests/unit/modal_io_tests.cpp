// Phase 6.1b — JSON/CLI support for modal analysis.
//
// Verifies the JSON writer loses no information from ModalSolver's output
// (every mode shape component serialized exactly, same convention as
// ResultsJson's ResultsJson.JsonPipelineIsBitIdenticalToDirectApi /
// ExampleModelsSolveAndSerializeExactly), that the published schema's key
// set matches the writer's actual output, and the CLI's exit codes and
// files for the new "modal" command.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "kopruq/io/Cli.hpp"

using kopruq::io::json;

namespace {

std::string readFile(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
std::filesystem::path source(const char* rel) { return std::filesystem::path(KOPRUQ_SOURCE_DIR) / rel; }

kopruq::io::ModelInput cantileverModel() {
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,4,0,0},{3,8,0,0}};
  m.materials={{1,210e6,.3,78.5}};
  m.sections={{1,.3,.02,.04,.01}};
  m.beams={{1,1,2,1,1,{0,1,0}},{2,2,3,1,1,{0,1,0}}};
  kopruq::io::ModelInput in; in.model=m; in.restraints={{1,{true,true,true,true,true,true}}};
  return in;
}

} // namespace

TEST(ModalResultsJson, SerializesEveryModeShapeComponentExactly) {
  auto in=cantileverModel();
  auto r=kopruq::ModalSolver::solve(in.model,in.restraints,4);
  ASSERT_TRUE(r.status.success)<<r.status.message;
  json out=kopruq::io::writeModalResults(in.model,r,"deadbeef");
  EXPECT_EQ(out["schema"],"kopruq-modal-results");
  EXPECT_EQ(out["metadata"]["model_sha256"],"deadbeef");
  EXPECT_EQ(out["diagnostics"]["free_dofs"],r.free_dofs);
  ASSERT_EQ(out["modes"].size(),r.modes.size());
  kopruq::DofManager dm(in.model);
  for (std::size_t i=0;i<r.modes.size();++i) {
    EXPECT_EQ(out["modes"][i]["index"],i);
    EXPECT_EQ(out["modes"][i]["frequency_hz"].get<double>(),r.modes[i].frequency_hz);
    EXPECT_EQ(out["modes"][i]["angular_frequency_rad_s"].get<double>(),r.modes[i].angular_frequency_rad_s);
    ASSERT_EQ(out["modes"][i]["mode_shape"].size(),in.model.nodes.size());
    for (std::size_t n=0;n<in.model.nodes.size();++n) {
      auto d=dm.node_dofs(in.model.nodes[n].id);
      EXPECT_EQ(out["modes"][i]["mode_shape"][n]["id"],in.model.nodes[n].id);
      for (int k=0;k<6;++k) EXPECT_EQ(out["modes"][i]["mode_shape"][n]["shape"][k].get<double>(), r.modes[i].shape((Eigen::Index)d[k]));
    }
  }
  // Non-decreasing frequencies survive serialization.
  for (std::size_t i=1;i<r.modes.size();++i) EXPECT_GE(out["modes"][i]["frequency_hz"].get<double>(), out["modes"][i-1]["frequency_hz"].get<double>());
}

TEST(ModalResultsJson, FailedAnalysisHasEmptyModesArray) {
  auto in=cantileverModel();
  auto r=kopruq::ModalSolver::solve(in.model,{},4); // no restraints
  json out=kopruq::io::writeModalResults(in.model,r,"0");
  EXPECT_EQ(out["status"]["success"],false);
  EXPECT_EQ(out["status"]["code"],"NoRestraints");
  EXPECT_TRUE(out["modes"].empty());
}

TEST(ModalResultsJson, OutputIsDeterministic) {
  auto in=cantileverModel();
  auto once=[&]{ return kopruq::io::writeModalResults(in.model,kopruq::ModalSolver::solve(in.model,in.restraints,3),"x").dump(2); };
  EXPECT_EQ(once(),once());
}

TEST(ModalResultsJson, SchemaTopLevelKeysMatchWriterOutput) {
  auto in=cantileverModel();
  auto r=kopruq::ModalSolver::solve(in.model,in.restraints,2);
  ASSERT_TRUE(r.status.success);
  json out=kopruq::io::writeModalResults(in.model,r,"x");
  json schema=json::parse(readFile(source("schemas/modal_results.schema.json")));
  std::vector<std::string> schemaKeys, outKeys;
  for (auto it=schema.at("properties").begin();it!=schema.at("properties").end();++it) schemaKeys.push_back(it.key());
  for (auto it=out.begin();it!=out.end();++it) outKeys.push_back(it.key());
  std::sort(schemaKeys.begin(),schemaKeys.end()); std::sort(outKeys.begin(),outKeys.end());
  EXPECT_EQ(schemaKeys,outKeys);
  EXPECT_EQ(schema.at("properties").at("schema_version").at("const"), kopruq::io::modal_results_schema_version);
  const auto& modeDef=schema.at("$defs").at("mode").at("properties");
  std::vector<std::string> modeSchemaKeys, modeOutKeys;
  for (auto it=modeDef.begin();it!=modeDef.end();++it) modeSchemaKeys.push_back(it.key());
  for (auto it=out["modes"][0].begin();it!=out["modes"][0].end();++it) modeOutKeys.push_back(it.key());
  std::sort(modeSchemaKeys.begin(),modeSchemaKeys.end()); std::sort(modeOutKeys.begin(),modeOutKeys.end());
  EXPECT_EQ(modeSchemaKeys,modeOutKeys);
}

TEST(Cli, ModalCommand) {
  auto run=[](std::vector<std::string> args, std::string* o=nullptr, std::string* e=nullptr) {
    std::ostringstream out, err; int code=kopruq::io::runCli(args,out,err);
    if (o) *o=out.str(); if (e) *e=err.str(); return code;
  };
  std::string out, err;
  EXPECT_NE(std::string(kopruq::io::cli_usage).find("modal <model.json>"), std::string::npos);

  EXPECT_EQ(run({"modal"}, nullptr, &err), 2);
  EXPECT_EQ(run({"modal", "a.json", "-n"}), 2);
  EXPECT_EQ(run({"modal", "a.json", "-n", "abc"}), 2);
  EXPECT_EQ(run({"modal", "a.json", "-n", "0"}), 2);
  EXPECT_EQ(run({"modal", source("does/not/exist.json").string()}, nullptr, &err), 4);
  EXPECT_EQ(run({"modal", source("tests/data/invalid_model.json").string()}, nullptr, &err), 3);

  const auto model = source("examples/cantilever.json");
  ASSERT_EQ(run({"modal", model.string(), "-n", "3"}, &out, &err), 0) << err;
  json results = json::parse(out);
  EXPECT_EQ(results["schema"], "kopruq-modal-results");
  EXPECT_EQ(results["metadata"]["model_sha256"], kopruq::io::sha256Hex(readFile(model)));
  ASSERT_EQ(results["modes"].size(), 3u);

  // Default -n (6) when omitted.
  ASSERT_EQ(run({"modal", model.string()}, &out, &err), 0) << err;
  EXPECT_EQ(json::parse(out)["modes"].size(), 6u);

  const auto file = std::filesystem::temp_directory_path() / "kopruq_cli_modal_results.json";
  std::filesystem::remove(file);
  ASSERT_EQ(run({"modal", model.string(), "-o", file.string(), "-n", "2"}, &out, &err), 0) << err;
  EXPECT_TRUE(out.empty());
  EXPECT_EQ(readFile(file), json::parse(readFile(file)).dump(2) + "\n");
  std::filesystem::remove(file);

  // A model that reads fine but has no restraints: modal results written, exit code 1.
  auto doc = json::parse(readFile(model));
  doc["supports"] = json::array();
  const auto unsupported = std::filesystem::temp_directory_path() / "kopruq_cli_modal_unsupported.json";
  { std::ofstream f(unsupported, std::ios::binary); f << doc.dump(); }
  EXPECT_EQ(run({"modal", unsupported.string()}, &out, &err), 1);
  EXPECT_EQ(json::parse(out)["status"]["code"], "NoRestraints");
  EXPECT_NE(err.find("analysis failed"), std::string::npos);
  std::filesystem::remove(unsupported);
}
