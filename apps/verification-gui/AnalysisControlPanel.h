#pragma once
// V2.0: the four action buttons, now a fixed-width vertical sidebar on the far left of the
// window (per the revised layout), not a horizontal row inside a boxed "2. İşlemler" panel.
// Enabling/highlighting buttons based on the workflow state is MainWindow's job -- this class
// only builds the widgets and exposes a way to mark one "primary" (the current step).
// V2.1: QToolButton (not QPushButton) so each button can show its icon above its label
// (Qt::ToolButtonTextUnderIcon) per the icon/visual revision spec; QToolButton is a
// QAbstractButton like QPushButton, so MainWindow's clicked()/setEnabled() usage is unaffected.
#include <QToolButton>
#include <QWidget>

class AnalysisControlPanel : public QWidget {
  Q_OBJECT
public:
  explicit AnalysisControlPanel(QWidget* parent = nullptr);

  QToolButton* validateButton() const { return validateButton_; }
  QToolButton* runSolverButton() const { return runSolverButton_; }
  QToolButton* compareButton() const { return compareButton_; }
  QToolButton* exportButton() const { return exportButton_; }

  // Marks exactly one button as the highlighted "next step" (blue, per the spec's "her aşamada
  // yalnızca sıradaki işlem butonu mavi renkte vurgulanmalı") -- pass nullptr to highlight none.
  // Uses a dynamic "primary" property + QSS (not objectName), since which button is primary
  // changes at runtime.
  void setPrimaryButton(QToolButton* button);

private:
  QToolButton* validateButton_{};
  QToolButton* runSolverButton_{};
  QToolButton* compareButton_{};
  QToolButton* exportButton_{};
};
