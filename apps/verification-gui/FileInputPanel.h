#pragma once
// GUI-1: pure layout -- five file-selector rows (area 1 of the approved reference layout). File
// dialogs, extension/existence validation and QSettings persistence are GUI-2, not implemented
// here.
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

  explicit FileInputPanel(QWidget* parent = nullptr);

  QLineEdit* pathField(Row row) const { return pathFields_[static_cast<std::size_t>(row)]; }
  QPushButton* browseButton(Row row) const { return browseButtons_[static_cast<std::size_t>(row)]; }
  QLabel* statusLabel(Row row) const { return statusLabels_[static_cast<std::size_t>(row)]; }

private:
  static constexpr std::size_t kRowCount = static_cast<std::size_t>(Row::Count);
  std::array<QLineEdit*, kRowCount> pathFields_{};
  std::array<QPushButton*, kRowCount> browseButtons_{};
  std::array<QLabel*, kRowCount> statusLabels_{};
};
