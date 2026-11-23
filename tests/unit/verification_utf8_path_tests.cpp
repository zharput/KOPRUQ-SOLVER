// 2026-11-23 -- UTF-8 paths in the verification library (GUI build): MIDAS Excel references are
// read, and the verification report written, through a folder named with Turkish letters (model 100
// lives in "100 Basit köprü"). Dedicated new file. The fixture workbook is written with xlnt through a
// stream (independent of the code under test).
#include <gtest/gtest.h>
#include <filesystem>
#include <random>
#include <fstream>
#include <string>
#include "ExcelReferenceReader.h"
#include "VerificationReportExporter.h"
#include <xlnt/xlnt.hpp>

using namespace kopruq::verification;

namespace {
const std::string kDirUtf8 = "kopruq_xlsx_\xC3\xB6\xC3\xBC_\xC5\x9F\xC4\x9F\xC4\xB1"; // ..._öü_şğı
struct TempDir {
  std::filesystem::path path; std::string utf8;
  TempDir() {
    { const std::string u = kDirUtf8 + "_" + std::to_string(std::random_device{}()); path = std::filesystem::temp_directory_path() / std::filesystem::path(std::u8string(u.begin(), u.end())); }
    std::filesystem::create_directories(path);
    const auto u8 = path.u8string(); utf8.assign(u8.begin(), u8.end());
  }
  ~TempDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
}

TEST(VerificationUtf8Path, ExcelReferenceInNonAsciiFolderIsRead) {
  TempDir dir;
  {
    xlnt::workbook wb; auto ws = wb.active_sheet();
    ws.cell("B1").value("Displacements(Global)");
    ws.cell("C2").value("Node"); ws.cell("D2").value("Load"); ws.cell("E2").value("DX\n(m)");
    ws.cell("C3").value(1); ws.cell("D3").value("SW"); ws.cell("E3").value(0.0025);
    std::ofstream out(dir.path / "disp.xlsx", std::ios::binary);
    wb.save(out);
  }
  const auto r = readReferenceExcel(dir.utf8 + "/disp.xlsx", ReferenceFileKind::Displacements);
  ASSERT_TRUE(r.table.has_value()) << (r.diagnostics.empty() ? std::string() : r.diagnostics.front());
  ASSERT_EQ(r.table->rows.size(), 1u);
  EXPECT_DOUBLE_EQ(r.table->rows[0][r.table->columnIndex("DX")].number, 0.0025);
}

TEST(VerificationUtf8Path, ReportIsWrittenToNonAsciiFolder) {
  TempDir dir;
  ReportData data; data.metadata.modelFileName = "m.json";
  std::string error;
  ASSERT_TRUE(exportVerificationReport(dir.utf8 + "/report.xlsx", data, error)) << error;
  EXPECT_TRUE(std::filesystem::exists(dir.path / "report.xlsx"));
}
