#include "EigenvalueReferenceReader.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <xlnt/xlnt.hpp>

namespace kopruq::verification {

namespace {

std::string trim(const std::string& s) {
  const auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  const auto b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

// Uppercased, whitespace-stripped: lets "E I G E N V A L U E     A N A L Y S I S" (MIDAS's own
// letter-spaced title) match "EIGENVALUEANALYSIS" without caring about the spacing MIDAS used.
std::string squash(const std::string& s) {
  std::string out;
  for (char c : s) if (!std::isspace(static_cast<unsigned char>(c))) out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return out;
}

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
std::string cellReference(int oneBasedColumn, int oneBasedRow) { return columnLetters(oneBasedColumn) + std::to_string(oneBasedRow); }

bool looksNumeric(const std::string& s) {
  if (s.empty()) return false;
  char* end = nullptr;
  std::strtod(s.c_str(), &end);
  return end != s.c_str() && *end == '\0';
}

constexpr int kMaxColumns = 20;
constexpr int kMaxScanRows = 300; // generous: the real sample has ~117 rows across all sub-tables

} // namespace

EigenvalueReferenceReadResult readEigenvalueReferenceExcel(const std::string& path) {
  EigenvalueReferenceReadResult result;

  {
    std::ifstream probe(path, std::ios::binary);
    if (!probe) { result.diagnostics.push_back("Dosya açılamadı: " + path); return result; }
    unsigned char sig[4] = {0, 0, 0, 0};
    probe.read(reinterpret_cast<char*>(sig), 4);
    const bool isOle2 = sig[0] == 0xD0 && sig[1] == 0xCF && sig[2] == 0x11 && sig[3] == 0xE0;
    const bool isZip = sig[0] == 'P' && sig[1] == 'K';
    if (isOle2) {
      result.diagnostics.push_back("Bu dosya eski .xls (BIFF/OLE2) biçiminde -- yalnızca .xlsx destekleniyor. "
                                    "Lütfen MIDAS Civil NX'ten .xlsx olarak yeniden dışa aktarın: " + path);
      return result;
    }
    if (!isZip) { result.diagnostics.push_back("Tanınmayan dosya biçimi (ne .xlsx ne .xls): " + path); return result; }
  }

  xlnt::workbook wb;
  try { wb.load(path); }
  catch (const std::exception& e) { result.diagnostics.push_back(std::string("Excel dosyası okunamadı: ") + e.what()); return result; }
  xlnt::worksheet ws = wb.active_sheet();

  auto cellText = [&](int c, int r) { return trim(ws.cell(cellReference(c, r)).to_string()); };

  // Locate the frequency sub-table's title row.
  int titleRow = -1;
  for (int r = 1; r <= kMaxScanRows; ++r) {
    for (int c = 1; c <= kMaxColumns; ++c) {
      if (squash(cellText(c, r)) == "EIGENVALUEANALYSIS") { titleRow = r; break; }
    }
    if (titleRow > 0) break;
  }
  if (titleRow < 0) {
    result.diagnostics.push_back("'E I G E N V A L U E   A N A L Y S I S' başlık satırı bulunamadı: " + path);
    return result;
  }

  const int headerRow = titleRow + 1, unitRow = titleRow + 2, firstDataRow = titleRow + 3;

  // The header row only labels "Mode No" (its own two-line cell, same "name\nunit" header
  // convention as every other reference export) and "Frequency"/"Period"/"Tolerance" once each --
  // MIDAS does not repeat "Frequency" over its two unit sub-columns (rad/sec and cycle/sec), so
  // those two are told apart by the UNIT row instead, not the header row.
  int modeCol = -1, angularCol = -1, hzCol = -1, periodCol = -1;
  for (int c = 1; c <= kMaxColumns; ++c) {
    const std::string header = cellText(c, headerRow);
    if (!header.empty()) {
      const auto nl = header.find('\n');
      const std::string name = trim(nl == std::string::npos ? header : header.substr(0, nl));
      if (name == "Mode") modeCol = c;
    }
    const std::string unit = cellText(c, unitRow);
    if (unit == "(rad/sec)") angularCol = c;
    else if (unit == "(cycle/sec)") hzCol = c;
    else if (unit == "(sec)") periodCol = c;
  }
  if (modeCol < 0 || angularCol < 0 || hzCol < 0 || periodCol < 0) {
    result.diagnostics.push_back("Beklenen sütunlar bulunamadı (Mode No/Frequency(rad/sec)/Frequency(cycle/sec)/Period): " + path);
    return result;
  }

  std::vector<EigenvalueReferenceRow> rows;
  for (int r = firstDataRow; r <= firstDataRow + kMaxScanRows; ++r) {
    const std::string modeText = cellText(modeCol, r);
    if (!looksNumeric(modeText)) break; // next sub-table's title, or end of data
    EigenvalueReferenceRow row;
    row.mode = static_cast<int>(std::strtod(modeText.c_str(), nullptr) + 0.5);
    row.angular_frequency_rad_s = std::strtod(cellText(angularCol, r).c_str(), nullptr);
    row.frequency_hz = std::strtod(cellText(hzCol, r).c_str(), nullptr);
    row.period_s = std::strtod(cellText(periodCol, r).c_str(), nullptr);
    rows.push_back(row);
  }
  if (rows.empty()) { result.diagnostics.push_back("Frekans tablosunda veri satırı bulunamadı: " + path); return result; }

  // Disclose, not silently skip: the modal participation mass/factor sub-tables below the
  // frequency table (if present) are never read by this function.
  for (int r = firstDataRow + static_cast<int>(rows.size()); r <= kMaxScanRows; ++r) {
    bool found = false;
    for (int c = 1; c <= kMaxColumns; ++c)
      if (squash(cellText(c, r)).find("PARTICIPATION") != std::string::npos) { found = true; break; }
    if (found) {
      result.diagnostics.push_back(
          "Modal katılım kütlesi/faktörü tabloları dosyada mevcut ama okunmuyor -- ModalSolver'ın "
          "düz modal analizi bir taban-hareket yönü içermediğinden katılım faktörü/kütlesi "
          "hesaplamıyor (bu yalnızca ResponseSpectrumSolver'da, belirli bir yön için var); "
          "karşılaştırma yalnızca frekans/periyot ile sınırlı: " + path);
      break;
    }
  }

  result.rows = std::move(rows);
  return result;
}

} // namespace kopruq::verification
