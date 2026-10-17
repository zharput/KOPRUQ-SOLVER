#pragma once
// JSON output for response spectrum analysis, format
// "kopruq-response-spectrum-results" version 1
// (schemas/response_spectrum_results.schema.json). Separate from both
// "kopruq-results" (static load cases) and "kopruq-modal-results" (plain
// modal analysis): each describes a different kind of result with its own
// fixed key set, none reused by the others.
//
// Deliberately omits each mode's full per-node/per-element AnalysisResults
// (available only through the C++ API) to keep the document a practical
// size; it keeps the per-mode summary (period, frequency, participation,
// effective/participating mass) and the SRSS-combined peak values, which
// are the actual engineering deliverable of this analysis.
//
// Deterministic for the same reason as writeResults/writeModalResults: no
// timestamps, sorted keys, round-trip doubles.
#include "ResultsJson.hpp"
#include "../solvers/ResponseSpectrumSolver.hpp"
#include <string>
#include <utility>
#include <vector>

namespace kopruq::io {

inline constexpr const char* response_spectrum_results_schema_name = "kopruq-response-spectrum-results";
inline constexpr int response_spectrum_results_schema_version = 1;

inline json writeResponseSpectrumResults(const std::vector<std::pair<std::string, ResponseSpectrumResults>>& cases, const std::string& modelSha256) {
  json j;
  j["schema"] = response_spectrum_results_schema_name;
  j["schema_version"] = response_spectrum_results_schema_version;
  j["metadata"] = {
    {"solver", "KOPRUQ-SOLVER"}, {"solver_version", version},
    {"model_schema", model_schema_name}, {"model_schema_version", model_schema_version},
    {"model_sha256", modelSha256},
    {"units", {{"length", "m"}, {"force", "kN"}, {"moment", "kNm"}, {"angle", "rad"}, {"frequency", "Hz"}, {"angular_frequency", "rad/s"}}}};
  // Aggregate top-level status: success only if every named case succeeded; otherwise
  // carries the first failing case's own status (code and message), same convention
  // as LinearStaticSolver::solveCases' aggregate status over its load cases.
  SolverStatus aggregate{true, "response spectrum analysis completed", SolverStatusCode::Success};
  j["cases"] = json::array();
  for (const auto& [name, r] : cases) {
    json cj = {{"name", name}, {"status", statusJson(r.status)}};
    if (!r.status.success) {
      if (aggregate.success) aggregate = r.status;
      cj["modes"] = json::array(); cj["combined_nodes"] = json::array(); cj["combined_elements"] = json::array();
      j["cases"].push_back(cj); continue;
    }
    cj["total_mass"] = r.total_mass;
    cj["total_mass_participation_ratio"] = r.total_mass_participation_ratio;
    cj["modes"] = json::array();
    for (const auto& m : r.modes)
      cj["modes"].push_back({{"period_s", m.period_s}, {"frequency_hz", m.frequency_hz}, {"participation_factor", m.participation_factor},
                             {"effective_mass", m.effective_mass}, {"mass_participation_ratio", m.mass_participation_ratio}});
    cj["combined_nodes"] = json::array();
    for (const auto& n : r.combined_nodes) cj["combined_nodes"].push_back({{"id", n.node_id}, {"displacement", n.displacement}, {"reaction", n.reaction}});
    cj["combined_elements"] = json::array();
    for (const auto& e : r.combined_elements) cj["combined_elements"].push_back({{"id", e.element_id}, {"global_end_forces", e.global_end_force}});
    j["cases"].push_back(cj);
  }
  j["status"] = statusJson(aggregate);
  return j;
}

} // namespace kopruq::io
