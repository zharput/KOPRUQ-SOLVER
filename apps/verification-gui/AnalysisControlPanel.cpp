#include "AnalysisControlPanel.h"
#include <QStyle>
#include <QVBoxLayout>

namespace {
QPushButton* makeStepButton(const QString& text, QWidget* parent) {
  auto* b = new QPushButton(text, parent);
  b->setMinimumHeight(56);
  b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  b->setEnabled(false); // nothing is wired until MainWindow's state machine says otherwise
  return b;
}
}

AnalysisControlPanel::AnalysisControlPanel(QWidget* parent) : QWidget(parent) {
  setObjectName("stepSidebar");
  setFixedWidth(200);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 16, 12, 16);
  layout->setSpacing(10);

  validateButton_ = makeStepButton(tr("1. Modeli Doğrula"), this);
  runSolverButton_ = makeStepButton(tr("2. Solver'ı Çalıştır"), this);
  compareButton_ = makeStepButton(tr("3. Sonuçları Karşılaştır"), this);
  exportButton_ = makeStepButton(tr("4. Raporu Dışa Aktar"), this);

  for (auto* b : {validateButton_, runSolverButton_, compareButton_, exportButton_}) layout->addWidget(b);
  layout->addStretch(1);
}

void AnalysisControlPanel::setPrimaryButton(QPushButton* button) {
  for (auto* b : {validateButton_, runSolverButton_, compareButton_, exportButton_}) {
    const bool isPrimary = (b == button);
    if (b->property("primary").toBool() == isPrimary) continue; // avoid needless re-polish
    b->setProperty("primary", isPrimary);
    b->style()->unpolish(b);
    b->style()->polish(b);
  }
}
