#include "ExecutionLogPanel.h"
#include <QScrollBar>
#include <QVBoxLayout>

namespace {
QString colorFor(ExecutionLogPanel::Level level) {
  switch (level) {
    case ExecutionLogPanel::Level::Success: return "#22C55E";
    case ExecutionLogPanel::Level::Warning: return "#F59E0B";
    case ExecutionLogPanel::Level::Error: return "#EF4444";
    case ExecutionLogPanel::Level::Info: return "#E5EAF2";
  }
  return "#E5EAF2";
}
// Qt's rich-text engine (QTextEdit::insertHtml) reads '<', '>', '&' as markup -- escape them so
// a log line that happens to contain e.g. a file path with '&' never breaks or mangles the line.
QString escapeHtml(const QString& s) {
  QString out = s;
  out.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
  return out;
}
}

ExecutionLogPanel::ExecutionLogPanel(QWidget* parent) : QGroupBox(tr("3. Çalışma Durumu"), parent) {
  auto* layout = new QVBoxLayout(this);

  progressBar_ = new QProgressBar(this);
  progressBar_->setRange(0, 100);
  progressBar_->setValue(0);

  log_ = new QTextEdit(this);
  log_->setReadOnly(true);
  log_->setPlaceholderText(tr("Henüz bir işlem çalıştırılmadı."));

  layout->addWidget(progressBar_);
  layout->addWidget(log_, 1);
}

void ExecutionLogPanel::appendLine(const QString& text, Level level) {
  log_->append(QStringLiteral("<span style=\"color:%1;\">%2</span>").arg(colorFor(level), escapeHtml(text)));
  log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
}
