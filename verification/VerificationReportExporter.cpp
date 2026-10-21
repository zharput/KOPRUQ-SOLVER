#include "VerificationReportExporter.h"
#include <xlnt/xlnt.hpp>

namespace kopruq::verification {

namespace {

std::string columnLetters(int oneBasedColumn) {
  std::string s;
  int n = oneBasedColumn;
  while (n > 0) {
    const int rem = (n - 1) % 26;
    s.insert(s.begin(), static_cast<char>('A' + rem));
    n = (n - 1) / 26;
  }
  return s;
}

std::string ref(int col, int row) { return columnLetters(col) + std::to_string(row); }

void setText(xlnt::worksheet& ws, int col, int row, const std::string& v) { ws.cell(ref(col, row)).value(v); }
void setNumber(xlnt::worksheet& ws, int col, int row, double v) { ws.cell(ref(col, row)).value(v); }

const char* statusText(ComparisonStatus s) { return comparisonStatusName(s); }

void writeMetadataBlock(xlnt::worksheet& ws, const ReportMetadata& m, int& row) {
  auto line = [&](const char* label, const std::string& value) {
    setText(ws, 1, row, label);
    setText(ws, 2, row, value);
    ++row;
  };
  line("Model", m.modelFileName);
  line("Solver Surumu", m.solverVersion);
  line("Calistirma Zamani", m.executionDateTimeIso);
  line("Displacements Dosyasi", m.displacementsFileName.empty() ? "(secilmedi)" : m.displacementsFileName);
  line("Reaction Dosyasi", m.reactionsFileName.empty() ? "(secilmedi)" : m.reactionsFileName);
  line("Beam Force Dosyasi", m.beamForceFileName.empty() ? "(secilmedi)" : m.beamForceFileName);
  line("Elastic Link Dosyasi", m.elasticLinkFileName.empty() ? "(secilmedi)" : m.elasticLinkFileName);
  setText(ws, 1, row, "Mutlak Tolerans Olcek Faktoru"); setNumber(ws, 2, row, m.absoluteToleranceScale); ++row;
  setText(ws, 1, row, "Bagil Tolerans"); setNumber(ws, 2, row, m.relativeTol); ++row;
  ++row;
}

void writeCategorySummaryRow(xlnt::worksheet& ws, int row, const std::string& name, const std::vector<ComparisonRow>& rows) {
  const auto stats = summarizeCategory(rows);
  const std::string status = stats.total == 0 ? "-" : (stats.fail == 0 ? "PASS" : "FAIL");
  int c = 1;
  setText(ws, c++, row, name);
  setNumber(ws, c++, row, stats.total);
  setNumber(ws, c++, row, stats.pass);
  setNumber(ws, c++, row, stats.fail);
  setNumber(ws, c++, row, stats.na);
  setNumber(ws, c++, row, stats.maxAbsDiff);
  setNumber(ws, c++, row, stats.maxAbsDiffPercent);
  setText(ws, c++, row, status);
}

// Shared by Displacements/Reactions (10 columns) -- Beam Force/Elastic Link use the 9-column
// variant below. Writes exactly the same rows already shown in the GUI, never recomputed.
void writeNodeBasedSheet(xlnt::worksheet& ws, const std::vector<ComparisonRow>& rows) {
  const char* headers[] = {"Yuk Durumu", "Dugum ID", "Bilesen", "MIDAS Degeri", "SOLVER Degeri",
                            "Isaretli Fark", "Mutlak Fark", "Bagil Fark (%)", "Birim", "Durum"};
  for (int c = 0; c < 10; ++c) setText(ws, c + 1, 1, headers[c]);
  int row = 2;
  for (const auto& r : rows) {
    const bool hasNumbers = r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail;
    int c = 1;
    setText(ws, c++, row, r.loadCase);
    setText(ws, c++, row, r.id);
    setText(ws, c++, row, r.component);
    setNumber(ws, c++, row, r.result.referenceValue);
    if (hasNumbers) {
      setNumber(ws, c++, row, r.result.solverValue);
      setNumber(ws, c++, row, r.result.signedDiff);
      setNumber(ws, c++, row, r.result.absDiff);
      setNumber(ws, c++, row, r.result.relDiff * 100.0);
    } else {
      c += 4;
    }
    setText(ws, c++, row, r.unit);
    setText(ws, c++, row, statusText(r.result.status));
    ++row;
  }
}

void writeBeamOrLinkSheet(xlnt::worksheet& ws, const std::vector<ComparisonRow>& rows) {
  const char* headers[] = {"Yuk Durumu", "ID", "Uc", "Bilesen", "MIDAS Degeri", "SOLVER Degeri", "Fark", "Birim", "Durum"};
  for (int c = 0; c < 9; ++c) setText(ws, c + 1, 1, headers[c]);
  int row = 2;
  for (const auto& r : rows) {
    const bool hasNumbers = r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail;
    int c = 1;
    setText(ws, c++, row, r.loadCase);
    setText(ws, c++, row, r.id);
    setText(ws, c++, row, r.location);
    setText(ws, c++, row, r.component);
    if (hasNumbers) {
      setNumber(ws, c++, row, r.result.referenceValue);
      setNumber(ws, c++, row, r.result.solverValue);
      setNumber(ws, c++, row, r.result.signedDiff);
    } else {
      c += 3;
    }
    setText(ws, c++, row, r.unit);
    setText(ws, c++, row, statusText(r.result.status));
    ++row;
  }
}

void writeDiagnosticsSheet(xlnt::worksheet& ws, const std::vector<std::string>& diagnostics) {
  setText(ws, 1, 1, "Aciklama");
  int row = 2;
  for (const auto& d : diagnostics) setText(ws, 1, row++, d);
}

} // namespace

bool exportVerificationReport(const std::string& outputXlsxPath, const ReportData& data, std::string& error) {
  try {
    xlnt::workbook wb;
    auto summary = wb.active_sheet();
    summary.title("Summary");
    int row = 1;
    writeMetadataBlock(summary, data.metadata, row);
    const char* catHeaders[] = {"Kategori", "Toplam", "Basarili", "Basarisiz", "N-A", "Maks Fark", "Maks Fark (%)", "Durum"};
    for (int c = 0; c < 8; ++c) setText(summary, c + 1, row, catHeaders[c]);
    ++row;
    writeCategorySummaryRow(summary, row++, "Displacements", data.displacements);
    writeCategorySummaryRow(summary, row++, "Reactions", data.reactions);
    writeCategorySummaryRow(summary, row++, "Beam Forces", data.beamForces);
    writeCategorySummaryRow(summary, row++, "Elastic Links", data.elasticLinks);

    auto displacements = wb.create_sheet();
    displacements.title("Displacements");
    writeNodeBasedSheet(displacements, data.displacements);

    auto reactions = wb.create_sheet();
    reactions.title("Reactions");
    writeNodeBasedSheet(reactions, data.reactions);

    auto beamForces = wb.create_sheet();
    beamForces.title("Beam Forces");
    writeBeamOrLinkSheet(beamForces, data.beamForces);

    auto elasticLinks = wb.create_sheet();
    elasticLinks.title("Elastic Links");
    writeBeamOrLinkSheet(elasticLinks, data.elasticLinks);

    auto diagnostics = wb.create_sheet();
    diagnostics.title("Diagnostics");
    writeDiagnosticsSheet(diagnostics, data.diagnostics);

    wb.save(outputXlsxPath);
    return true;
  } catch (const std::exception& e) {
    error = e.what();
    return false;
  }
}

} // namespace kopruq::verification
