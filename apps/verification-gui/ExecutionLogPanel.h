#pragma once
// GUI-1: pure layout -- progress bar + log panel (area 3). Populating it from a real QProcess
// run is GUI-3. V2.0: the log distinguishes INFO/SUCCESS/WARNING/ERROR lines by color, per the
// revised spec -- a plain QPlainTextEdit can't color individual lines, so this uses QTextEdit
// (still read-only, still just a log, never user-editable).
#include <QGroupBox>
#include <QProgressBar>
#include <QTextEdit>

class ExecutionLogPanel : public QGroupBox {
  Q_OBJECT
public:
  enum class Level { Info, Success, Warning, Error };

  explicit ExecutionLogPanel(QWidget* parent = nullptr);

  QProgressBar* progressBar() const { return progressBar_; }
  QTextEdit* log() const { return log_; }

  // Appends one line in the given level's color; always scrolls to keep the latest line visible.
  void appendLine(const QString& text, Level level = Level::Info);

private:
  QProgressBar* progressBar_{};
  QTextEdit* log_{};
};
