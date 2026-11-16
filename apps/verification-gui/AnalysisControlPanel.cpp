#include "AnalysisControlPanel.h"
#include "IconProvider.h"

#include <QStyle>
#include <QVBoxLayout>

namespace {
constexpr int kIconSize = 24;

QToolButton* makeStepButton(const QString& text, const QString& iconName, QWidget* parent) {
  auto* b = new QToolButton(parent);
  b->setText(text);
  b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  b->setIconSize(QSize(kIconSize, kIconSize));
  b->setIcon(icons::tintedIcon(iconName, QColor("#FFFFFF"), kIconSize));
  b->setMinimumHeight(64);
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

  validateButton_ = makeStepButton(tr("1. Modeli Doğrula"), "circle-check", this);
  runSolverButton_ = makeStepButton(tr("2. Solver'ı Çalıştır"), "play", this);
  compareButton_ = makeStepButton(tr("3. Sonuçları Karşılaştır"), "arrow-left-right", this);
  exportButton_ = makeStepButton(tr("4. Raporu Dışa Aktar"), "file-text", this);

  for (auto* b : {validateButton_, runSolverButton_, compareButton_, exportButton_}) layout->addWidget(b);
  layout->addStretch(1);
}

void AnalysisControlPanel::setPrimaryButton(QToolButton* button) {
  for (auto* b : {validateButton_, runSolverButton_, compareButton_, exportButton_}) {
    const bool isPrimary = (b == button);
    if (b->property("primary").toBool() == isPrimary) continue; // avoid needless re-polish
    b->setProperty("primary", isPrimary);
    b->style()->unpolish(b);
    b->style()->polish(b);
  }
}
