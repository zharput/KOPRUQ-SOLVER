#pragma once
// Reads MIDAS Civil NX's "Eigenvalue Mode.xlsx" export -- structurally different from every
// other reference export (ExcelReferenceReader.h): instead of one header row followed by a flat
// data block, it stacks several distinct, independently-titled sub-tables in one sheet ("E I G E
// N V A L U E   A N A L Y S I S" frequency table, then two "MODAL PARTICIPATION MASSES PRINTOUT"
// blocks, then a "MODAL PARTICIPATION FACTOR PRINTOUT" block -- confirmed from the real export,
// `verification/models/13 Eigen-1/Eigenvalue Mode.xlsx`, 2026-11-17).
//
// Only the first (frequency) table is read here: Mode No / Frequency (rad/sec and cycle/sec) /
// Period (sec) -- the quantities `kopruq::ModalSolver`/`kopruq-modal-results` directly produce
// (`angular_frequency_rad_s`, `frequency_hz`), so they can be compared without any new solver
// capability. The modal participation mass/factor tables are explicitly NOT read: ModalSolver's
// plain modal analysis has no base-excitation direction, so it computes no participation
// factor/mass at all (that concept only exists inside ResponseSpectrumSolver, driven by a
// specific direction) -- comparing them would need a new, separate solver capability, not
// something this reader can respect by simply parsing more columns. Disclosed, not silently
// dropped: `EigenvalueReferenceReadResult::diagnostics` always notes this when the file has those
// sections.
#include <optional>
#include <string>
#include <vector>

namespace kopruq::verification {

struct EigenvalueReferenceRow {
  int mode{};
  double angular_frequency_rad_s{}, frequency_hz{}, period_s{};
};

struct EigenvalueReferenceReadResult {
  std::optional<std::vector<EigenvalueReferenceRow>> rows; // nullopt on a fatal read failure (see diagnostics[0])
  std::vector<std::string> diagnostics;
};

// path: an .xlsx file (OOXML); a legacy .xls is rejected with a clear diagnostic, same policy as
// ExcelReferenceReader.h.
EigenvalueReferenceReadResult readEigenvalueReferenceExcel(const std::string& path);

} // namespace kopruq::verification
