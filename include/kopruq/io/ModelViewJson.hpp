#pragma once
// "kopruq-model-view" v1 (2026-11-23): everything a viewer needs about a model in one document,
// produced by the CLI command `inspect` (docs: ui/docs/cli-commands-design.md). It is a NEW schema:
// the existing kopruq-model / kopruq-results schemas and the existing commands are unchanged.
//   model            the model exactly as io::writeModel writes it (the kopruq-model v1 object)
//   section_outlines display outlines per section id: contours in section coordinates (y = member
//                    local y, z = member local z, centroid at the origin); the first contour is the
//                    outer boundary, further ones are holes. Display data only, never used by the solver.
//   bounds           node coordinate bounding box
//   import           counts and the disclosed assumptions / rejected records of a MIDAS import (empty
//                    for a native model) -- the same information `solve` prints on stderr, structured
#include "ModelJson.hpp"
#include "ResultsJson.hpp"
#include "midas/MidasJsonImporter.hpp"

namespace kopruq::io {

inline constexpr const char* model_view_schema_name = "kopruq-model-view";
inline constexpr int model_view_schema_version = 1;

inline json modelViewJson(const ModelInput& in, const midas::MidasImportReport* report, const std::string& sourceFormat, const std::string& modelSha256) {
  json j;
  j["schema"] = model_view_schema_name;
  j["schema_version"] = model_view_schema_version;
  j["metadata"] = {{"solver", "KOPRUQ-SOLVER"}, {"solver_version", version}, {"source_format", sourceFormat},
                  {"model_sha256", modelSha256}, {"units", {{"length", "m"}, {"force", "kN"}, {"moment", "kNm"}, {"angle", "rad"}}}};
  j["model"] = writeModel(in);
  j["section_outlines"] = json::array();
  for (const auto& [sid, contours] : in.section_outlines) {
    json cs = json::array();
    for (const auto& c : contours) { json pts = json::array(); for (const auto& p : c) pts.push_back({p.x(), p.y()}); cs.push_back(std::move(pts)); }
    j["section_outlines"].push_back({{"section", sid}, {"contours", std::move(cs)}});
  }
  if (!in.model.nodes.empty()) {
    double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    for (const auto& n : in.model.nodes) { const double p[3] = {n.x, n.y, n.z}; for (int k = 0; k < 3; ++k) { lo[k] = std::min(lo[k], p[k]); hi[k] = std::max(hi[k], p[k]); } }
    j["bounds"] = {{"min", {lo[0], lo[1], lo[2]}}, {"max", {hi[0], hi[1], hi[2]}}};
  }
  json imp = {{"assumptions", json::array()}, {"rejected", json::array()}};
  if (report) {
    for (const auto& a : report->assumptions) imp["assumptions"].push_back(a);
    for (const auto& r : report->rejected) imp["rejected"].push_back(r);
    imp["counts"] = {{"nodes", report->nodes}, {"beams", report->beams}, {"materials", report->materials}, {"sections", report->sections},
                     {"supports", report->supports}, {"rigid_links", report->rigid_links}, {"load_cases", report->load_cases},
                     {"nodal_loads", report->nodal_loads}, {"member_loads", report->member_loads}, {"thermal_loads", report->thermal_loads},
                     {"elastic_links", report->springs}, {"joint_springs", report->joint_springs}, {"elastic_links_rejected", report->elastic_links_rejected},
                     {"combinations", report->combinations}, {"spectrum_combinations", report->spectrum_combinations}};
  }
  j["import"] = std::move(imp);
  return j;
}

} // namespace kopruq::io
