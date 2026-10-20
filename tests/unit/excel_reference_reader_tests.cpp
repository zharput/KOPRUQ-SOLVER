// GUI-4: ExcelReferenceReader tests. Builds small .xlsx fixtures with xlnt itself (round-trip),
// mirroring the real MIDAS export structure confirmed against the actual sample files in
// xmidas/ during the GUI-0/GUI-4 audit: a title row, then a header row whose cells are
// "Name\n(unit)", then data rows, with MIDAS's own leading blank columns before the first
// labeled column.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "ExcelReferenceReader.h"
#include <xlnt/xlnt.hpp>

using namespace kopruq::verification;

namespace {
std::filesystem::path tempXlsxPath(const char* name) {
  return std::filesystem::temp_directory_path() / name;
}

// Mirrors Displacements(Global).xls's real column layout (col A,B blank; Node, Load, Stage,
// Step, Step Order, DX(m), DY(m), Load Order), trimmed to a handful of columns for brevity.
std::filesystem::path writeDisplacementsFixture() {
  xlnt::workbook wb;
  auto ws = wb.active_sheet();
  ws.cell("B1").value("Displacements(Global)");
  ws.cell("C2").value("Node");
  ws.cell("D2").value("Load");
  ws.cell("E2").value("DX\n(m)");
  ws.cell("F2").value("DY\n(m)");
  ws.cell("C3").value(1);
  ws.cell("D3").value("SW");
  ws.cell("E3").value(0.0);
  ws.cell("F3").value(0.00022791048104409128);
  ws.cell("C4").value(2);
  ws.cell("D4").value("SW");
  ws.cell("E4").value(0.0);
  ws.cell("F4").value(0.00033520543365739286);
  const auto path = tempXlsxPath("kopruq_test_displacements.xlsx");
  wb.save(path.string());
  return path;
}
}

TEST(ExcelReferenceReader, ParsesHeaderNamesAndUnitsIgnoringMidasLeadingBlankColumns) {
  const auto path = writeDisplacementsFixture();
  auto result = readReferenceExcel(path.string(), ReferenceFileKind::Displacements);
  ASSERT_TRUE(result.table.has_value()) << (result.diagnostics.empty() ? "" : result.diagnostics.front());
  const auto& t = *result.table;
  ASSERT_EQ(t.columns.size(), 4u);
  EXPECT_EQ(t.columns[0].name, "Node");
  EXPECT_TRUE(t.columns[0].unit.empty());
  EXPECT_EQ(t.columns[2].name, "DX");
  EXPECT_EQ(t.columns[2].unit, "(m)");
  std::filesystem::remove(path);
}

TEST(ExcelReferenceReader, ColumnIndexLookupIsCaseInsensitiveAndPositionIndependent) {
  const auto path = writeDisplacementsFixture();
  auto result = readReferenceExcel(path.string(), ReferenceFileKind::Displacements);
  ASSERT_TRUE(result.table.has_value());
  EXPECT_EQ(result.table->columnIndex("dx"), 2);
  EXPECT_EQ(result.table->columnIndex("DY"), 3);
  EXPECT_EQ(result.table->columnIndex("Nonexistent"), -1);
  std::filesystem::remove(path);
}

TEST(ExcelReferenceReader, ReadsAllDataRowsWithCorrectNumericAndTextValues) {
  const auto path = writeDisplacementsFixture();
  auto result = readReferenceExcel(path.string(), ReferenceFileKind::Displacements);
  ASSERT_TRUE(result.table.has_value());
  const auto& t = *result.table;
  ASSERT_EQ(t.rows.size(), 2u);

  const int nodeCol = t.columnIndex("Node");
  const int loadCol = t.columnIndex("Load");
  const int dyCol = t.columnIndex("DY");

  EXPECT_TRUE(t.rows[0][nodeCol].isNumber);
  EXPECT_DOUBLE_EQ(t.rows[0][nodeCol].number, 1.0);
  EXPECT_FALSE(t.rows[0][loadCol].isNumber);
  EXPECT_EQ(t.rows[0][loadCol].text, "SW");
  EXPECT_TRUE(t.rows[1][dyCol].isNumber);
  EXPECT_DOUBLE_EQ(t.rows[1][dyCol].number, 0.00033520543365739286);
  std::filesystem::remove(path);
}

TEST(ExcelReferenceReader, RejectsLegacyXlsWithClearDiagnosticRatherThanMisreading) {
  const auto path = tempXlsxPath("kopruq_test_legacy.xls");
  {
    std::ofstream f(path, std::ios::binary);
    const unsigned char ole2Sig[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    f.write(reinterpret_cast<const char*>(ole2Sig), sizeof(ole2Sig));
  }
  auto result = readReferenceExcel(path.string(), ReferenceFileKind::Displacements);
  EXPECT_FALSE(result.table.has_value());
  ASSERT_FALSE(result.diagnostics.empty());
  EXPECT_NE(result.diagnostics.front().find(".xls"), std::string::npos);
  std::filesystem::remove(path);
}

TEST(ExcelReferenceReader, MissingFileIsDiagnosedNotCrashed) {
  auto result = readReferenceExcel("/definitely/not/a/real/path.xlsx", ReferenceFileKind::Reaction);
  EXPECT_FALSE(result.table.has_value());
  ASSERT_FALSE(result.diagnostics.empty());
}

TEST(ExcelReferenceReader, ReferenceFileKindNameMatchesMidasOwnLabels) {
  EXPECT_STREQ(referenceFileKindName(ReferenceFileKind::BeamForce), "Beam Force");
  EXPECT_STREQ(referenceFileKindName(ReferenceFileKind::Displacements), "Displacements(Global)");
  EXPECT_STREQ(referenceFileKindName(ReferenceFileKind::Reaction), "Reaction(Global)");
  EXPECT_STREQ(referenceFileKindName(ReferenceFileKind::ElasticLink), "Elastic Link");
}
