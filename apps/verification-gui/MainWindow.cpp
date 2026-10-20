#include "MainWindow.h"
#include "AnalysisControlPanel.h"
#include "ExecutionLogPanel.h"
#include "FileInputPanel.h"
#include "ResultsComparisonPanel.h"
#include "SolverRunner.h"
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSplitter>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(tr("KOPRUQ-SOLVER | MIDAS Verification Tool"));
  setMinimumSize(1100, 720);

  fileInputPanel_ = new FileInputPanel(this);

  auto* rightColumn = new QWidget(this);
  auto* rightLayout = new QVBoxLayout(rightColumn);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  analysisControlPanel_ = new AnalysisControlPanel(rightColumn);
  executionLogPanel_ = new ExecutionLogPanel(rightColumn);
  rightLayout->addWidget(analysisControlPanel_);
  rightLayout->addWidget(executionLogPanel_, 1);

  auto* topSplitter = new QSplitter(Qt::Horizontal, this);
  topSplitter->addWidget(fileInputPanel_);
  topSplitter->addWidget(rightColumn);
  topSplitter->setStretchFactor(0, 3);
  topSplitter->setStretchFactor(1, 2);

  resultsPanel_ = new ResultsComparisonPanel(this);

  auto* mainSplitter = new QSplitter(Qt::Vertical, this);
  mainSplitter->addWidget(topSplitter);
  mainSplitter->addWidget(resultsPanel_);
  mainSplitter->setStretchFactor(0, 1);
  mainSplitter->setStretchFactor(1, 2);

  setCentralWidget(mainSplitter);

  solverRunner_ = new SolverRunner(this);
  connect(solverRunner_, &SolverRunner::logLine, this, &MainWindow::appendLog);
  connect(solverRunner_, &SolverRunner::finished, this, &MainWindow::onRunFinished);

  connect(fileInputPanel_, &FileInputPanel::selectionChanged, this, &MainWindow::updateActionAvailability);
  connect(analysisControlPanel_->validateButton(), &QPushButton::clicked, this, [this] { startRun(RunMode::ValidateOnly); });
  connect(analysisControlPanel_->runSolverButton(), &QPushButton::clicked, this, [this] { startRun(RunMode::Solve); });

  updateActionAvailability();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (solverRunner_) solverRunner_->cancel();
  event->accept();
}

void MainWindow::updateActionAvailability() {
  const bool ready = fileInputPanel_->hasRequiredModel() && !solverRunner_->isRunning();
  analysisControlPanel_->validateButton()->setEnabled(ready);
  analysisControlPanel_->runSolverButton()->setEnabled(ready);
  // Compare/Export stay disabled until GUI-5/GUI-6 give them something real to act on.
}

void MainWindow::startRun(RunMode mode) {
  if (!fileInputPanel_->hasRequiredModel()) {
    appendLog(tr("MIDAS NX Model (JSON) seçilmeden çalıştırılamaz."));
    return;
  }
  const QString cli = SolverRunner::locateCli();
  if (cli.isEmpty()) {
    appendLog(tr("kopruq_solver_cli bulunamadı (GUI yanında veya PATH'te değil)."));
    return;
  }

  const QString modelPath = fileInputPanel_->filePath(FileInputPanel::Row::MidasModel);
  const QString workDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/kopruq_verification";
  QDir().mkpath(workDir);
  const QString outputPath = workDir + "/" + QFileInfo(modelPath).completeBaseName() + ".results.json";

  activeRunMode_ = mode;
  analysisControlPanel_->validateButton()->setEnabled(false);
  analysisControlPanel_->runSolverButton()->setEnabled(false);
  executionLogPanel_->progressBar()->setRange(0, 0); // indeterminate -- the CLI exposes no granular progress

  const QString label = (mode == RunMode::ValidateOnly) ? tr("Modeli Doğrula") : tr("Solver'ı Çalıştır");
  solverRunner_->run(cli, modelPath, "midas-nx-json", outputPath, label);
}

void MainWindow::onRunFinished(int exitCode, QProcess::ExitStatus status, const QString& outputPath) {
  const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
  executionLogPanel_->progressBar()->setRange(0, 100);

  if (status == QProcess::CrashExit) {
    executionLogPanel_->progressBar()->setValue(0);
    appendLog(tr("[%1] İşlem beklenmedik şekilde sonlandı.").arg(ts));
  } else if (activeRunMode_ == RunMode::ValidateOnly) {
    // The CLI has no separate "validate only" mode: exit code 3 means the model itself was
    // rejected before any solve was attempted (invalid), anything else (0 or 1) means the model
    // was read and accepted. This button reports only that distinction -- never claims a solve
    // succeeded, since that is "Solver'ı Çalıştır"'s claim to make, not this one's.
    if (exitCode == 3) {
      executionLogPanel_->progressBar()->setValue(0);
      appendLog(tr("[%1] Model GEÇERSİZ -- ayrıntılar için günlüğe bakın.").arg(ts));
    } else {
      executionLogPanel_->progressBar()->setValue(100);
      appendLog(tr("[%1] Model geçerli (okunabildi ve kabul edildi).").arg(ts));
    }
  } else {
    if (exitCode == 0) {
      lastResultsPath_ = outputPath;
      executionLogPanel_->progressBar()->setValue(100);
      appendLog(tr("[%1] Analiz tamamlandı. Sonuçlar: %2").arg(ts, outputPath));
    } else if (exitCode == 1 && QFileInfo::exists(outputPath)) {
      // Per the CLI's documented contract, exit 1 still writes results (a load case that failed
      // to solve, not a crash) -- the file exists and is recorded, but this is not success.
      lastResultsPath_ = outputPath;
      executionLogPanel_->progressBar()->setValue(0);
      appendLog(tr("[%1] Analiz BAŞARISIZ (sonuçlar yine de yazıldı): %2").arg(ts, outputPath));
    } else {
      executionLogPanel_->progressBar()->setValue(0);
      appendLog(tr("[%1] Analiz BAŞARISIZ (çıkış kodu %2).").arg(ts).arg(exitCode));
    }
  }
  updateActionAvailability();
}

void MainWindow::appendLog(const QString& line) {
  executionLogPanel_->log()->appendPlainText(line);
}
