// GUI eigenvalue row (2026-11-17): matchEigenvalueFrequencies tests. Builds a `kopruq-modal-
// results` JSON fragment directly (the real shape produced by io::writeModalResults,
// include/kopruq/io/ModalResultsJson.hpp) and EigenvalueReferenceRow values directly in memory
// (no Excel I/O needed; EigenvalueReferenceReader's own tests already cover the file-reading
// path).
#include <gtest/gtest.h>
#include "ResultMatcher.h"

using namespace kopruq::verification;
using Json = nlohmann::json;

namespace {
Json sampleModalResults() {
  return Json::parse(R"({
    "status": {"success": true},
    "modes": [
      {"index": 0, "frequency_hz": 4.846981, "angular_frequency_rad_s": 30.454481},
      {"index": 1, "frequency_hz": 9.9197, "angular_frequency_rad_s": 62.321}
    ]
  })");
}
}

TEST(EigenvalueResultMatcher, MatchesByOneBasedModeNumberAgainstZeroBasedSolverIndex) {
  std::vector<EigenvalueReferenceRow> ref = {
      {/*mode=*/1, /*angular=*/30.454481, /*hz=*/4.846981, /*period=*/0.206314},
  };
  auto cmp = matchEigenvalueFrequencies(sampleModalResults(), ref, ToleranceSettings{});
  ASSERT_EQ(cmp.rows.size(), 2u); // Hz + rad/s rows for the one reference mode
  for (const auto& row : cmp.rows) {
    EXPECT_EQ(row.id, "1");
    EXPECT_EQ(row.result.status, ComparisonStatus::Pass) << row.component;
  }
}

TEST(EigenvalueResultMatcher, FailsOnARealMismatchRatherThanBeingToleranceWashed) {
  std::vector<EigenvalueReferenceRow> ref = {
      {/*mode=*/2, /*angular=*/62.321, /*hz=*/9.9197, /*period=*/0.1008}, // matches solver mode index 1 exactly
  };
  // Introduce a genuine mismatch: solver's second mode is actually 9.9197 Hz, reference claims 20.0 Hz.
  std::vector<EigenvalueReferenceRow> mismatched = {{/*mode=*/2, /*angular=*/62.321, /*hz=*/20.0, /*period=*/0.05}};
  auto cmp = matchEigenvalueFrequencies(sampleModalResults(), mismatched, ToleranceSettings{});
  bool anyFail = false;
  for (const auto& row : cmp.rows) if (row.result.status == ComparisonStatus::Fail) anyFail = true;
  EXPECT_TRUE(anyFail);
  (void)ref;
}

TEST(EigenvalueResultMatcher, ReferenceModeBeyondSolverOutputIsNotApplicableNotDropped) {
  std::vector<EigenvalueReferenceRow> ref = {{/*mode=*/5, /*angular=*/1.0, /*hz=*/1.0, /*period=*/1.0}}; // index 4, not in sampleModalResults' 2 modes
  auto cmp = matchEigenvalueFrequencies(sampleModalResults(), ref, ToleranceSettings{});
  ASSERT_EQ(cmp.rows.size(), 2u);
  for (const auto& row : cmp.rows) EXPECT_EQ(row.result.status, ComparisonStatus::NotApplicable);
}

TEST(EigenvalueResultMatcher, FailedModalAnalysisProducesNotApplicableAndADiagnostic) {
  Json failed = Json::parse(R"({"status": {"success": false}, "modes": []})");
  std::vector<EigenvalueReferenceRow> ref = {{1, 30.0, 4.8, 0.2}};
  auto cmp = matchEigenvalueFrequencies(failed, ref, ToleranceSettings{});
  for (const auto& row : cmp.rows) EXPECT_EQ(row.result.status, ComparisonStatus::NotApplicable);
  EXPECT_FALSE(cmp.diagnostics.empty());
}
