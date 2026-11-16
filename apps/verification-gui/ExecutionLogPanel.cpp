#include "ExecutionLogPanel.h"
#include "IconProvider.h"
#include "PanelHeader.h"

#include <QScrollBar>
#include <QTextDocument>
#include <QUrl>
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
// The level's own color doubles as its status icon's tint -- success/warning/error/info icons
// are the same shapes used elsewhere (circle-check, alert-triangle, x-circle, info-circle),
// just recolored here to match each log level.
const char* iconNameFor(ExecutionLogPanel::Level level) {
  switch (level) {
    case ExecutionLogPanel::Level::Success: return "circle-check";
    case ExecutionLogPanel::Level::Warning: return "alert-triangle";
    case ExecutionLogPanel::Level::Error: return "x-circle";
    case ExecutionLogPanel::Level::Info: return "info-circle";
  }
  return "info-circle";
}
// Qt's rich-text engine (QTextEdit::insertHtml) reads '<', '>', '&' as markup -- escape them so
// a log line that happens to contain e.g. a file path with '&' never breaks or mangles the line.
QString escapeHtml(const QString& s) {
  QString out = s;
  out.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
  return out;
}
}

ExecutionLogPanel::ExecutionLogPanel(QWidget* parent) : QGroupBox(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->addWidget(makePanelHeader("activity", tr("3. Çalışma Durumu"), this));

  progressBar_ = new QProgressBar(this);
  progressBar_->setRange(0, 100);
  progressBar_->setValue(0);

  log_ = new QTextEdit(this);
  log_->setReadOnly(true);
  log_->setPlaceholderText(tr("Henüz bir işlem çalıştırılmadı."));

  // Registering each level's tinted icon once as a named document resource (rather than relying
  // on Qt's SVG image-format plugin, whose static/plugin registration is not guaranteed) lets
  // appendLine() reference it via a plain <img src="icon:..."> tag below.
  constexpr int kLogIconSize = 14;
  for (auto level : {Level::Info, Level::Success, Level::Warning, Level::Error}) {
    const QString url = QStringLiteral("icon:%1").arg(static_cast<int>(level));
    log_->document()->addResource(QTextDocument::ImageResource, QUrl(url),
                                   icons::tintedPixmap(iconNameFor(level), QColor(colorFor(level)), kLogIconSize));
  }

  layout->addWidget(progressBar_);
  layout->addWidget(log_, 1);
}

void ExecutionLogPanel::appendLine(const QString& text, Level level) {
  log_->append(QStringLiteral("<img src=\"icon:%1\"> <span style=\"color:%2;\">%3</span>")
                   .arg(static_cast<int>(level))
                   .arg(colorFor(level), escapeHtml(text)));
  log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
}
