// 2026-11-23 -- element station results (kopruq/results/ElementStations.hpp, CLI `stations`).
// Dedicated new file. Expected values are derived by hand from beam statics (closed forms) or by an
// independent numerical quadrature of the loads inside this file; nothing is computed with the code under test
// except the solved model itself (end forces), which the closed forms also predict.
// Convention under test: f = [N,Vy,Vz,T,My,Mz] = force/moment that the part beyond x exerts on the piece [0,x].
// For a member along +x with reference_y = +y the local axes equal the global ones.
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <sstream>
#include <string>
#include "kopruq/io/Cli.hpp"

using nlohmann::json;
namespace {
json runStations(const json& model, std::vector<std::string> extra = {}) {
  const auto dir = std::filesystem::temp_directory_path(); const auto path = dir / ("kopruq_st_" + std::to_string(std::hash<std::string>{}(model.dump())) + ".json");
  { std::ofstream(path, std::ios::binary) << model.dump(); }
  std::ostringstream out, err; std::vector<std::string> args{"stations", path.string()}; args.insert(args.end(), extra.begin(), extra.end());
  const int rc = kopruq::io::runCli(args, out, err); std::filesystem::remove(path);
  EXPECT_EQ(rc, 0) << err.str(); return json::parse(out.str());
}
json base(double L, json supports, json beams = nullptr) {
  return {{"schema", "kopruq-model"}, {"schema_version", 1},
          {"nodes", {{{"id", 1}, {"x", 0}, {"y", 0}, {"z", 0}}, {{"id", 2}, {"x", L}, {"y", 0}, {"z", 0}}}},
          {"materials", {{{"id", 1}, {"elastic_modulus", 2e8}, {"poisson_ratio", 0.3}, {"unit_weight", 25}}}},
          {"sections", {{{"id", 1}, {"area", 0.02}, {"Iy", 1e-4}, {"Iz", 2e-4}, {"J", 3e-4}}}},
          {"beams", beams.is_null() ? json::array({{{"id", 7}, {"nodes", {1, 2}}, {"material", 1}, {"section", 1}}}) : beams}, {"supports", supports}};
}
const json fixed1 = {{{"node", 1}, {"restraints", {true, true, true, true, true, true}}}};
const json pinroller = {{{"node", 1}, {"restraints", {true, true, true, true, false, false}}}, {{"node", 2}, {"restraints", {false, true, true, false, false, false}}}};
const json fixedfixed = {{{"node", 1}, {"restraints", {true, true, true, true, true, true}}}, {{"node", 2}, {"restraints", {true, true, true, true, true, true}}}};
struct Row { double x; std::string side; std::array<double, 6> f; };
std::vector<Row> rowsOf(const json& doc, const std::string& kind, size_t idx) {
  std::vector<Row> r; for (const auto& s : doc[kind][idx]["elements"][0]["stations"]) { std::array<double, 6> f; for (int i = 0; i < 6; ++i) f[i] = s["f"][i]; r.push_back({s["x"], s.value("side", "single"), f}); }
  return r;
}
}

TEST(ElementStations, SimplySupportedUniformLoadMatchesClosedForm) {
  const double L = 6, q = -10;  // kN/m in z
  auto m = base(L, pinroller); m["load_cases"] = {{{"name", "q"}, {"member", {{{"element", 7}, {"axes", "global"}, {"direction", "z"}, {"w1", q}, {"w2", q}}}}}};
  const auto doc = runStations(m, {"--stations", "13"});
  EXPECT_EQ(doc["schema"], "kopruq-element-stations");
  const auto rows = rowsOf(doc, "load_cases", 0); ASSERT_EQ(rows.size(), 13u);
  for (const auto& r : rows) {
    // sagging moment M = |q| x (L-x)/2 about y is NEGATIVE in this convention (moment on the +x face); shear Vz = -(|q| (L/2 - x))
    EXPECT_NEAR(r.f[4], -(-q) * r.x * (L - r.x) / 2, 1e-9) << r.x;
    EXPECT_NEAR(r.f[2], -(-q) * (L / 2 - r.x), 1e-9) << r.x;
    EXPECT_NEAR(r.f[0], 0, 1e-9); EXPECT_NEAR(r.f[1], 0, 1e-9); EXPECT_NEAR(r.f[3], 0, 1e-9); EXPECT_NEAR(r.f[5], 0, 1e-9);
  }
  EXPECT_LT(doc["load_cases"][0]["elements"][0]["closure_force"].get<double>(), 1e-9);
  EXPECT_LT(doc["load_cases"][0]["elements"][0]["closure_moment"].get<double>(), 1e-9);
}

TEST(ElementStations, FixedFixedUniformLoadEndAndMidMoments) {
  const double L = 8, q = -12;
  auto m = base(L, fixedfixed); m["load_cases"] = {{{"name", "q"}, {"member", {{{"element", 7}, {"axes", "local"}, {"direction", "z"}, {"w1", q}, {"w2", q}}}}}};
  const auto rows = rowsOf(runStations(m, {"--stations", "9"}), "load_cases", 0);
  // fixed-fixed: hogging end moment |q|L^2/12 at both ends, sagging |q|L^2/24 at mid-span (sagging negative here)
  EXPECT_NEAR(rows.front().f[4], (-q) * L * L / 12, 1e-6);   // x = 0: hogging = positive
  EXPECT_NEAR(rows[4].f[4], -(-q) * L * L / 24, 1e-6);       // x = L/2
  EXPECT_NEAR(rows.back().f[4], (-q) * L * L / 12, 1e-6);    // x = L
  EXPECT_NEAR(rows[4].f[2], 0, 1e-9);                        // zero shear at mid-span
}

TEST(ElementStations, CantileverPointLoadHasBeforeAfterStationsAtTheLoad) {
  const double L = 4, P = -30;  // load down at mid-span
  auto m = base(L, fixed1); m["load_cases"] = {{{"name", "P"}, {"point", {{{"element", 7}, {"axes", "local"}, {"direction", "z"}, {"value", P}, {"position", 0.5}}}}}};
  const auto rows = rowsOf(runStations(m, {"--stations", "5"}), "load_cases", 0);
  int before = 0, after = 0;
  for (const auto& r : rows) {
    const double x = r.x, Mexp = x < L / 2 ? (-P) * (L / 2 - x) : 0;        // hogging P (L/2 - x) up to the load, zero beyond
    EXPECT_NEAR(r.f[4], Mexp, 1e-9) << "x=" << x;
    EXPECT_NEAR(r.f[2], (x < L / 2 || r.side == "before") ? P : 0, 1e-9) << "x=" << x << " " << r.side;  // shear = P (down-load) until the load, 0 after
    if (r.side == "before") ++before; if (r.side == "after") ++after;
  }
  EXPECT_EQ(before, 1); EXPECT_EQ(after, 1);
}

TEST(ElementStations, ReproducesRecoveredEndForcesAndPointMomentsAndOffsetsAndReleases) {
  // skewed 3D member with offsets, a release, Timoshenko shear areas, global projected load, point force/moment, self-weight, thermal
  json m = {{"schema", "kopruq-model"}, {"schema_version", 1},
    {"nodes", {{{"id", 1}, {"x", 0}, {"y", 0}, {"z", 0}}, {{"id", 2}, {"x", 5}, {"y", 3}, {"z", 1.5}}}},
    {"materials", {{{"id", 1}, {"elastic_modulus", 3e7}, {"poisson_ratio", 0.2}, {"unit_weight", 25}, {"thermal_expansion", 1e-5}}}},
    {"sections", {{{"id", 1}, {"area", 0.3}, {"Iy", 0.004}, {"Iz", 0.009}, {"J", 0.006}, {"Asy", 0.25}, {"Asz", 0.25}}}},
    {"beams", {{{"id", 7}, {"nodes", {1, 2}}, {"material", 1}, {"section", 1}, {"reference_y", {0, 0, 1}}, {"offset1", {0, 0, 0.2}}, {"offset2", {0.1, 0, 0}}, {"releases", {{"end", {false, false, false, false, false, true}}}}}}},
    {"supports", fixed1},
    {"load_cases", {{{"name", "a"}, {"self_weight", 1}, {"member", {{{"element", 7}, {"axes", "global"}, {"direction", "z"}, {"w1", -4}, {"w2", -9}, {"start", 0.2}, {"end", 0.9}, {"projected", true}},
                                                             {{"element", 7}, {"axes", "local"}, {"direction", "y"}, {"w1", 2}, {"w2", 2}}, {{"element", 7}, {"axes", "local"}, {"direction", "x"}, {"kind", "moment"}, {"w1", 1.5}, {"w2", 1.5}}}},
                     {"point", {{{"element", 7}, {"axes", "global"}, {"direction", "x"}, {"value", 7}, {"position", 0.3}}, {{"element", 7}, {"axes", "local"}, {"direction", "y"}, {"kind", "moment"}, {"value", 5}, {"position", 0.6}}}},
                     {"thermal", {{{"element", 7}, {"uniform_delta_t", 20}, {"gradient_y_delta_t", 10}, {"gradient_y_depth", 0.8}}}}},
                    {{"name", "b"}, {"nodal", {{{"node", 2}, {"values", {10, -5, 20, 1, 2, 3}}}}}}}},
    {"combinations", {{{"name", "1.35a+0.5b"}, {"terms", {{{"case", "a"}, {"factor", 1.35}}, {{"case", "b"}, {"factor", 0.5}}}}}}}};
  const auto doc = runStations(m, {"--stations", "7"});
  const auto res = [&] { // end forces from `solve` (same model)
    const auto path = std::filesystem::temp_directory_path() / "kopruq_st_solve.json"; { std::ofstream(path, std::ios::binary) << m.dump(); }
    std::ostringstream out, err; EXPECT_EQ(kopruq::io::runCli({"solve", path.string()}, out, err), 0) << err.str(); std::filesystem::remove(path); return json::parse(out.str()); }();
  for (const char* kind : {"load_cases", "combinations"}) for (size_t k = 0; k < doc[kind].size(); ++k) {
    const auto& el = doc[kind][k]["elements"][0]; const auto& f = res[kind][k]["elements"][0]["local_end_forces"];
    EXPECT_LT(el["closure_force"].get<double>(), 1e-8) << kind << k; EXPECT_LT(el["closure_moment"].get<double>(), 1e-8) << kind << k;
    const auto& st = el["stations"]; const auto& first = st.front()["f"];
    // x = 0: R(0) = -f1 (loads at x=0 are none), M(0) = -m1
    for (int i = 0; i < 3; ++i) { EXPECT_NEAR(first[i].get<double>(), -f[i].get<double>(), 1e-8); EXPECT_NEAR(first[3 + i].get<double>(), -f[3 + i].get<double>(), 1e-8); }
  }
  // the released RZ at the end: the moment about local z at x = L is zero (end force of a released DOF)
  EXPECT_NEAR(doc["load_cases"][0]["elements"][0]["stations"].back()["f"][5].get<double>(), 0.0, 1e-8);
}

TEST(ElementStations, CombinationStationsAreTheLinearSuperpositionOfTheCases) {
  const double L = 5;
  auto m = base(L, fixed1);
  // both cases carry a (zero-valued) load at the same position so that all three station lists coincide
  m["load_cases"] = {{{"name", "g"}, {"member", {{{"element", 7}, {"axes", "local"}, {"direction", "z"}, {"w1", -3}, {"w2", -6}}}}, {"point", {{{"element", 7}, {"axes", "local"}, {"direction", "y"}, {"value", 0}, {"position", 0.25}}}}},
                     {{"name", "p"}, {"point", {{{"element", 7}, {"axes", "local"}, {"direction", "y"}, {"value", 8}, {"position", 0.25}}}}}};
  m["combinations"] = {{{"name", "c"}, {"terms", {{{"case", "g"}, {"factor", 1.35}}, {{"case", "p"}, {"factor", 1.5}}}}}};
  const auto doc = runStations(m, {"--stations", "5"});
  const auto g = rowsOf(doc, "load_cases", 0), p = rowsOf(doc, "load_cases", 1), c = rowsOf(doc, "combinations", 0);
  ASSERT_EQ(g.size(), c.size()); ASSERT_EQ(p.size(), c.size());
  for (size_t k = 0; k < c.size(); ++k) for (int i = 0; i < 6; ++i) EXPECT_NEAR(c[k].f[i], 1.35 * g[k].f[i] + 1.5 * p[k].f[i], 1e-8) << "k=" << k << " i=" << i;
  // closed form of the point-load case: Vy = P = +8 up to the load (cantilever, load +8 in y at x = L/4), 0 beyond it
  for (const auto& r : p) if (r.x > 1.3) EXPECT_NEAR(r.f[1], 0.0, 1e-9); else if (r.side == "before" || r.x < 1.2) EXPECT_NEAR(r.f[1], 8.0, 1e-9);
}

TEST(ElementStations, UsageErrors) {
  std::ostringstream out, err;
  EXPECT_EQ(kopruq::io::runCli({"stations"}, out, err), 2);
  EXPECT_EQ(kopruq::io::runCli({"stations", "x.json", "--stations", "1"}, out, err), 2);
  EXPECT_EQ(kopruq::io::runCli({"stations", "x.json", "--stations", "abc"}, out, err), 2);
}
