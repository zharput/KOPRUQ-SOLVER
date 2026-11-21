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
#include "EigenvalueReferenceReader.h"
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

// 2026-11-21. MIDAS "Reaction(Local)": reactions in each node's local axes (node "reaction_local" in
// results.json, written only for nodes with local axes; any other node's local reaction is its
// global "reaction"). Columns Fx/Fy/Fz/Mx/My/Mz.
CategoryComparison matchLocalReactions(const nlohmann::json& results, const ReferenceTable& ref, const ToleranceSettings& tol);

// 2026-11-21. MIDAS "Elastic Link" forces (No./Load/Node, Axial..Moment-z in link-local axes, the
// same internal force on both node rows) against results.json "springs" local_end_forces[6..11].
CategoryComparison matchElasticLinks(const nlohmann::json& results, const ReferenceTable& ref, const ToleranceSettings& tol);

// Response spectrum (2026-11-20): appends every successful case of a response-spectrum results
// document (kopruq::io::writeResponseSpectrumResults, the CLI's `spectrum` command) to
// results["load_cases"] under MIDAS's own load-case name "<name>(RS)", with its combined nodal
// displacements/reactions and element local_end_forces, flagged "peak_magnitude": true. A CQC/SRSS
// peak is a magnitude, so for such a case the three matchers above compare |KOPRUQ| with |MIDAS|
// (and apply no beam-force sign convention). Cases already present by that name are not added twice.
void appendSpectrumCases(nlohmann::json& results, const nlohmann::json& spectrumResults);

// MIDAS's "Truss Force" export (TYPE=="TRUSS" elements only, see MidasJsonImporter.hpp): a
// single axial force per element per load case, given as separate "Force-I"/"Force-J" columns
// in the SAME row (unlike Beam Force's separate-row-per-end "Part" column layout) -- for a
// pin-ended truss member, Force-I and Force-J are numerically identical (constant axial force
// along the member, since all transverse/bending terms are released, see Truss3D.hpp), so both
// are compared independently against the solver's own recovered local_end_forces[0]/[6].
CategoryComparison matchTrussForces(const nlohmann::json& results, const ReferenceTable& ref, const ToleranceSettings& tol);

struct CategoryStats { int total{}, pass{}, fail{}, na{}; double maxAbsDiff{}, maxAbsDiffPercent{}; };

// Shared by the GUI's Özet tab (MainWindow::onCompare) and VerificationReportExporter's Summary
// sheet -- one implementation, so the two can never disagree (per the task spec: never maintain
// separate, inconsistent comparison calculations for the GUI and the report). `na` folds in both
// NotApplicable and Unsupported rows, matching the approved UI's single N/A tile.
CategoryStats summarizeCategory(const std::vector<ComparisonRow>& rows);

// Marks every Elastic Link reference row Unsupported. Since 2026-11-21 the GUI compares elastic
// links with matchElasticLinks (above); this is kept for callers that cannot compare them.
CategoryComparison markElasticLinksUnsupported(const ReferenceTable& ref);

// `modalResults` is `kopruq-modal-results` (io::writeModalResults's own JSON, read back with
// nlohmann::json::parse). Matches by mode index (`eigenRef[i].mode` is 1-based, `modalResults`'s
// own "index" field is 0-based -- see ModalResultsJson.hpp -- so `mode - 1` is the lookup key, not
// position in the vector, in case a future reference export's modes are ever out of order).
// Every reference mode produces two ComparisonRows (angular frequency, frequency in Hz); modes
// beyond what ModalSolver computed (fewer modes requested than MIDAS's reference has) report
// NotApplicable, never silently dropped.
CategoryComparison matchEigenvalueFrequencies(const nlohmann::json& modalResults,
                                               const std::vector<EigenvalueReferenceRow>& eigenRef,
                                               const ToleranceSettings& tol);

} // namespace kopruq::verification
