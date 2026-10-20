#pragma once
// GUI-4: reads a MIDAS Civil NX Excel result export (.xlsx) into a generic, header-driven table.
// Qt-independent by design (apps/verification-gui links against this, but this file never
// includes a Qt header) -- see docs/SOLVER_ROADMAP.md's "QT6 VERIFICATION GUI" section 4.
//
// MIDAS's own exports (inspected directly against the real samples in xmidas/, GUI-0/GUI-4
// audit) do not put data in fixed column positions in any contract sense -- different result
// categories have different column counts/orders (Beam Force: 23 columns; Displacements/
// Reaction: 15; Elastic Link: 19), and nothing guarantees a future MIDAS version won't add or
// reorder columns. So this reader locates every field by its header text, never by a hardcoded
// column index, and exposes both: the full row as read (nothing dropped) and name-based lookup.
//
// Each result category's header is a single text row (not necessarily the file's first row --
// MIDAS's actual exports have a title row above it) containing entries like "DX\n(m)" -- this
// reader treats the first line of a multi-line header cell as the field name and the rest
// (typically a parenthesized unit) as the unit, never discarding the unit, since the approved
// comparison tables have a dedicated "Birim" column.
#include <optional>
#include <string>
#include <vector>

namespace kopruq::verification {

enum class ReferenceFileKind { BeamForce, Displacements, Reaction, ElasticLink };

// What MIDAS's own export literally calls each result category, used only for log/diagnostic
// text (never for parsing decisions -- the actual column set is discovered from the file).
const char* referenceFileKindName(ReferenceFileKind kind);

struct CellValue {
  bool isEmpty{true};
  bool isNumber{false};
  double number{0.0};
  std::string text; // populated whenever the cell holds text, including numbers MIDAS exported
                     // as text (e.g. some "Part"/"Component" columns) -- never silently dropped
};

struct ReferenceColumn {
  std::string name; // header text before the first newline, trimmed
  std::string unit; // header text after the first newline (often "(kN)", "(m)", "([rad])"), trimmed; empty if none
};

struct ReferenceTable {
  ReferenceFileKind kind;
  std::vector<ReferenceColumn> columns;
  std::vector<std::vector<CellValue>> rows; // rows[r][c], same order/width as `columns`

  // Case-insensitive match on the column's name (not the unit); returns -1 if no column has
  // that name, so a caller can report a missing expected field rather than reading garbage.
  int columnIndex(const std::string& name) const;
};

struct ReferenceReadResult {
  std::optional<ReferenceTable> table; // empty on a fatal read failure (see diagnostics[0])
  std::vector<std::string> diagnostics; // non-fatal notes (e.g. a sheet with zero data rows) and,
                                         // on failure, the one fatal reason
};

// path: an .xlsx file (OOXML). A legacy .xls (BIFF/OLE2) file is rejected with a clear
// diagnostic, not guessed at or silently misread -- per the 2026-10-20 decision to support only
// .xlsx (see docs/SOLVER_ROADMAP.md); re-export from MIDAS Civil NX as .xlsx if needed.
ReferenceReadResult readReferenceExcel(const std::string& path, ReferenceFileKind expectedKind);

} // namespace kopruq::verification
