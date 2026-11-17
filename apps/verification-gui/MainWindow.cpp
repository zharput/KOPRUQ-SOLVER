#include "MainWindow.h"
#include "AnalysisControlPanel.h"
#include "ExecutionLogPanel.h"
#include "FileInputPanel.h"
#include "ResultsComparisonPanel.h"
#include "ModelViewerWidget.h"
#include "SolverRunner.h"
#include "EigenvalueReferenceReader.h"
#include "ExcelReferenceReader.h"
#include "ResultMatcher.h"
#include "VerificationReportExporter.h"
#include "../../include/kopruq/io/Cli.hpp"
#include "../../include/kopruq/assembly/MatrixAssembler.hpp"
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <fstream>
#include <sstream>

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
    for (int col = 0; col < table->columnCount(); ++col)
      if (auto* item = table->item(i, col)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
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
    for (int col = 0; col < table->columnCount(); ++col)
      if (auto* item = table->item(i, col)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  }
}

// Eigenvalue (modal) comparison: same per-row fields as populateNodeBasedTable, minus "Yük
// Durumu" (modal analysis has no load case) -- `ComparisonRow::id` holds the 1-based mode number.
void populateEigenvalueTable(QTableWidget* table, const std::vector<ComparisonRow>& rows) {
  table->setRowCount(static_cast<int>(rows.size()));
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    const auto& r = rows[static_cast<std::size_t>(i)];
    int c = 0;
    const bool hasNumbers = r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail;
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.id)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.component)));
    table->setItem(i, c++, new QTableWidgetItem(fmt(r.result.referenceValue)));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.solverValue) : QString()));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.signedDiff) : QString()));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.absDiff) : QString()));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? QString::number(r.result.relDiff * 100.0, 'f', 3) + "%" : QString()));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.unit)));
    table->setItem(i, c++, new QTableWidgetItem(statusText(r.result.status)));
    for (int col = 0; col < table->columnCount(); ++col)
      if (auto* item = table->item(i, col)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  }
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(tr("KOPRUQ-SOLVER | MIDAS Verification Tool"));
  setMinimumSize(1200, 760);

  // V2.0 layout: a fixed-width step sidebar on the far left; everything else in a right column
  // split top (Girdi Dosyaları + Çalışma Durumu) / bottom (Model Bilgileri + Sonuç tabloları).
  analysisControlPanel_ = new AnalysisControlPanel(this);

  fileInputPanel_ = new FileInputPanel(this);
  executionLogPanel_ = new ExecutionLogPanel(this);
  auto* topSplitter = new QSplitter(Qt::Horizontal, this);
  topSplitter->addWidget(fileInputPanel_);
  topSplitter->addWidget(executionLogPanel_);
  topSplitter->setStretchFactor(0, 3);
  topSplitter->setStretchFactor(1, 3); // Çalışma Durumu is at least as wide as Girdi Dosyaları

  modelInfoPanel_ = new ModelInfoPanel(this);
  resultsPanel_ = new ResultsComparisonPanel(this);

  // V2.2 Stage A: "Model Görünümü" sits beside the existing results-tables group as a sibling
  // tab, switchable, rather than replacing/nesting inside ResultsComparisonPanel's own internal
  // tab widget (which stays exactly as it was).
  modelViewer_ = new ModelViewerWidget(this);
  auto* viewerToolBar = new QToolBar(tr("Görünüm Araçları"), this);
  viewerToolBar->addAction(tr("Modele Sığdır"), modelViewer_, &ModelViewerWidget::fitToModel);
  viewerToolBar->addAction(tr("Görünümü Sıfırla"), modelViewer_, &ModelViewerWidget::resetView);
  viewerToolBar->addSeparator();
  viewerToolBar->addAction(tr("İzometrik"), modelViewer_, &ModelViewerWidget::viewIsometric);
  viewerToolBar->addAction(tr("Üstten"), modelViewer_, &ModelViewerWidget::viewTop);
  viewerToolBar->addAction(tr("Önden"), modelViewer_, &ModelViewerWidget::viewFront);
  viewerToolBar->addAction(tr("Yandan"), modelViewer_, &ModelViewerWidget::viewSide);
  viewerToolBar->addSeparator();
  viewerToolBar->addAction(tr("Yakınlaştır"), modelViewer_, &ModelViewerWidget::zoomIn);
  viewerToolBar->addAction(tr("Uzaklaştır"), modelViewer_, &ModelViewerWidget::zoomOut);
  viewerToolBar->addSeparator();
  // V2.2 Stage B: ID labels default OFF (performance requirement for dense models).
  auto* nodeIdAction = viewerToolBar->addAction(tr("Düğüm ID"));
  nodeIdAction->setCheckable(true);
  connect(nodeIdAction, &QAction::toggled, modelViewer_, &ModelViewerWidget::setShowNodeIds);
  auto* elementIdAction = viewerToolBar->addAction(tr("Eleman ID"));
  elementIdAction->setCheckable(true);
  connect(elementIdAction, &QAction::toggled, modelViewer_, &ModelViewerWidget::setShowElementIds);
  viewerToolBar->addSeparator();
  // V2.2 Stage C: Supports/Elastic Links default ON (cheap, structurally important); Local Axes
  // default OFF (visual clutter), matching ModelViewerWidget's own defaults.
  auto* supportsAction = viewerToolBar->addAction(tr("Mesnetler"));
  supportsAction->setCheckable(true);
  supportsAction->setChecked(true);
  connect(supportsAction, &QAction::toggled, modelViewer_, &ModelViewerWidget::setShowSupports);
  auto* elasticLinksAction = viewerToolBar->addAction(tr("Elastik Bağlantılar"));
  elasticLinksAction->setCheckable(true);
  elasticLinksAction->setChecked(true);
  connect(elasticLinksAction, &QAction::toggled, modelViewer_, &ModelViewerWidget::setShowElasticLinks);
  auto* localAxesAction = viewerToolBar->addAction(tr("Yerel Eksenler"));
  localAxesAction->setCheckable(true);
  connect(localAxesAction, &QAction::toggled, modelViewer_, &ModelViewerWidget::setShowLocalAxes);

  // V2.2 Stage B: Element/Node Information panel, filled only from ModelViewerWidget's own
  // selection signals -- its text is built from real model fields, never fabricated here.
  auto* viewerInfoPanel = new QPlainTextEdit(this);
  viewerInfoPanel->setReadOnly(true);
  viewerInfoPanel->setPlainText(tr("Seçim yok."));
  viewerInfoPanel->setMaximumWidth(260);
  connect(modelViewer_, &ModelViewerWidget::nodeInfoChanged, viewerInfoPanel, &QPlainTextEdit::setPlainText);
  connect(modelViewer_, &ModelViewerWidget::elementInfoChanged, viewerInfoPanel, &QPlainTextEdit::setPlainText);
  connect(modelViewer_, &ModelViewerWidget::selectionCleared, viewerInfoPanel, [viewerInfoPanel, this] { viewerInfoPanel->setPlainText(tr("Seçim yok.")); });

  auto* viewerPage = new QWidget(this);
  auto* viewerOuterLayout = new QHBoxLayout(viewerPage);
  viewerOuterLayout->setContentsMargins(0, 0, 0, 0);
  viewerOuterLayout->setSpacing(0);
  auto* viewerLeft = new QWidget(viewerPage);
  auto* viewerLayout = new QVBoxLayout(viewerLeft);
  viewerLayout->setContentsMargins(0, 0, 0, 0);
  viewerLayout->setSpacing(0);
  viewerLayout->addWidget(viewerToolBar);
  viewerLayout->addWidget(modelViewer_, 1);
  viewerOuterLayout->addWidget(viewerLeft, 1);
  viewerOuterLayout->addWidget(viewerInfoPanel);

  auto* resultsTabs = new QTabWidget(this);
  resultsTabs->addTab(resultsPanel_, tr("Sonuç Karşılaştırma Tabloları"));
  resultsTabs->addTab(viewerPage, tr("Model Görünümü"));

  auto* bottomSplitter = new QSplitter(Qt::Horizontal, this);
  bottomSplitter->addWidget(modelInfoPanel_);
  bottomSplitter->addWidget(resultsTabs);
  bottomSplitter->setStretchFactor(0, 1);
  bottomSplitter->setStretchFactor(1, 3);

  auto* rightSplitter = new QSplitter(Qt::Vertical, this);
  rightSplitter->addWidget(topSplitter);
  rightSplitter->addWidget(bottomSplitter);
  rightSplitter->setStretchFactor(0, 1);
  rightSplitter->setStretchFactor(1, 2);

  auto* central = new QWidget(this);
  auto* centralLayout = new QHBoxLayout(central);
  centralLayout->setContentsMargins(0, 0, 0, 0);
  centralLayout->setSpacing(0);
  centralLayout->addWidget(analysisControlPanel_);
  centralLayout->addWidget(rightSplitter, 1);
  setCentralWidget(central);

  solverRunner_ = new SolverRunner(this);
  connect(solverRunner_, &SolverRunner::logLine, this, &MainWindow::onSolverLogLine);
  connect(solverRunner_, &SolverRunner::finished, this, &MainWindow::onRunFinished);
  connect(solverRunner_, &SolverRunner::started, this, &MainWindow::applyWorkflowState);

  connect(fileInputPanel_, &FileInputPanel::rowChanged, this, [this](FileInputPanel::Row row) {
    if (row == FileInputPanel::Row::MidasModel) onModelRowChanged();
    else onReferenceRowChanged();
  });
  connect(analysisControlPanel_->validateButton(), &QToolButton::clicked, this, [this] { startRun(RunMode::ValidateOnly); });
  connect(analysisControlPanel_->runSolverButton(), &QToolButton::clicked, this, [this] { startRun(RunMode::Solve); });
  connect(analysisControlPanel_->compareButton(), &QToolButton::clicked, this, &MainWindow::onCompare);
  connect(analysisControlPanel_->exportButton(), &QToolButton::clicked, this, &MainWindow::onExport);
  connect(resultsPanel_->absoluteToleranceField(), &QLineEdit::textChanged, this, [this](const QString&) { onToleranceChanged(); });
  connect(resultsPanel_->relativeToleranceField(), &QLineEdit::textChanged, this, [this](const QString&) { onToleranceChanged(); });

  applyWorkflowState();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (solverRunner_) solverRunner_->cancel();
  event->accept();
}

void MainWindow::applyWorkflowState() {
  const bool busy = solverRunner_->isRunning();
  const bool hasModel = fileInputPanel_->hasRequiredModel();

  analysisControlPanel_->validateButton()->setEnabled(!busy && hasModel);
  analysisControlPanel_->runSolverButton()->setEnabled(!busy && hasModel && state_ >= WorkflowState::Validated);
  analysisControlPanel_->compareButton()->setEnabled(!busy && state_ >= WorkflowState::Solved);
  analysisControlPanel_->exportButton()->setEnabled(!busy && state_ >= WorkflowState::Compared);

  QToolButton* primary = nullptr;
  if (!busy) {
    switch (state_) {
      case WorkflowState::InputReady: if (hasModel) primary = analysisControlPanel_->validateButton(); break;
      case WorkflowState::Validated: primary = analysisControlPanel_->runSolverButton(); break;
      case WorkflowState::Solved: primary = analysisControlPanel_->compareButton(); break;
      case WorkflowState::Compared: primary = analysisControlPanel_->exportButton(); break;
    }
  }
  analysisControlPanel_->setPrimaryButton(primary);
}

void MainWindow::invalidateComparison() {
  lastDisplacementRows_.clear();
  lastReactionRows_.clear();
  lastBeamForceRows_.clear();
  lastElasticLinkRows_.clear();
  lastEigenvalueRows_.clear();
  lastDiagnostics_.clear();
  resultsPanel_->displacementsTable()->setRowCount(0);
  resultsPanel_->reactionsTable()->setRowCount(0);
  resultsPanel_->beamForcesTable()->setRowCount(0);
  resultsPanel_->elasticLinksTable()->setRowCount(0);
  resultsPanel_->eigenvalueTable()->setRowCount(0);
  resultsPanel_->diagnosticsTable()->setRowCount(0);
  resultsPanel_->categoryTable()->setRowCount(0);
  resultsPanel_->setSummaryCounts(0, 0, 0, 0);
  if (state_ == WorkflowState::Compared) state_ = WorkflowState::Solved;
  applyWorkflowState();
}

void MainWindow::invalidateSolveAndBeyond() {
  invalidateComparison();
  lastResultsPath_.clear();
  lastModalResultsPath_.clear();
  if (state_ == WorkflowState::Solved) state_ = WorkflowState::Validated;
  applyWorkflowState();
}

void MainWindow::fullReset() {
  invalidateSolveAndBeyond();
  state_ = WorkflowState::InputReady;
  modelInfo_ = ModelInfoPanel::Info{};
  modelInfoPanel_->reset();
  modelViewer_->clearModel();
  applyWorkflowState();
}

void MainWindow::loadViewerModel() {
  modelViewer_->clearModel(); // never leave a stale model displayed while (re)loading
  if (!fileInputPanel_->isUsable(FileInputPanel::Row::MidasModel)) return;
  const std::string path = fileInputPanel_->filePath(FileInputPanel::Row::MidasModel).toStdString();
  std::string text;
  kopruq::io::ModelInput model;
  int exitCode = 0;
  std::ostringstream err;
  if (!kopruq::io::detail::readModelFile(path, "midas-nx-json", err, text, model, exitCode)) {
    appendLog(tr("Model Görünümü: dosya okunamadı (%1).").arg(QString::fromStdString(err.str())), /*Warning*/ 2);
    return;
  }
  try {
    kopruq::MatrixAssembler::assemble(model.model); // same validation gate as a solve
  } catch (const std::exception& e) {
    appendLog(tr("Model Görünümü: model geçerli değil (%1).").arg(QString::fromStdString(e.what())), /*Warning*/ 2);
    return;
  }
  modelViewer_->setModel(model);
}

void MainWindow::onModelRowChanged() {
  // "Eski model sonuçları yeni modelle asla karşılaştırılmamalı" -- any prior validate/solve/
  // compare result belonged to whatever file was selected before, so a full reset is the only
  // safe response to the MIDAS model row itself changing.
  fullReset();
  loadViewerModel();
  appendLog(tr("Model dosyası değişti, durum sıfırlandı."), /*Info*/ 0);
}

void MainWindow::onReferenceRowChanged() {
  // An Excel row's path or its "Karşılaştır" checkbox changed -- the model and any solve results
  // are still perfectly valid, only a previously-run comparison is now potentially stale.
  invalidateComparison();
}

void MainWindow::onToleranceChanged() {
  invalidateComparison();
}

void MainWindow::startRun(RunMode mode) {
  if (!fileInputPanel_->hasRequiredModel()) {
    appendLog(tr("MIDAS NX Model (JSON) seçilmeden çalıştırılamaz."), /*Error*/ 3);
    return;
  }
  if (mode == RunMode::Solve && state_ < WorkflowState::Validated) {
    appendLog(tr("Önce Modeli Doğrula ile model geçerliliği onaylanmalı."), /*Warning*/ 2);
    return;
  }
  const QString cli = SolverRunner::locateCli();
  if (cli.isEmpty()) {
    appendLog(tr("kopruq_solver_cli bulunamadı (GUI yanında veya PATH'te değil)."), /*Error*/ 3);
    return;
  }

  const QString modelPath = fileInputPanel_->filePath(FileInputPanel::Row::MidasModel);
  const QString workDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/kopruq_verification";
  QDir().mkpath(workDir);
  const QString outputPath = workDir + "/" + QFileInfo(modelPath).completeBaseName() + ".results.json";

  activeRunMode_ = mode;
  executionLogPanel_->progressBar()->setRange(0, 0); // indeterminate -- the CLI exposes no granular progress
  applyWorkflowState(); // disables everything while busy

  const QString label = (mode == RunMode::ValidateOnly) ? tr("Modeli Doğrula") : tr("Solver'ı Çalıştır");
  solverRunner_->run(cli, modelPath, "midas-nx-json", outputPath, label);
}

// Chained automatically by onRunFinished right after a successful "solve", only when the
// Eigenvalue Mode (Excel) row has a usable file -- a second CLI invocation (`modal`, not
// `solve`) rather than teaching "solve" to also run modal analysis, since the two are
// independent analyses in the CLI's own design (Phase 6.1's ModalSolver vs Phase 4's
// LinearStaticSolver) and every other model/workflow must stay unaffected by this row existing.
void MainWindow::startModalRun() {
  const QString cli = SolverRunner::locateCli();
  const QString modelPath = fileInputPanel_->filePath(FileInputPanel::Row::MidasModel);
  const QString workDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/kopruq_verification";
  const QString outputPath = workDir + "/" + QFileInfo(modelPath).completeBaseName() + ".modal-results.json";

  activeRunMode_ = RunMode::Modal;
  QStringList extra;
  const bool lumped = fileInputPanel_->useLumpedMass();
  if (lumped) extra << "--mass-type" << "lumped";
  appendLog(lumped ? tr("Kütle tipi: Lumped (MIDAS NX varsayılanı)") : tr("Kütle tipi: Consistent (solver varsayılanı)"), /*Info*/ 0);
  solverRunner_->run(cli, modelPath, "midas-nx-json", outputPath, tr("Eigenvalue (Modal) Analizi"), "modal", extra);
}

void MainWindow::onRunFinished(int exitCode, QProcess::ExitStatus status, const QString& outputPath) {
  const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
  executionLogPanel_->progressBar()->setRange(0, 100);

  if (status == QProcess::CrashExit) {
    executionLogPanel_->progressBar()->setValue(0);
    appendLog(tr("[%1] İşlem beklenmedik şekilde sonlandı.").arg(ts), /*Error*/ 3);
  } else if (activeRunMode_ == RunMode::ValidateOnly) {
    // The CLI has no separate "validate only" mode: exit code 3 means the model itself was
    // rejected before any solve was attempted (invalid), anything else (0 or 1) means the model
    // was read and accepted. This button reports only that distinction -- never claims a solve
    // succeeded, since that is "Solver'ı Çalıştır"'s claim to make, not this one's.
    if (exitCode == 3) {
      executionLogPanel_->progressBar()->setValue(0);
      appendLog(tr("[%1] Model GEÇERSİZ -- ayrıntılar için günlüğe bakın.").arg(ts), /*Error*/ 3);
      modelInfoPanel_->setStatus(tr("Model geçersiz."), /*isError=*/true);
      state_ = WorkflowState::InputReady;
    } else {
      executionLogPanel_->progressBar()->setValue(100);
      appendLog(tr("[%1] Model geçerli (okunabildi ve kabul edildi).").arg(ts), /*Success*/ 1);
      modelInfo_.modelName = QFileInfo(fileInputPanel_->filePath(FileInputPanel::Row::MidasModel)).completeBaseName();
      modelInfo_.hasModelName = true;
      modelInfo_.unitSystem = tr("kN - m (KOPRUQ-SOLVER dahili)");
      modelInfo_.hasUnitSystem = true;
      modelInfoPanel_->setData(modelInfo_);
      modelInfoPanel_->setStatus(tr("Model geçerli. Analize hazır."), /*isError=*/false);
      appendLog(tr("[%1] MIDAS yük kombinasyonları desteklenmiyor (Yük Kombinasyonu Sayısı: N/A).").arg(ts), /*Warning*/ 2);
      state_ = WorkflowState::Validated;
    }
  } else if (activeRunMode_ == RunMode::Solve) {
    if (exitCode == 0) {
      lastResultsPath_ = outputPath;
      executionLogPanel_->progressBar()->setValue(100);
      appendLog(tr("[%1] Analiz tamamlandı. Sonuçlar: %2").arg(ts, outputPath), /*Success*/ 1);
      // Eigenvalue Mode (Excel) row provided -> chain a second CLI invocation (`modal`) before
      // declaring the step Solved, so a comparison against it is possible right after. Static
      // solve already succeeded regardless of what this second run does, so its own outcome is
      // handled entirely in the RunMode::Modal branch below (never re-fails this step).
      if (fileInputPanel_->isUsable(FileInputPanel::Row::EigenvalueMode)) {
        appendLog(tr("[%1] Eigenvalue Mode (Excel) seçili -- modal analiz de çalıştırılıyor...").arg(ts), /*Info*/ 0);
        startModalRun();
        return; // stays "busy" (buttons disabled, progress indeterminate) until the modal run finishes
      }
      lastModalResultsPath_.clear();
      state_ = WorkflowState::Solved;
    } else if (exitCode == 1 && QFileInfo::exists(outputPath)) {
      // Per the CLI's documented contract, exit 1 still writes results (a load case that failed
      // to solve, not a crash) -- the file exists and is recorded, but this is not success.
      lastResultsPath_ = outputPath;
      executionLogPanel_->progressBar()->setValue(0);
      appendLog(tr("[%1] Analiz BAŞARISIZ (sonuçlar yine de yazıldı): %2").arg(ts, outputPath), /*Error*/ 3);
      state_ = WorkflowState::Validated; // "Solver başarısızsa üçüncü aşamaya geçilmemeli"
    } else {
      executionLogPanel_->progressBar()->setValue(0);
      appendLog(tr("[%1] Analiz BAŞARISIZ (çıkış kodu %2).").arg(ts).arg(exitCode), /*Error*/ 3);
      state_ = WorkflowState::Validated;
    }
  } else { // RunMode::Modal -- always finalizes to Solved, since the static solve it was chained
           // after already succeeded; a failed/missing modal run only means the Eigenvalue
           // comparison category will itself report N/A later, not that the whole step failed.
    if (exitCode == 0) {
      lastModalResultsPath_ = outputPath;
      executionLogPanel_->progressBar()->setValue(100);
      appendLog(tr("[%1] Modal analiz tamamlandı. Sonuçlar: %2").arg(ts, outputPath), /*Success*/ 1);
    } else {
      lastModalResultsPath_.clear();
      executionLogPanel_->progressBar()->setValue(0);
      appendLog(tr("[%1] Modal analiz BAŞARISIZ (çıkış kodu %2) -- Eigenvalue karşılaştırması N/A olacak.").arg(ts).arg(exitCode), /*Error*/ 3);
    }
    state_ = WorkflowState::Solved;
  }
  applyWorkflowState();
}

void MainWindow::onSolverLogLine(const QString& line) {
  static const QRegularExpression importRe(
      "midas-nx-json import: (\\d+) nodes, (\\d+) beams, (\\d+) materials, (\\d+) sections, "
      "(\\d+) supports, (\\d+) rigid links, (\\d+) load cases, (\\d+) nodal loads, "
      "(\\d+) member loads, (\\d+) thermal loads, (\\d+) elastic links rejected");
  static const QRegularExpression boundsRe(
      R"(midas-nx-json bounds: X\[([-\d.eE]+),([-\d.eE]+)\] Y\[([-\d.eE]+),([-\d.eE]+)\] Z\[([-\d.eE]+),([-\d.eE]+)\])");

  if (const auto m = importRe.match(line); m.hasMatch()) {
    modelInfo_.nodeCount = m.captured(1).toLong(); modelInfo_.hasNodeCount = true;
    modelInfo_.elementCount = m.captured(2).toLong(); modelInfo_.hasElementCount = true;
    modelInfo_.materialCount = m.captured(3).toLong(); modelInfo_.hasMaterialCount = true;
    modelInfo_.sectionCount = m.captured(4).toLong(); modelInfo_.hasSectionCount = true;
    modelInfo_.loadCaseCount = m.captured(7).toLong(); modelInfo_.hasLoadCaseCount = true;
    modelInfo_.elasticLinkCount = m.captured(11).toLong(); modelInfo_.hasElasticLinkCount = true;
    // combinationCount is deliberately never set here -- MIDAS load combinations are not parsed
    // by the importer at all, so ModelInfoPanel keeps showing "N/A" for it, not a guess.
    modelInfoPanel_->setData(modelInfo_);
  } else if (const auto b = boundsRe.match(line); b.hasMatch()) {
    const double minX = b.captured(1).toDouble(), maxX = b.captured(2).toDouble();
    const double minY = b.captured(3).toDouble(), maxY = b.captured(4).toDouble();
    const double minZ = b.captured(5).toDouble(), maxZ = b.captured(6).toDouble();
    modelInfo_.extentX = maxX - minX;
    modelInfo_.extentY = maxY - minY;
    modelInfo_.extentZ = maxZ - minZ;
    modelInfo_.hasExtents = true;
    modelInfoPanel_->setData(modelInfo_);
  }

  int level = 0; // Info
  if (line.contains(QStringLiteral("[rejected]")) || line.contains(QStringLiteral("[assumed]"))) level = 2; // Warning
  else if (line.contains(QStringLiteral("BAŞARISIZ")) || line.contains(QStringLiteral("GEÇERSİZ")) || line.contains(QStringLiteral("hata"), Qt::CaseInsensitive)) level = 3; // Error
  appendLog(line, level);
}

void MainWindow::appendLog(const QString& line, int level) {
  executionLogPanel_->appendLine(line, static_cast<ExecutionLogPanel::Level>(level));
}

void MainWindow::onCompare() {
  if (state_ < WorkflowState::Solved || lastResultsPath_.isEmpty()) {
    appendLog(tr("Önce Solver'ı Çalıştır ile bir sonuç üretilmeli."), /*Warning*/ 2);
    return;
  }

  const bool anyChecked = fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::Displacements) ||
                           fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::Reactions) ||
                           fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::BeamForce) ||
                           fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::ElasticLink) ||
                           fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::EigenvalueMode);
  if (!anyChecked) {
    appendLog(tr("En az bir Excel referans dosyası seçilip \"Karşılaştır\" işaretlenmeli."), /*Warning*/ 2);
    return;
  }

  nlohmann::json results;
  {
    std::ifstream in(lastResultsPath_.toStdString(), std::ios::binary);
    if (!in) {
      appendLog(tr("Sonuç dosyası açılamadı: %1").arg(lastResultsPath_), /*Error*/ 3);
      return;
    }
    try {
      in >> results;
    } catch (const std::exception& e) {
      appendLog(tr("Sonuç dosyası (results.json) ayrıştırılamadı: %1").arg(QString::fromStdString(e.what())), /*Error*/ 3);
      return;
    }
  }

  ToleranceSettings tol;
  bool ok = false;
  if (const double v = resultsPanel_->absoluteToleranceField()->text().toDouble(&ok); ok && v > 0) tol.absoluteToleranceScale = v;
  if (const double v = resultsPanel_->relativeToleranceField()->text().toDouble(&ok); ok && v >= 0) tol.relativeTol = v;

  struct FileSpec { FileInputPanel::Row row; ReferenceFileKind kind; const char* label; QString* fileNameOut; };
  const FileSpec specs[] = {
      {FileInputPanel::Row::Displacements, ReferenceFileKind::Displacements, "Displacements(Global)", &lastDisplacementsFile_},
      {FileInputPanel::Row::Reactions, ReferenceFileKind::Reaction, "Reaction(Global)", &lastReactionsFile_},
      {FileInputPanel::Row::BeamForce, ReferenceFileKind::BeamForce, "Beam Force", &lastBeamForceFile_},
      {FileInputPanel::Row::ElasticLink, ReferenceFileKind::ElasticLink, "Elastic Link", &lastElasticLinkFile_},
  };

  std::vector<ComparisonRow> displacementRows, reactionRows, beamForceRows, elasticLinkRows;
  QStringList allDiagnostics;

  for (const auto& spec : specs) {
    *spec.fileNameOut = QString();
    if (!fileInputPanel_->isUsable(spec.row)) {
      allDiagnostics << tr("%1: dosya seçilmedi, bu kategori karşılaştırılmadı.").arg(spec.label);
      continue;
    }
    if (!fileInputPanel_->isIncludedInComparison(spec.row)) {
      allDiagnostics << tr("%1: dosya seçili ama \"Karşılaştır\" işaretli değil, bu kategori atlandı.").arg(spec.label);
      continue;
    }
    *spec.fileNameOut = QFileInfo(fileInputPanel_->filePath(spec.row)).fileName();
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

  // Eigenvalue Mode (Excel): a separate code path, not folded into the FileSpec loop above --
  // it compares against the chained `modal` CLI run's own results.json (lastModalResultsPath_),
  // never the static-solve `results` already parsed above, and reads via a dedicated parser
  // (EigenvalueReferenceReader, not ExcelReferenceReader/ReferenceTable) since the real export's
  // layout (stacked, independently-titled sub-tables) does not fit that generic reader.
  std::vector<ComparisonRow> eigenvalueRows;
  lastEigenvalueFile_.clear();
  if (!fileInputPanel_->isUsable(FileInputPanel::Row::EigenvalueMode)) {
    allDiagnostics << tr("Eigenvalue Mode: dosya seçilmedi, bu kategori karşılaştırılmadı.");
  } else if (!fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::EigenvalueMode)) {
    allDiagnostics << tr("Eigenvalue Mode: dosya seçili ama \"Karşılaştır\" işaretli değil, bu kategori atlandı.");
  } else {
    lastEigenvalueFile_ = QFileInfo(fileInputPanel_->filePath(FileInputPanel::Row::EigenvalueMode)).fileName();
    auto eigenRead = kopruq::verification::readEigenvalueReferenceExcel(fileInputPanel_->filePath(FileInputPanel::Row::EigenvalueMode).toStdString());
    for (const auto& d : eigenRead.diagnostics) allDiagnostics << QString::fromStdString(d);
    if (eigenRead.rows) {
      if (lastModalResultsPath_.isEmpty()) {
        allDiagnostics << tr("Eigenvalue Mode: modal analiz çalıştırılmadı veya başarısız oldu, karşılaştırma yapılamadı.");
      } else {
        nlohmann::json modalResults;
        std::ifstream modalIn(lastModalResultsPath_.toStdString(), std::ios::binary);
        if (!modalIn) {
          allDiagnostics << tr("Modal sonuç dosyası açılamadı: %1").arg(lastModalResultsPath_);
        } else {
          try {
            modalIn >> modalResults;
            CategoryComparison cmp = kopruq::verification::matchEigenvalueFrequencies(modalResults, *eigenRead.rows, tol);
            for (const auto& d : cmp.diagnostics) allDiagnostics << QString::fromStdString(d);
            eigenvalueRows = std::move(cmp.rows);
          } catch (const std::exception& e) {
            allDiagnostics << tr("Modal sonuç dosyası (modal-results.json) ayrıştırılamadı: %1").arg(QString::fromStdString(e.what()));
          }
        }
      }
    }
  }

  populateNodeBasedTable(resultsPanel_->displacementsTable(), displacementRows);
  populateNodeBasedTable(resultsPanel_->reactionsTable(), reactionRows);
  populateBeamOrLinkTable(resultsPanel_->beamForcesTable(), beamForceRows);
  populateBeamOrLinkTable(resultsPanel_->elasticLinksTable(), elasticLinkRows);
  populateEigenvalueTable(resultsPanel_->eigenvalueTable(), eigenvalueRows);

  auto* diag = resultsPanel_->diagnosticsTable();
  diag->setRowCount(static_cast<int>(allDiagnostics.size()));
  for (int i = 0; i < static_cast<int>(allDiagnostics.size()); ++i) {
    diag->setItem(i, 0, new QTableWidgetItem(tr("Karşılaştırma")));
    diag->setItem(i, 1, new QTableWidgetItem(tr("GUI-5")));
    diag->setItem(i, 2, new QTableWidgetItem(allDiagnostics[i]));
    diag->setItem(i, 3, new QTableWidgetItem(tr("Bilgi")));
    for (int col = 0; col < diag->columnCount(); ++col)
      if (auto* item = diag->item(i, col)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  }

  const struct { const char* name; const std::vector<ComparisonRow>* rows; } categories[] = {
      {"Deplasmanlar (Global)", &displacementRows},
      {"Mesnet Tepkimeleri (Global)", &reactionRows},
      {"Kiriş İç Kuvvetleri", &beamForceRows},
      {"Elastik Bağlantılar", &elasticLinkRows},
      {"Özdeğer (Eigenvalue)", &eigenvalueRows},
  };
  auto* catTable = resultsPanel_->categoryTable();
  constexpr int kCategoryCount = 5;
  catTable->setRowCount(kCategoryCount);
  int totalAll = 0, passAll = 0, failAll = 0, naAll = 0;
  for (int i = 0; i < kCategoryCount; ++i) {
    const auto stats = kopruq::verification::summarizeCategory(*categories[i].rows);
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
    for (int col = 0; col < catTable->columnCount(); ++col)
      if (auto* item = catTable->item(i, col)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  }
  resultsPanel_->setSummaryCounts(totalAll, passAll, failAll, naAll);

  // Cache exactly what was just shown, verbatim, for GUI-6's export -- never recomputed there.
  lastDisplacementRows_ = std::move(displacementRows);
  lastReactionRows_ = std::move(reactionRows);
  lastBeamForceRows_ = std::move(beamForceRows);
  lastElasticLinkRows_ = std::move(elasticLinkRows);
  lastEigenvalueRows_ = std::move(eigenvalueRows);
  lastDiagnostics_ = allDiagnostics;
  lastTolerance_ = tol;
  lastSolverVersion_ = results.value("metadata", nlohmann::json::object()).value("solver_version", std::string{}).c_str();
  lastComparisonTimestamp_ = QDateTime::currentDateTime().toString(Qt::ISODate);
  state_ = WorkflowState::Compared;
  applyWorkflowState();

  appendLog(tr("[%1] Karşılaştırma tamamlandı. Toplam %2, PASS %3, FAIL %4, N/A %5.")
                .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                .arg(totalAll).arg(passAll).arg(failAll).arg(naAll),
            failAll > 0 ? /*Warning*/ 2 : /*Success*/ 1);
}

void MainWindow::onExport() {
  if (state_ < WorkflowState::Compared) {
    appendLog(tr("Önce Sonuçları Karşılaştır ile bir karşılaştırma üretilmeli."), /*Warning*/ 2);
    return;
  }
  const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
                               "/KOPRUQ_SOLVER_Verification_Report.xlsx";
  const QString chosen = QFileDialog::getSaveFileName(this, tr("Raporu Dışa Aktar"), defaultPath, tr("Excel dosyası (*.xlsx)"));
  if (chosen.isEmpty()) return;

  kopruq::verification::ReportData reportData;
  reportData.displacements = lastDisplacementRows_;
  reportData.reactions = lastReactionRows_;
  reportData.beamForces = lastBeamForceRows_;
  reportData.elasticLinks = lastElasticLinkRows_;
  for (const auto& d : lastDiagnostics_) reportData.diagnostics.push_back(d.toStdString());
  reportData.metadata.modelFileName = QFileInfo(fileInputPanel_->filePath(FileInputPanel::Row::MidasModel)).fileName().toStdString();
  reportData.metadata.solverVersion = lastSolverVersion_.toStdString();
  reportData.metadata.executionDateTimeIso = lastComparisonTimestamp_.toStdString();
  reportData.metadata.displacementsFileName = lastDisplacementsFile_.toStdString();
  reportData.metadata.reactionsFileName = lastReactionsFile_.toStdString();
  reportData.metadata.beamForceFileName = lastBeamForceFile_.toStdString();
  reportData.metadata.elasticLinkFileName = lastElasticLinkFile_.toStdString();
  reportData.metadata.absoluteToleranceScale = lastTolerance_.absoluteToleranceScale;
  reportData.metadata.relativeTol = lastTolerance_.relativeTol;

  std::string error;
  if (kopruq::verification::exportVerificationReport(chosen.toStdString(), reportData, error)) {
    appendLog(tr("[%1] Rapor dışa aktarıldı: %2")
                  .arg(QDateTime::currentDateTime().toString("HH:mm:ss"), chosen),
              /*Success*/ 1);
  } else {
    appendLog(tr("Rapor dışa aktarılamadı: %1").arg(QString::fromStdString(error)), /*Error*/ 3);
  }
}
