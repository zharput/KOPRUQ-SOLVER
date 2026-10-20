#pragma once
// GUI-1: pure layout -- the tabbed comparison area (area 4) plus the bottom tolerance-settings
// strip. Tables are empty (headers only); populating them from a real comparison is GUI-5.
// QSettings persistence of the tolerance fields is GUI-2.
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>

class ResultsComparisonPanel : public QWidget {
  Q_OBJECT
public:
  explicit ResultsComparisonPanel(QWidget* parent = nullptr);

  QTableWidget* categoryTable() const { return categoryTable_; }
  QTableWidget* displacementsTable() const { return displacementsTable_; }
  QTableWidget* reactionsTable() const { return reactionsTable_; }
  QTableWidget* beamForcesTable() const { return beamForcesTable_; }
  QTableWidget* elasticLinksTable() const { return elasticLinksTable_; }
  QTableWidget* diagnosticsTable() const { return diagnosticsTable_; }

  QLineEdit* absoluteToleranceField() const { return absTolField_; }
  QLineEdit* relativeToleranceField() const { return relTolField_; }
  QPushButton* saveSettingsButton() const { return saveSettingsButton_; }

  // GUI-5: updates the four summary tiles. `na` is meant to include both NotApplicable and
  // Unsupported rows (the approved layout has no separate fifth tile for Unsupported) -- the
  // per-row detail tables and the category table still distinguish them precisely.
  void setSummaryCounts(int total, int pass, int fail, int na);

private:
  QWidget* buildSummaryTab();
  static QTableWidget* buildTable(const QStringList& headers, QWidget* parent);

  QTabWidget* tabs_{};
  QTableWidget* categoryTable_{};
  QTableWidget* displacementsTable_{};
  QTableWidget* reactionsTable_{};
  QTableWidget* beamForcesTable_{};
  QTableWidget* elasticLinksTable_{};
  QTableWidget* diagnosticsTable_{};

  QLabel* totalCompared_{};
  QLabel* totalPass_{};
  QLabel* totalFail_{};
  QLabel* totalNa_{};

  QLineEdit* absTolField_{};
  QLineEdit* relTolField_{};
  QPushButton* saveSettingsButton_{};
};
