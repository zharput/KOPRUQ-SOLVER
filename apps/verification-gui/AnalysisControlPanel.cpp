#include "AnalysisControlPanel.h"
#include <QGridLayout>

AnalysisControlPanel::AnalysisControlPanel(QWidget* parent) : QGroupBox(tr("2. İşlemler"), parent) {
  auto* grid = new QGridLayout(this);
  grid->setHorizontalSpacing(8);

  validateButton_ = new QPushButton(tr("Modeli Doğrula"), this);
  runSolverButton_ = new QPushButton(tr("Solver'ı Çalıştır"), this);
  runSolverButton_->setObjectName("primaryAction");
  compareButton_ = new QPushButton(tr("Sonuçları Karşılaştır"), this);
  exportButton_ = new QPushButton(tr("Raporu Dışa Aktar"), this);

  // GUI-1 has no prerequisite tracking yet -- buttons are not yet wired to any action, so they
  // start disabled rather than appearing clickable with nothing behind them (CLAUDE.md: never
  // simulate a successful operation).
  for (auto* b : {validateButton_, runSolverButton_, compareButton_, exportButton_}) b->setEnabled(false);

  grid->addWidget(validateButton_, 0, 0);
  grid->addWidget(runSolverButton_, 0, 1);
  grid->addWidget(compareButton_, 0, 2);
  grid->addWidget(exportButton_, 0, 3);
}
