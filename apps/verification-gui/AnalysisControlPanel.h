#pragma once
// V2.0: the four action buttons, now a fixed-width vertical sidebar on the far left of the
// window (per the revised layout), not a horizontal row inside a boxed "2. İşlemler" panel.
// Enabling/highlighting buttons based on the workflow state is MainWindow's job -- this class
// only builds the widgets and exposes a way to mark one "primary" (the current step).
#include <QPushButton>
#include <QWidget>

class AnalysisControlPanel : public QWidget {
  Q_OBJECT
public:
  explicit AnalysisControlPanel(QWidget* parent = nullptr);

  QPushButton* validateButton() const { return validateButton_; }
  QPushButton* runSolverButton() const { return runSolverButton_; }
  QPushButton* compareButton() const { return compareButton_; }
  QPushButton* exportButton() const { return exportButton_; }

  // Marks exactly one button as the highlighted "next step" (blue, per the spec's "her aşamada
  // yalnızca sıradaki işlem butonu mavi renkte vurgulanmalı") -- pass nullptr to highlight none.
  // Uses a dynamic "primary" property + QSS (not objectName), since which button is primary
  // changes at runtime.
  void setPrimaryButton(QPushButton* button);

private:
  QPushButton* validateButton_{};
  QPushButton* runSolverButton_{};
  QPushButton* compareButton_{};
  QPushButton* exportButton_{};
};
