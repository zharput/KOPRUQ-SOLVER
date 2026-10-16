#pragma once
// JSON output for modal analysis, format "kopruq-modal-results" version 1
// (schemas/modal_results.schema.json). Separate from the static-analysis
// "kopruq-results" format (ResultsJson.hpp): the two describe different
// kinds of results and neither reuses the other's fixed key set.
//
// Deterministic for the same reason as writeResults: no timestamps, sorted
// keys, round-trip doubles. metadata.model_sha256 identifies the exact
// input file, same convention as writeResults.
#include "ResultsJson.hpp"
#include "../solvers/ModalSolver.hpp"
#include <array>
#include <string>

namespace kopruq::io {

inline constexpr const char* modal_results_schema_name = "kopruq-modal-results";
inline constexpr int modal_results_schema_version = 1;

inline json writeModalResults(const Model& model, const ModalResults& r, const std::string& modelSha256) {
  json j;
  j["schema"] = modal_results_schema_name;
  j["schema_version"] = modal_results_schema_version;
  j["metadata"] = {
    {"solver", "KOPRUQ-SOLVER"}, {"solver_version", version},
    {"model_schema", model_schema_name}, {"model_schema_version", model_schema_version},
    {"model_sha256", modelSha256},
    {"units", {{"length", "m"}, {"angle", "rad"}, {"frequency", "Hz"}, {"angular_frequency", "rad/s"}}}};
  j["status"] = statusJson(r.status);
  j["diagnostics"] = {{"total_dofs", r.total_dofs}, {"free_dofs", r.free_dofs}, {"fixed_dofs", r.fixed_dofs}};
  j["modes"] = json::array();
  if (r.status.success) {
    DofManager dm(model);
    for (std::size_t i = 0; i < r.modes.size(); ++i) {
      const auto& mode = r.modes[i];
      json mj = {{"index", i}, {"frequency_hz", mode.frequency_hz}, {"angular_frequency_rad_s", mode.angular_frequency_rad_s}, {"mode_shape", json::array()}};
      for (const auto& n : model.nodes) {
        auto d = dm.node_dofs(n.id);
        std::array<Real, 6> shape{};
        for (int k = 0; k < 6; ++k) shape[k] = mode.shape(static_cast<Eigen::Index>(d[k]));
        mj["mode_shape"].push_back({{"id", n.id}, {"shape", shape}});
      }
      j["modes"].push_back(mj);
    }
  }
  return j;
}

} // namespace kopruq::io
