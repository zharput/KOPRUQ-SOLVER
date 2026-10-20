#pragma once
// GUI-1: pure layout -- the four action buttons (area 2). Enabling/disabling based on
// prerequisites and wiring to real actions (validate/solve/compare/export) is GUI-2 onward.
#include <QGroupBox>
#include <QPushButton>

class AnalysisControlPanel : public QGroupBox {
  Q_OBJECT
public:
  explicit AnalysisControlPanel(QWidget* parent = nullptr);

  QPushButton* validateButton() const { return validateButton_; }
  QPushButton* runSolverButton() const { return runSolverButton_; }
  QPushButton* compareButton() const { return compareButton_; }
  QPushButton* exportButton() const { return exportButton_; }

private:
  QPushButton* validateButton_{};
  QPushButton* runSolverButton_{};
  QPushButton* compareButton_{};
  QPushButton* exportButton_{};
};
