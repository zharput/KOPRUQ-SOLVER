#include "MainWindow.h"
#include "AnalysisControlPanel.h"
#include "ExecutionLogPanel.h"
#include "FileInputPanel.h"
#include "ResultsComparisonPanel.h"
#include "SolverRunner.h"
#include "ExcelReferenceReader.h"
#include "ResultMatcher.h"
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <fstream>

using kopruq::verification::CategoryComparison;
using kopruq::verification::ComparisonRow;
using kopruq::verification::ComparisonStatus;
using kopruq::verification::ReferenceFileKind;
using kopruq::verification::ToleranceSettings;

namespace {

QString statusText(ComparisonStatus s) { return QString::fromUtf8(kopruq::verification::comparisonStatusName(s)); }

QString fmt(double v) { return QString::number(v, 'g', 10); }

// Shared by Displacements and Reactions: identical 10-column layout (loadCase, id, component,
// reference, solver, signed/abs/rel diff, unit, status) -- see ResultsComparisonPanel.cpp.
void populateNodeBasedTable(QTableWidget* table, const std::vector<ComparisonRow>& rows) {
  table->setRowCount(static_cast<int>(rows.size()));
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    const auto& r = rows[static_cast<std::size_t>(i)];
    int c = 0;
    const bool hasNumbers = r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail;
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.loadCase)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.id)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.component)));
    table->setItem(i, c++, new QTableWidgetItem(fmt(r.result.referenceValue)));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.solverValue) : QString()));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.signedDiff) : QString()));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.absDiff) : QString()));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? QString::number(r.result.relDiff * 100.0, 'f', 3) + "%" : QString()));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.unit)));
    table->setItem(i, c++, new QTableWidgetItem(statusText(r.result.status)));
  }
}

// Beam forces and elastic links share a 9-column layout (loadCase, id, location/end, component,
// reference, solver, a single signed diff, unit, status) -- elastic links always have
// status == Unsupported, so their numeric cells stay blank rather than showing a misleading 0.
void populateBeamOrLinkTable(QTableWidget* table, const std::vector<ComparisonRow>& rows) {
  table->setRowCount(static_cast<int>(rows.size()));
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    const auto& r = rows[static_cast<std::size_t>(i)];
    int c = 0;
    const bool hasNumbers = r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail;
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.loadCase)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.id)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.location)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.component)));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.referenceValue) : QString()));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.solverValue) : QString()));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.signedDiff) : QString()));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.unit)));
    table->setItem(i, c++, new QTableWidgetItem(statusText(r.result.status)));
  }
}

struct CategoryStats { int total{}, pass{}, fail{}, na{}; double maxAbsDiff{}, maxAbsDiffPercent{}; };

CategoryStats summarize(const std::vector<ComparisonRow>& rows) {
  CategoryStats s;
  s.total = static_cast<int>(rows.size());
  for (const auto& r : rows) {
    switch (r.result.status) {
      case ComparisonStatus::Pass: ++s.pass; break;
      case ComparisonStatus::Fail: ++s.fail; break;
      default: ++s.na; break; // NotApplicable and Unsupported both roll into the single N/A tile
    }
    if (r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail) {
      s.maxAbsDiff = std::max(s.maxAbsDiff, r.result.absDiff);
      s.maxAbsDiffPercent = std::max(s.maxAbsDiffPercent, r.result.relDiff * 100.0);
    }
  }
  return s;
}

} // namespace

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
  connect(analysisControlPanel_->compareButton(), &QPushButton::clicked, this, &MainWindow::onCompare);

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
  // Compare needs an actual results.json from a prior solve; it does not require every Excel
  // reference file (each is individually optional, per the approved spec) -- onCompare() skips
  // and diagnoses whichever ones are missing rather than refusing to run at all.
  analysisControlPanel_->compareButton()->setEnabled(!lastResultsPath_.isEmpty() && !solverRunner_->isRunning());
  // Export stays disabled until GUI-6 gives it something real to act on.
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

void MainWindow::onCompare() {
  if (lastResultsPath_.isEmpty()) {
    appendLog(tr("Önce Solver'ı Çalıştır ile bir sonuç üretilmeli."));
    return;
  }

  nlohmann::json results;
  {
    std::ifstream in(lastResultsPath_.toStdString(), std::ios::binary);
    if (!in) {
      appendLog(tr("Sonuç dosyası açılamadı: %1").arg(lastResultsPath_));
      return;
    }
    try {
      in >> results;
    } catch (const std::exception& e) {
      appendLog(tr("Sonuç dosyası (results.json) ayrıştırılamadı: %1").arg(QString::fromStdString(e.what())));
      return;
    }
  }

  ToleranceSettings tol;
  bool ok = false;
  if (const double v = resultsPanel_->absoluteToleranceField()->text().toDouble(&ok); ok && v > 0) tol.absoluteToleranceScale = v;
  if (const double v = resultsPanel_->relativeToleranceField()->text().toDouble(&ok); ok && v >= 0) tol.relativeTol = v;

  struct FileSpec { FileInputPanel::Row row; ReferenceFileKind kind; const char* label; };
  const FileSpec specs[] = {
      {FileInputPanel::Row::Displacements, ReferenceFileKind::Displacements, "Displacements(Global)"},
      {FileInputPanel::Row::Reactions, ReferenceFileKind::Reaction, "Reaction(Global)"},
      {FileInputPanel::Row::BeamForce, ReferenceFileKind::BeamForce, "Beam Force"},
      {FileInputPanel::Row::ElasticLink, ReferenceFileKind::ElasticLink, "Elastic Link"},
  };

  std::vector<ComparisonRow> displacementRows, reactionRows, beamForceRows, elasticLinkRows;
  QStringList allDiagnostics;

  for (const auto& spec : specs) {
    if (!fileInputPanel_->isUsable(spec.row)) {
      allDiagnostics << tr("%1: dosya seçilmedi, bu kategori karşılaştırılmadı.").arg(spec.label);
      continue;
    }
    auto read = kopruq::verification::readReferenceExcel(fileInputPanel_->filePath(spec.row).toStdString(), spec.kind);
    for (const auto& d : read.diagnostics) allDiagnostics << QString::fromStdString(d);
    if (!read.table) continue;

    CategoryComparison cmp;
    switch (spec.kind) {
      case ReferenceFileKind::Displacements: cmp = kopruq::verification::matchDisplacements(results, *read.table, tol); break;
      case ReferenceFileKind::Reaction: cmp = kopruq::verification::matchReactions(results, *read.table, tol); break;
      case ReferenceFileKind::BeamForce: cmp = kopruq::verification::matchBeamForces(results, *read.table, tol); break;
      case ReferenceFileKind::ElasticLink: cmp = kopruq::verification::markElasticLinksUnsupported(*read.table); break;
    }
    for (const auto& d : cmp.diagnostics) allDiagnostics << QString::fromStdString(d);

    switch (spec.kind) {
      case ReferenceFileKind::Displacements: displacementRows = std::move(cmp.rows); break;
      case ReferenceFileKind::Reaction: reactionRows = std::move(cmp.rows); break;
      case ReferenceFileKind::BeamForce: beamForceRows = std::move(cmp.rows); break;
      case ReferenceFileKind::ElasticLink: elasticLinkRows = std::move(cmp.rows); break;
    }
  }

  populateNodeBasedTable(resultsPanel_->displacementsTable(), displacementRows);
  populateNodeBasedTable(resultsPanel_->reactionsTable(), reactionRows);
  populateBeamOrLinkTable(resultsPanel_->beamForcesTable(), beamForceRows);
  populateBeamOrLinkTable(resultsPanel_->elasticLinksTable(), elasticLinkRows);

  auto* diag = resultsPanel_->diagnosticsTable();
  diag->setRowCount(static_cast<int>(allDiagnostics.size()));
  for (int i = 0; i < static_cast<int>(allDiagnostics.size()); ++i) {
    diag->setItem(i, 0, new QTableWidgetItem(tr("Karşılaştırma")));
    diag->setItem(i, 1, new QTableWidgetItem(tr("GUI-5")));
    diag->setItem(i, 2, new QTableWidgetItem(allDiagnostics[i]));
    diag->setItem(i, 3, new QTableWidgetItem(tr("Bilgi")));
  }

  const struct { const char* name; const std::vector<ComparisonRow>* rows; } categories[] = {
      {"Deplasmanlar (Global)", &displacementRows},
      {"Mesnet Tepkimeleri (Global)", &reactionRows},
      {"Kiriş İç Kuvvetleri", &beamForceRows},
      {"Elastik Bağlantılar", &elasticLinkRows},
  };
  auto* catTable = resultsPanel_->categoryTable();
  catTable->setRowCount(4);
  int totalAll = 0, passAll = 0, failAll = 0, naAll = 0;
  for (int i = 0; i < 4; ++i) {
    const auto stats = summarize(*categories[i].rows);
    totalAll += stats.total; passAll += stats.pass; failAll += stats.fail; naAll += stats.na;
    const QString status = stats.total == 0 ? tr("—") : (stats.fail == 0 ? tr("PASS") : tr("FAIL"));
    int c = 0;
    catTable->setItem(i, c++, new QTableWidgetItem(QString::fromUtf8(categories[i].name)));
    catTable->setItem(i, c++, new QTableWidgetItem(QString::number(stats.total)));
    catTable->setItem(i, c++, new QTableWidgetItem(QString::number(stats.pass)));
    catTable->setItem(i, c++, new QTableWidgetItem(QString::number(stats.fail)));
    catTable->setItem(i, c++, new QTableWidgetItem(QString::number(stats.na)));
    catTable->setItem(i, c++, new QTableWidgetItem(fmt(stats.maxAbsDiff)));
    catTable->setItem(i, c++, new QTableWidgetItem(QString::number(stats.maxAbsDiffPercent, 'f', 3) + "%"));
    catTable->setItem(i, c++, new QTableWidgetItem(status));
    catTable->setItem(i, c++, new QTableWidgetItem(tr("İlgili sekmeye bakın")));
  }
  resultsPanel_->setSummaryCounts(totalAll, passAll, failAll, naAll);

  appendLog(tr("[%1] Karşılaştırma tamamlandı. Toplam %2, PASS %3, FAIL %4, N/A %5.")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                .arg(totalAll).arg(passAll).arg(failAll).arg(naAll));
}
