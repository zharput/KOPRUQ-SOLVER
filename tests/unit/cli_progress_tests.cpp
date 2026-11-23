// 2026-11-23 -- opt-in `--progress json` on the CLI (docs: ui/docs/cli-commands-design.md section 3).
// Dedicated new file. The stream is newline-delimited JSON on stderr, ends with {"event":"done"}, and
// without the flag every output stays exactly as before.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include "kopruq/io/Cli.hpp"

using nlohmann::json;
namespace {
struct Dir {
  std::filesystem::path path; std::string utf8;
  Dir() { path = std::filesystem::temp_directory_path() / ("kopruq_progress_" + std::to_string(std::random_device{}())); std::filesystem::create_directories(path); utf8 = path.generic_string(); }
  ~Dir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
  std::string write(const std::string& n, const std::string& t) { std::ofstream(path / n, std::ios::binary) << t; return utf8 + "/" + n; }
};
const char* kModel = R"({"schema":"kopruq-model","schema_version":1,
  "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":4,"y":0,"z":0}],
  "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3,"unit_weight":78.5}],
  "sections":[{"id":1,"area":0.02,"Iy":1e-4,"Iz":2e-4,"J":3e-4}],
  "beams":[{"id":7,"nodes":[1,2],"material":1,"section":1}],
  "supports":[{"node":1,"restraints":[true,true,true,true,true,true]}],
  "load_cases":[{"name":"c","nodal":[{"node":2,"values":[0,0,-1000,0,0,0]}]}],
  "response_spectra":[{"name":"SX","direction":"x","num_modes":2,"spectrum":[{"period":0.01,"sa":1.0},{"period":5.0,"sa":1.0}]},
                      {"name":"SY","direction":"y","num_modes":2,"spectrum":[{"period":0.01,"sa":1.0},{"period":5.0,"sa":1.0}]}]})";
int run(std::vector<std::string> a, std::string* out, std::string* err) { std::ostringstream o, e; const int rc = kopruq::io::runCli(a, o, e); *out = o.str(); *err = e.str(); return rc; }
std::vector<json> events(const std::string& err) {
  std::vector<json> v; std::istringstream in(err); std::string line;
  while (std::getline(in, line)) if (line.rfind("{\"event\"", 0) == 0) v.push_back(json::parse(line));
  return v;
}
}

TEST(CliProgress, SolveStreamsStagesInOrderAndEndsWithDone) {
  Dir d; const auto m = d.write("m.json", kModel); std::string out, err;
  ASSERT_EQ(run({"solve", m, "-o", d.utf8 + "/r.json", "--progress", "json"}, &out, &err), 0) << err;
  const auto ev = events(err);
  std::vector<std::string> seq; for (const auto& e : ev) seq.push_back(e["event"].get<std::string>() + (e.contains("name") ? ":" + e["name"].get<std::string>() + ":" + e["state"].get<std::string>() : ""));
  EXPECT_EQ(seq, (std::vector<std::string>{"stage:read:start", "stage:read:done", "stage:solve:start", "stage:solve:done", "stage:write:start", "stage:write:done", "done"}));
  EXPECT_EQ(ev.back()["exit"], 0);
  for (std::size_t i = 1; i < ev.size(); ++i) EXPECT_GE(ev[i]["t"].get<double>(), ev[i - 1]["t"].get<double>()) << "time never goes backwards";
}

TEST(CliProgress, WithoutTheFlagNothingChanges) {
  Dir d; const auto m = d.write("m.json", kModel); std::string o1, e1, o2, e2;
  ASSERT_EQ(run({"solve", m, "-o", d.utf8 + "/a.json"}, &o1, &e1), 0);
  ASSERT_EQ(run({"solve", m, "-o", d.utf8 + "/b.json", "--progress", "json"}, &o2, &e2), 0);
  EXPECT_TRUE(events(e1).empty()); EXPECT_EQ(e1.find("\"event\""), std::string::npos);
  auto slurp = [&](const char* n) { std::ifstream f(d.path / n, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), {}); };
  EXPECT_EQ(slurp("a.json"), slurp("b.json")) << "results are byte-identical with and without the flag";
}

TEST(CliProgress, SpectrumReportsEachCaseAndModalWorks) {
  Dir d; const auto m = d.write("m.json", kModel); std::string out, err;
  ASSERT_EQ(run({"spectrum", m, "-o", d.utf8 + "/s.json", "--progress", "json"}, &out, &err), 0) << err;
  std::vector<json> cases; for (const auto& e : events(err)) if (e["event"] == "case") cases.push_back(e);
  ASSERT_EQ(cases.size(), 2u); EXPECT_EQ(cases[0]["name"], "SX"); EXPECT_EQ(cases[1]["index"], 2); EXPECT_EQ(cases[1]["of"], 2);
  ASSERT_EQ(run({"modal", m, "-o", d.utf8 + "/mo.json", "-n", "2", "--progress", "json"}, &out, &err), 0) << err;
  EXPECT_EQ(events(err).back()["event"], "done");
}

TEST(CliProgress, FailuresStillCloseTheStreamWithTheExitCode) {
  Dir d; std::string out, err;
  EXPECT_EQ(run({"solve", d.utf8 + "/missing.json", "--progress", "json"}, &out, &err), 4);
  auto ev = events(err); ASSERT_FALSE(ev.empty()); EXPECT_EQ(ev.back()["event"], "done"); EXPECT_EQ(ev.back()["exit"], 4);
  const auto bad = d.write("bad.json", "{nope");
  EXPECT_EQ(run({"solve", bad, "--progress", "json"}, &out, &err), 3);
  EXPECT_EQ(events(err).back()["exit"], 3);
  EXPECT_EQ(run({"solve", bad, "--progress", "xml"}, &out, &err), 2); // only 'json' is accepted
}
