#pragma once
// GUI-5: matches KOPRUQ-SOLVER's own results.json (the "kopruq-results" schema,
// include/kopruq/io/ResultsJson.hpp) against a MIDAS Excel reference table
// (ExcelReferenceReader) by original MIDAS node/element id + load case name -- both of which
// Phase 7B's importer preserves unchanged (docs/MIDAS_JSON_DATA_AUDIT.md). Every reference row
// produces exactly one ComparisonRow with a real status: PASS/FAIL (actually compared),
// NotApplicable (the id/load-case pair has no counterpart in results.json, or that load case
// failed to solve), or Unsupported (a known formulation gap, e.g. elastic links). Never silently
// dropped.
#include "ComparisonEngine.h"
#include "ExcelReferenceReader.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace kopruq::verification {

struct ComparisonRow {
  std::string loadCase;
  std::string id;       // original MIDAS node/element/link id, as text
  std::string location; // "I"/"J" end for beam forces; empty for node-based categories
  std::string component;
  std::string unit;
  ComparisonResult result;
};

struct CategoryComparison {
  std::vector<ComparisonRow> rows;
  std::vector<std::string> diagnostics; // e.g. "load case 'TU' not found in results.json"
};

// `results` is the parsed "kopruq-results" document (io::writeResults's own JSON, read back with
// nlohmann::json::parse -- never re-derived or recomputed here, only looked up).
CategoryComparison matchDisplacements(const nlohmann::json& results, const ReferenceTable& ref, const ToleranceSettings& tol);
CategoryComparison matchReactions(const nlohmann::json& results, const ReferenceTable& ref, const ToleranceSettings& tol);
CategoryComparison matchBeamForces(const nlohmann::json& results, const ReferenceTable& ref, const ToleranceSettings& tol);

// Elastic links: Phase 7B's importer unconditionally rejects every MIDAS ELNK record
// (non-coincident-node elastic links are a real formulation gap in SpringLink, not a missing-
// data gap -- see MidasJsonImporter.hpp), so no element in results.json can ever correspond to
// one. Every reference row is reported Unsupported, never guessed at or silently skipped.
CategoryComparison markElasticLinksUnsupported(const ReferenceTable& ref);

} // namespace kopruq::verification
