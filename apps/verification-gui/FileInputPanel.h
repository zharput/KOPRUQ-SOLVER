#pragma once
// GUI-2: five file-selector rows (area 1), now with real QFileDialog wiring, extension/
// existence validation, and QSettings-backed persistence of the last-used path per row.
//
// Status states are deliberately limited to what a file-selection check can actually determine
// -- NotSelected/Selected/Invalid/Unsupported -- never "Valid" in the sense of a parsed,
// structurally-correct model: that is the "Modeli Doğrula" action's job (GUI-3, once the CLI is
// wired via QProcess), not this panel's. Reporting a file as "Valid" here, before anything has
// actually read it, would be exactly the kind of simulated success CLAUDE.md forbids.
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <array>

class FileInputPanel : public QGroupBox {
  Q_OBJECT
public:
  // One row per required input file, in the approved layout's order.
  enum class Row { MidasModel, BeamForce, Displacements, ElasticLink, Reactions, Count };
  enum class Status { NotSelected, Selected, Invalid, Unsupported };

  explicit FileInputPanel(QWidget* parent = nullptr);

  QLineEdit* pathField(Row row) const { return pathFields_[static_cast<std::size_t>(row)]; }
  QPushButton* browseButton(Row row) const { return browseButtons_[static_cast<std::size_t>(row)]; }
  QLabel* statusLabel(Row row) const { return statusLabels_[static_cast<std::size_t>(row)]; }

  QString filePath(Row row) const { return pathField(row)->text(); }
  Status status(Row row) const { return statuses_[static_cast<std::size_t>(row)]; }
  // True only for Selected (right extension, file exists on disk) -- never for Invalid/
  // Unsupported/NotSelected.
  bool isUsable(Row row) const { return status(row) == Status::Selected; }

  // The one required input (area 2's "Modeli Doğrula"/"Solver'ı Çalıştır" prerequisite); the
  // four Excel reference files are each individually optional, per the approved spec.
  bool hasRequiredModel() const { return isUsable(Row::MidasModel); }

signals:
  // Emitted whenever any row's path or validity changes, so the main window can react (e.g.
  // enable "Modeli Doğrula" once a usable MIDAS model is selected -- GUI-3's concern, not
  // decided here).
  void selectionChanged();

private:
  static constexpr std::size_t kRowCount = static_cast<std::size_t>(Row::Count);

  void browseFor(Row row);
  void setPath(Row row, const QString& path, bool persist);
  void revalidate(Row row);
  void restorePersistedPaths();

  std::array<QLineEdit*, kRowCount> pathFields_{};
  std::array<QPushButton*, kRowCount> browseButtons_{};
  std::array<QLabel*, kRowCount> statusLabels_{};
  std::array<Status, kRowCount> statuses_{};
};
