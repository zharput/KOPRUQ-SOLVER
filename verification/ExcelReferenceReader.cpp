#include "ExcelReferenceReader.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <xlnt/xlnt.hpp>

namespace kopruq::verification {

const char* referenceFileKindName(ReferenceFileKind kind) {
  switch (kind) {
    case ReferenceFileKind::BeamForce: return "Beam Force";
    case ReferenceFileKind::Displacements: return "Displacements(Global)";
    case ReferenceFileKind::Reaction: return "Reaction(Global)";
    case ReferenceFileKind::ElasticLink: return "Elastic Link";
  }
  return "?";
}

int ReferenceTable::columnIndex(const std::string& name) const {
  auto lower = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
  };
  const std::string target = lower(name);
  for (std::size_t i = 0; i < columns.size(); ++i)
    if (lower(columns[i].name) == target) return static_cast<int>(i);
  return -1;
}

namespace {

std::string trim(const std::string& s) {
  const auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  const auto b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

// Spreadsheet column letters: 1 -> A, 26 -> Z, 27 -> AA, ... -- elementary arithmetic, no xlnt
// dependency, so an upper bound on columns can be scanned without relying on
// worksheet::highest_column()'s exact return type across xlnt versions.
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

std::string cellReference(int oneBasedColumn, int oneBasedRow) {
  return columnLetters(oneBasedColumn) + std::to_string(oneBasedRow);
}

bool looksNumeric(const std::string& s) {
  if (s.empty()) return false;
  char* end = nullptr;
  std::strtod(s.c_str(), &end);
  return end != s.c_str() && *end == '\0';
}

constexpr int kMaxColumns = 40;
constexpr int kMaxHeaderScanRows = 10;
constexpr int kMaxDataRows = 20000;

} // namespace

ReferenceReadResult readReferenceExcel(const std::string& path, ReferenceFileKind expectedKind) {
  ReferenceReadResult result;

  // Sniff the real file format instead of trusting the extension: a legacy .xls (BIFF/OLE2,
  // signature D0 CF 11 E0) is rejected with a clear, actionable diagnostic -- per the
  // 2026-10-20 decision, only .xlsx (OOXML, a ZIP container, signature "PK") is supported.
  {
    std::ifstream probe(path, std::ios::binary);
    if (!probe) {
      result.diagnostics.push_back("Dosya açılamadı: " + path);
      return result;
    }
    unsigned char sig[4] = {0, 0, 0, 0};
    probe.read(reinterpret_cast<char*>(sig), 4);
    const bool isOle2 = sig[0] == 0xD0 && sig[1] == 0xCF && sig[2] == 0x11 && sig[3] == 0xE0;
    const bool isZip = sig[0] == 'P' && sig[1] == 'K';
    if (isOle2) {
      result.diagnostics.push_back(
          "Bu dosya eski .xls (BIFF/OLE2) biçiminde -- yalnızca .xlsx destekleniyor. "
          "Lütfen MIDAS Civil NX'ten .xlsx olarak yeniden dışa aktarın: " + path);
      return result;
    }
    if (!isZip) {
      result.diagnostics.push_back("Tanınmayan dosya biçimi (ne .xlsx ne .xls): " + path);
      return result;
    }
  }

  xlnt::workbook wb;
  try {
    wb.load(path);
  } catch (const std::exception& e) {
    result.diagnostics.push_back(std::string("Excel dosyası okunamadı: ") + e.what());
    return result;
  }

  xlnt::worksheet ws = wb.active_sheet();

  // Locate the header row: MIDAS's exports have a title row above it (merged cell with the
  // sheet name), so the header is not assumed to be row 1 -- the first row with >= 3 non-empty,
  // non-numeric-looking cells is taken as the header, never a hardcoded row index.
  int headerRow = -1;
  for (int r = 1; r <= kMaxHeaderScanRows && headerRow < 0; ++r) {
    int textCells = 0;
    for (int c = 1; c <= kMaxColumns; ++c) {
      const std::string v = trim(ws.cell(cellReference(c, r)).to_string());
      if (!v.empty() && !looksNumeric(v)) ++textCells;
    }
    if (textCells >= 3) headerRow = r;
  }
  if (headerRow < 0) {
    result.diagnostics.push_back("Başlık satırı bulunamadı: " + path);
    return result;
  }

  // Columns: skip MIDAS's leading blank columns, then take the contiguous run of non-empty
  // header cells as the table's width -- never a fixed column count, since Beam Force (23),
  // Elastic Link (19) and Displacements/Reaction (15) all differ.
  int startCol = -1, endCol = -1;
  for (int c = 1; c <= kMaxColumns; ++c) {
    const std::string v = trim(ws.cell(cellReference(c, headerRow)).to_string());
    if (startCol < 0) {
      if (!v.empty()) startCol = c;
    } else if (v.empty()) {
      endCol = c - 1;
      break;
    }
  }
  if (startCol < 0) {
    result.diagnostics.push_back("Başlık satırında hiç sütun bulunamadı: " + path);
    return result;
  }
  if (endCol < 0) endCol = kMaxColumns; // header ran all the way to the scan limit

  ReferenceTable table;
  table.kind = expectedKind;
  for (int c = startCol; c <= endCol; ++c) {
    const std::string raw = ws.cell(cellReference(c, headerRow)).to_string();
    const auto nl = raw.find('\n');
    ReferenceColumn col;
    if (nl == std::string::npos) {
      col.name = trim(raw);
    } else {
      col.name = trim(raw.substr(0, nl));
      col.unit = trim(raw.substr(nl + 1));
    }
    table.columns.push_back(std::move(col));
  }

  bool hitScanLimit = true;
  for (int r = headerRow + 1; r <= headerRow + kMaxDataRows; ++r) {
    bool rowEmpty = true;
    std::vector<CellValue> row;
    row.reserve(table.columns.size());
    for (int c = startCol; c <= endCol; ++c) {
      xlnt::cell cell = ws.cell(cellReference(c, r));
      const std::string text = cell.to_string();
      CellValue cv;
      if (!text.empty()) {
        rowEmpty = false;
        cv.isEmpty = false;
        if (cell.data_type() == xlnt::cell_type::number) {
          cv.isNumber = true;
          cv.number = cell.value<double>();
          cv.text = text;
        } else {
          cv.text = text;
        }
      }
      row.push_back(std::move(cv));
    }
    if (rowEmpty) { hitScanLimit = false; break; }
    table.rows.push_back(std::move(row));
  }
  if (hitScanLimit)
    result.diagnostics.push_back(
        "Satır tarama sınırına (" + std::to_string(kMaxDataRows) + ") ulaşıldı -- dosyada daha "
        "fazla veri olabilir, kesilmiş olabilir: " + path);

  if (table.rows.empty())
    result.diagnostics.push_back(std::string(referenceFileKindName(expectedKind)) + ": veri satırı bulunamadı (yalnızca başlık okundu): " + path);

  result.table = std::move(table);
  return result;
}

} // namespace kopruq::verification
