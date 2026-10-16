#pragma once
// JSON results output, format "kopruq-results" version 1 (schemas/results.schema.json).
//
// Output is deterministic: no timestamps, sorted object keys, and doubles
// written with round-trip precision, so identical input gives byte-identical
// results. metadata.model_sha256 identifies the exact input file.
#include "ModelJson.hpp"
#include "../api/SolverAPI.hpp"
#include <string>

namespace kopruq::io {

inline constexpr const char* results_schema_name = "kopruq-results";
inline constexpr int results_schema_version = 1;

inline const char* statusCodeName(SolverStatusCode c) {
  switch (c) {
    case SolverStatusCode::Success: return "Success";
    case SolverStatusCode::InvalidModel: return "InvalidModel";
    case SolverStatusCode::NoRestraints: return "NoRestraints";
    case SolverStatusCode::SingularSystem: return "SingularSystem";
    case SolverStatusCode::NonFiniteSolution: return "NonFiniteSolution";
    case SolverStatusCode::NumericalFailure: return "NumericalFailure";
  }
  return "Unknown";
}

inline json statusJson(const SolverStatus& s) {
  return {{"success", s.success}, {"code", statusCodeName(s.code)}, {"message", s.message}};
}

// One load case or combination. Failed solutions carry only name and status.
inline json solutionJson(const std::string& name, const StaticSolution& s) {
  json j = {{"name", name}, {"status", statusJson(s.status)}};
  if (!s.status.success) return j;
  const auto& r = s.results;
  j["diagnostics"] = {
    {"total_dofs", s.total_dofs}, {"free_dofs", s.free_dofs}, {"fixed_dofs", s.fixed_dofs},
    {"residual_norm", s.residual_norm}, {"relative_residual", s.relative_residual},
    {"force_equilibrium_error", r.force_equilibrium_error}, {"moment_equilibrium_error", r.moment_equilibrium_error},
    {"element_equilibrium_error", r.element_equilibrium_error}, {"element_moment_equilibrium_error", r.element_moment_equilibrium_error},
    {"max_element_force_error", r.max_element_force_error}, {"max_element_moment_error", r.max_element_moment_error},
    {"strain_energy", r.strain_energy}, {"element_strain_energy", r.element_strain_energy}, {"energy_error", r.energy_error}};
  j["nodes"] = json::array();
  for (const auto& n : r.nodes)
    j["nodes"].push_back({{"id", n.node_id}, {"displacement", n.displacement}, {"reaction", n.reaction}, {"restrained", n.restrained}});
  j["elements"] = json::array();
  for (const auto& e : r.elements)
    j["elements"].push_back({{"id", e.element_id}, {"local_end_forces", e.local_end_force}, {"global_end_forces", e.global_end_force},
                             {"strain_energy", e.strain_energy}, {"normalized_force_error", e.normalized_force_error},
                             {"normalized_moment_error", e.normalized_moment_error}});
  return j;
}

inline json writeResults(const ModelInput& in, const MultiCaseSolution& r, const std::string& modelSha256) {
  json j;
  j["schema"] = results_schema_name;
  j["schema_version"] = results_schema_version;
  j["metadata"] = {
    {"solver", "KOPRUQ-SOLVER"}, {"solver_version", version},
    {"model_schema", model_schema_name}, {"model_schema_version", model_schema_version},
    {"model_sha256", modelSha256},
    {"units", {{"length", "m"}, {"force", "kN"}, {"moment", "kNm"}, {"angle", "rad"}}}};
  j["status"] = statusJson(r.status);
  j["load_cases"] = json::array();
  for (std::size_t i = 0; i < r.cases.size(); ++i) j["load_cases"].push_back(solutionJson(in.load_cases.at(i).name, r.cases[i]));
  j["combinations"] = json::array();
  for (std::size_t i = 0; i < r.combinations.size(); ++i) j["combinations"].push_back(solutionJson(in.combinations.at(i).name, r.combinations[i]));
  return j;
}

} // namespace kopruq::io
