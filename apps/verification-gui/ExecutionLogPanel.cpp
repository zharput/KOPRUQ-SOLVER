#include "ExecutionLogPanel.h"
#include <QVBoxLayout>

ExecutionLogPanel::ExecutionLogPanel(QWidget* parent) : QGroupBox(tr("3. Çalışma Durumu"), parent) {
  auto* layout = new QVBoxLayout(this);

  progressBar_ = new QProgressBar(this);
  progressBar_->setRange(0, 100);
  progressBar_->setValue(0);

  log_ = new QPlainTextEdit(this);
  log_->setReadOnly(true);
  log_->setPlaceholderText(tr("Henüz bir işlem çalıştırılmadı."));

  layout->addWidget(progressBar_);
  layout->addWidget(log_, 1);
}
