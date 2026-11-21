#pragma once
// GUI-2: five file-selector rows (area 1), now with real QFileDialog wiring, extension/
// existence validation, and QSettings-backed persistence of the last-used path per row.
//
// Status states are deliberately limited to what a file-selection check can actually determine
// -- NotSelected/Selected/Invalid/Unsupported -- never "Valid" in the sense of a parsed,
// structurally-correct model: that is the "Modeli Doğrula" action's job (GUI-3, once the CLI is
// wired via QProcess), not this panel's. Reporting a file as "Valid" here, before anything has
// actually read it, would be exactly the kind of simulated success CLAUDE.md forbids.
#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <array>

class FileInputPanel : public QGroupBox {
  Q_OBJECT
public:
  // One row per required input file, in the approved layout's order.
  enum class Row { MidasModel, BeamForce, TrussForce, Displacements, ElasticLink, Reactions, ReactionsLocal, EigenvalueMode, Count };
  enum class Status { NotSelected, Selected, Invalid, Unsupported };

  explicit FileInputPanel(QWidget* parent = nullptr);

  QLineEdit* pathField(Row row) const { return pathFields_[static_cast<std::size_t>(row)]; }
  QPushButton* browseButton(Row row) const { return browseButtons_[static_cast<std::size_t>(row)]; }
  QLabel* statusLabel(Row row) const { return statusLabels_[static_cast<std::size_t>(row)]; }
  // Null for Row::MidasModel -- the model isn't itself a comparison category, so it has no
  // "include in comparison" checkbox.
  QCheckBox* includeCheckBox(Row row) const { return includeCheckBoxes_[static_cast<std::size_t>(row)]; }

  QString filePath(Row row) const { return pathField(row)->text(); }
  Status status(Row row) const { return statuses_[static_cast<std::size_t>(row)]; }
  // True only for Selected (right extension, file exists on disk) -- never for Invalid/
  // Unsupported/NotSelected.
  bool isUsable(Row row) const { return status(row) == Status::Selected; }

  // The one required input (area 2's "Modeli Doğrula"/"Solver'ı Çalıştır" prerequisite); the
  // four Excel reference files are each individually optional, per the approved spec.
  bool hasRequiredModel() const { return isUsable(Row::MidasModel); }

  // A usable file AND its "Karşılaştır" checkbox is checked -- lets the user pick exactly which
  // already-selected reference files actually get compared on a given run (e.g. keep all four
  // paths filled in but only tick Beam Force this time), without having to clear the others'
  // paths. Always false for Row::MidasModel (it has no checkbox -- it's the model, not a
  // comparison category).
  bool isIncludedInComparison(Row row) const;

  // Only meaningful for Row::EigenvalueMode: whether the user wants the chained modal CLI run to
  // use `--mass-type lumped` instead of the solver's long-standing default (consistent). See
  // ModalSolver.hpp's MassMatrixType -- lumped reproduces MIDAS NX's own default eigenvalue
  // analysis far more closely on the models it was validated against (Phase 7H.1, 2026-11-17).
  bool useLumpedMass() const { return lumpedMassCheckBox_ && lumpedMassCheckBox_->isChecked(); }

signals:
  // Emitted whenever any row's path or validity changes, so the main window can react (e.g.
  // enable "Modeli Doğrula" once a usable MIDAS model is selected -- GUI-3's concern, not
  // decided here).
  void selectionChanged();
  // Same moment as selectionChanged(), but names *which* row changed (path OR its "Karşılaştır"
  // checkbox) -- MainWindow's workflow-state machine reacts very differently to the MIDAS model
  // row changing (full reset back to step 1) than to an Excel row or checkbox changing (only the
  // comparison step is invalidated, model/solve results are kept) -- see V2.0's state-reset
  // rules.
  void rowChanged(Row row);

private:
  static constexpr std::size_t kRowCount = static_cast<std::size_t>(Row::Count);

  void browseFor(Row row);
  void setPath(Row row, const QString& path, bool persist);
  void revalidate(Row row);
  void restorePersistedPaths();

  std::array<QLineEdit*, kRowCount> pathFields_{};
  std::array<QPushButton*, kRowCount> browseButtons_{};
  std::array<QLabel*, kRowCount> statusLabels_{};
  std::array<QCheckBox*, kRowCount> includeCheckBoxes_{}; // null for Row::MidasModel
  QCheckBox* lumpedMassCheckBox_{}; // only for Row::EigenvalueMode -- see useLumpedMass()
  std::array<Status, kRowCount> statuses_{};
};
