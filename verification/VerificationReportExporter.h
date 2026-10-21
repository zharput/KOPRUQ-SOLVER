#pragma once
// GUI-6: writes KOPRUQ_SOLVER_Verification_Report.xlsx from the SAME comparison results already
// computed by GUI-5's ResultMatcher/ComparisonEngine and already shown in the GUI -- this file
// never recomputes a comparison, it only serializes rows/diagnostics the caller already has
// (per the task spec: "Do not maintain separate, inconsistent comparison calculations for the
// GUI and report"). Qt-independent, like the rest of verification/.
#include "ComparisonEngine.h"
#include "ResultMatcher.h"
#include <string>
#include <vector>

namespace kopruq::verification {

struct ReportMetadata {
  std::string modelFileName;
  std::string solverVersion;          // from results.json's metadata.solver_version, not re-derived
  std::string executionDateTimeIso;   // when the comparison that produced this data was run
  std::string displacementsFileName;  // empty if that category's file was not selected/usable
  std::string reactionsFileName;
  std::string beamForceFileName;
  std::string elasticLinkFileName;
  double absoluteToleranceScale{1.0};
  double relativeTol{0.0};
};

struct ReportData {
  ReportMetadata metadata;
  std::vector<ComparisonRow> displacements, reactions, beamForces, elasticLinks;
  // Exactly the diagnostics already aggregated and shown in the GUI's Diagnostik tab -- not
  // re-collected here, so the report and the GUI can never disagree with each other.
  std::vector<std::string> diagnostics;
};

// Returns true on success; on failure, `error` carries a human-readable reason (never throws
// past this boundary, so a GUI caller can log it instead of crashing on a write failure, e.g. a
// file open in Excel).
bool exportVerificationReport(const std::string& outputXlsxPath, const ReportData& data, std::string& error);

} // namespace kopruq::verification
