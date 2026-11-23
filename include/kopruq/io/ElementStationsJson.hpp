#pragma once
// JSON output "kopruq-element-stations" version 1 (schemas/element_stations.schema.json): internal forces
// along Beam3D members for every load case and combination (results/ElementStations.hpp has the method and
// the sign convention). A combination's stations come from its own combined load case and recovered end
// forces (exactly linear in the cases). Response-spectrum peaks are not signed fields and have no equilibrium
// stations, so they are not produced here.
#include "ResultsJson.hpp"
#include "../results/ElementStations.hpp"

namespace kopruq::io {

inline constexpr const char* stations_schema_name = "kopruq-element-stations";
inline constexpr int stations_schema_version = 1;

inline json stationsOf(const std::string& name, const Model& m, const LoadCase& lc, const StaticSolution& s, int n) {
  json j = {{"name", name}, {"status", statusJson(s.status)}};
  if (!s.status.success) return j;
  j["elements"] = json::array();
  for (const auto& e : computeElementStations(m, lc, s.results, n)) {
    json st = json::array();
    for (const auto& r : e.rows) { json o = {{"x", r.x}, {"f", r.f}}; if (r.side != 's') o["side"] = r.side == 'b' ? "before" : "after"; st.push_back(o); }
    j["elements"].push_back({{"id", e.element}, {"length", e.length}, {"closure_force", e.closure_force}, {"closure_moment", e.closure_moment}, {"stations", st}});
  }
  return j;
}

inline json writeStations(const ModelInput& in, const MultiCaseSolution& r, const std::string& modelSha256, int n) {
  json j;
  j["schema"] = stations_schema_name; j["schema_version"] = stations_schema_version;
  j["metadata"] = {{"solver", "KOPRUQ-SOLVER"}, {"solver_version", version}, {"model_sha256", modelSha256},
                   {"units", {{"length", "m"}, {"force", "kN"}, {"moment", "kNm"}}},
                   {"convention", "solver local axes; f = [N, Vy, Vz, T, My, Mz] = force/moment exerted on the +x face of the piece [0,x] by the part beyond x (tension positive); no MIDAS sign flip"},
                   {"uniform_stations", n}};
  j["status"] = statusJson(r.status);
  j["load_cases"] = json::array();
  for (std::size_t i = 0; i < r.cases.size(); ++i) j["load_cases"].push_back(stationsOf(in.load_cases.at(i).name, in.model, in.load_cases.at(i), r.cases[i], n));
  j["combinations"] = json::array();
  for (std::size_t k = 0; k < r.combinations.size(); ++k) {
    LoadCase lc; lc.name = in.combinations.at(k).name;
    for (const auto& [idx, f] : in.combinations.at(k).terms) LinearStaticSolver::appendFactoredLoads(lc, in.load_cases.at(idx), f);
    j["combinations"].push_back(stationsOf(lc.name, in.model, lc, r.combinations[k], n));
  }
  return j;
}

} // namespace kopruq::io
