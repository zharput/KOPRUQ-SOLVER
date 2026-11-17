// GUI eigenvalue row (2026-11-17): EigenvalueReferenceReader tests. Builds a small .xlsx fixture
// with xlnt itself, mirroring the real MIDAS "Eigenvalue Mode.xlsx" export structure confirmed
// against the actual sample (verification/models/13 Eigen-1): a letter-spaced title row ("E I G
// E N V A L U E   A N A L Y S I S"), a two-row header (names, then units -- "Frequency" labeled
// once but spanning two unit sub-columns, "(rad/sec)" and "(cycle/sec)"), data rows, then a
// "MODAL PARTICIPATION MASSES PRINTOUT" section this reader deliberately does not parse.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "EigenvalueReferenceReader.h"
#include <xlnt/xlnt.hpp>

using namespace kopruq::verification;

namespace {
std::filesystem::path tempXlsxPath(const char* name) { return std::filesystem::temp_directory_path() / name; }

std::filesystem::path writeEigenvalueFixture(bool includeParticipationSection) {
  xlnt::workbook wb;
  auto ws = wb.active_sheet();
  ws.cell("B1").value("Eigenvalue Mode");
  ws.cell("C3").value("E I G E N V A L U E     A N A L Y S I S");
  ws.cell("D4").value("Mode\nNo");
  ws.cell("E4").value("Frequency");
  ws.cell("I4").value("Period");
  ws.cell("E5").value("(rad/sec)");
  ws.cell("G5").value("(cycle/sec)");
  ws.cell("I5").value("(sec)");
  // Two data rows.
  ws.cell("D6").value(1); ws.cell("E6").value(30.454481); ws.cell("G6").value(4.846981); ws.cell("I6").value(0.206314);
  ws.cell("D7").value(2); ws.cell("E7").value(62.050266); ws.cell("G7").value(9.875607); ws.cell("I7").value(0.10126);
  if (includeParticipationSection) {
    ws.cell("C8").value("MODAL PARTICIPATION MASSES PRINTOUT");
    ws.cell("D9").value("Mode\nNo"); ws.cell("E9").value("TRAN-X");
    ws.cell("D10").value(1); ws.cell("E10").value("0.00");
  }
  const auto path = tempXlsxPath("kopruq_test_eigenvalue.xlsx");
  wb.save(path.string());
  return path;
}
}

TEST(EigenvalueReferenceReader, ParsesFrequencyTableFromRealExportLayout) {
  const auto path = writeEigenvalueFixture(/*includeParticipationSection=*/false);
  auto result = readEigenvalueReferenceExcel(path.string());
  ASSERT_TRUE(result.rows.has_value()) << (result.diagnostics.empty() ? "" : result.diagnostics.front());
  const auto& rows = *result.rows;
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].mode, 1);
  EXPECT_DOUBLE_EQ(rows[0].angular_frequency_rad_s, 30.454481);
  EXPECT_DOUBLE_EQ(rows[0].frequency_hz, 4.846981);
  EXPECT_DOUBLE_EQ(rows[0].period_s, 0.206314);
  EXPECT_EQ(rows[1].mode, 2);
  EXPECT_DOUBLE_EQ(rows[1].frequency_hz, 9.875607);
  std::filesystem::remove(path);
}

TEST(EigenvalueReferenceReader, StopsAtParticipationSectionAndDisclosesItWasNotParsed) {
  const auto path = writeEigenvalueFixture(/*includeParticipationSection=*/true);
  auto result = readEigenvalueReferenceExcel(path.string());
  ASSERT_TRUE(result.rows.has_value());
  EXPECT_EQ(result.rows->size(), 2u); // the participation row (mode 1, text "0.00") must not be read as a third frequency row
  bool disclosed = false;
  for (const auto& d : result.diagnostics) if (d.find("PARTICIPATION") != std::string::npos || d.find("katılım") != std::string::npos) disclosed = true;
  EXPECT_TRUE(disclosed);
  std::filesystem::remove(path);
}

TEST(EigenvalueReferenceReader, RejectsLegacyXlsWithClearDiagnostic) {
  const auto path = tempXlsxPath("kopruq_test_eigenvalue_legacy.xls");
  {
    std::ofstream f(path, std::ios::binary);
    const unsigned char ole2Sig[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    f.write(reinterpret_cast<const char*>(ole2Sig), sizeof(ole2Sig));
  }
  auto result = readEigenvalueReferenceExcel(path.string());
  EXPECT_FALSE(result.rows.has_value());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().find(".xls"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(EigenvalueReferenceReader, MissingTitleRowIsDiagnosedNotMisread) {
  xlnt::workbook wb;
  auto ws = wb.active_sheet();
  ws.cell("A1").value("not an eigenvalue export");
  const auto path = tempXlsxPath("kopruq_test_eigenvalue_notitle.xlsx");
  wb.save(path.string());
  auto result = readEigenvalueReferenceExcel(path.string());
  EXPECT_FALSE(result.rows.has_value());
  ASSERT_FALSE(result.diagnostics.empty());
  std::filesystem::remove(path);
}
