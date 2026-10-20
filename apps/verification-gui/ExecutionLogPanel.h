#pragma once
// GUI-1: pure layout -- progress bar + log panel (area 3). Populating it from a real QProcess
// run is GUI-3.
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QProgressBar>

class ExecutionLogPanel : public QGroupBox {
  Q_OBJECT
public:
  explicit ExecutionLogPanel(QWidget* parent = nullptr);

  QProgressBar* progressBar() const { return progressBar_; }
  QPlainTextEdit* log() const { return log_; }

private:
  QProgressBar* progressBar_{};
  QPlainTextEdit* log_{};
};
