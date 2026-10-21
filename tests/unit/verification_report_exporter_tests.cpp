// GUI-6: VerificationReportExporter tests. Builds a small ReportData directly in memory (the
// same shape MainWindow::onExport() hands it -- no recomputation here, this file only checks
// that what goes in comes back out of the written .xlsx unchanged) and reads the written file
// back with xlnt to verify real content, not just "a file exists".
#include <gtest/gtest.h>
#include <filesystem>
#include "VerificationReportExporter.h"
#include <xlnt/xlnt.hpp>

using namespace kopruq::verification;

namespace {
ReportData sampleReport() {
  ReportData data;
  data.metadata.modelFileName = "deneme1.json";
  data.metadata.solverVersion = "0.1.0";
  data.metadata.executionDateTimeIso = "2026-10-21T10:00:00";
  data.metadata.displacementsFileName = "Displacements(Global).xlsx";
  data.metadata.absoluteToleranceScale = 2.0;
  data.metadata.relativeTol = 0.01;

  ComparisonRow pass;
  pass.loadCase = "SW";
  pass.id = "1";
  pass.component = "DX";
  pass.unit = "(m)";
  pass.result.status = ComparisonStatus::Pass;
  pass.result.referenceValue = 0.1;
  pass.result.solverValue = 0.1001;
  pass.result.signedDiff = 0.0001;
  pass.result.absDiff = 0.0001;
  pass.result.relDiff = 0.001;
  data.displacements.push_back(pass);

  ComparisonRow unsupported;
  unsupported.loadCase = "SW";
  unsupported.id = "1";
  unsupported.location = "26";
  unsupported.result.status = ComparisonStatus::Unsupported;
  data.elasticLinks.push_back(unsupported);

  data.diagnostics.push_back("Beam Force: dosya secilmedi, bu kategori karsilastirilmadi.");
  return data;
}
}

TEST(VerificationReportExporter, WritesAFileThatXlntCanReadBack) {
  const auto path = std::filesystem::temp_directory_path() / "kopruq_test_report.xlsx";
  std::string error;
  ASSERT_TRUE(exportVerificationReport(path.string(), sampleReport(), error)) << error;
  ASSERT_TRUE(std::filesystem::exists(path));

  xlnt::workbook wb;
  ASSERT_NO_THROW(wb.load(path.string()));
  std::filesystem::remove(path);
}

TEST(VerificationReportExporter, SummarySheetContainsMetadataAndCategoryTotals) {
  const auto path = std::filesystem::temp_directory_path() / "kopruq_test_report2.xlsx";
  std::string error;
  ASSERT_TRUE(exportVerificationReport(path.string(), sampleReport(), error)) << error;

  xlnt::workbook wb;
  wb.load(path.string());
  auto ws = wb.sheet_by_title("Summary");
  EXPECT_EQ(ws.cell("A1").to_string(), "Model");
  EXPECT_EQ(ws.cell("B1").to_string(), "deneme1.json");

  bool foundDisplacementsRow = false;
  for (auto row : ws.rows(false)) {
    if (!row.empty() && row[0].to_string() == "Displacements") {
      foundDisplacementsRow = true;
      // 1 PASS row in data.displacements -> Toplam=1, Basarili=1, Basarisiz=0.
      EXPECT_EQ(row[1].to_string(), "1");
      EXPECT_EQ(row[2].to_string(), "1");
      EXPECT_EQ(row[3].to_string(), "0");
    }
  }
  EXPECT_TRUE(foundDisplacementsRow);
  std::filesystem::remove(path);
}

TEST(VerificationReportExporter, AllSixWorksheetsArePresent) {
  const auto path = std::filesystem::temp_directory_path() / "kopruq_test_report3.xlsx";
  std::string error;
  ASSERT_TRUE(exportVerificationReport(path.string(), sampleReport(), error)) << error;

  xlnt::workbook wb;
  wb.load(path.string());
  const std::vector<std::string> expected{"Summary", "Displacements", "Reactions", "Beam Forces", "Elastic Links", "Diagnostics"};
  for (const auto& name : expected) EXPECT_TRUE(wb.contains(name)) << name;
  std::filesystem::remove(path);
}

TEST(VerificationReportExporter, UnsupportedRowsLeaveNumericCellsBlankNotZero) {
  const auto path = std::filesystem::temp_directory_path() / "kopruq_test_report4.xlsx";
  std::string error;
  ASSERT_TRUE(exportVerificationReport(path.string(), sampleReport(), error)) << error;

  xlnt::workbook wb;
  wb.load(path.string());
  auto ws = wb.sheet_by_title("Elastic Links");
  // Row 2 is the one Unsupported elastic-link row; its MIDAS/SOLVER/Fark cells (E,F,G) must be
  // blank, not a fabricated 0.
  EXPECT_TRUE(ws.cell("E2").to_string().empty());
  EXPECT_TRUE(ws.cell("F2").to_string().empty());
  EXPECT_EQ(ws.cell("I2").to_string(), "UNSUPPORTED");
  std::filesystem::remove(path);
}

TEST(VerificationReportExporter, FailsGracefullyOnUnwritablePathRatherThanCrashing) {
  std::string error;
  const bool ok = exportVerificationReport("/definitely/not/a/writable/directory/report.xlsx", sampleReport(), error);
  EXPECT_FALSE(ok);
  EXPECT_FALSE(error.empty());
}
