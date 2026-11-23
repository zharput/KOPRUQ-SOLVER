#include "MainWindow.h"
#include "AnalysisControlPanel.h"
#include "ExecutionLogPanel.h"
#include "FileInputPanel.h"
#include "ResultsComparisonPanel.h"
#include "ModelViewerWidget.h"
#include "kopruq/io/Utf8Path.hpp"
#include "SolverRunner.h"
#include "EigenvalueReferenceReader.h"
#include "ExcelReferenceReader.h"
#include "ResultMatcher.h"
#include "VerificationReportExporter.h"
#include "../../include/kopruq/io/Cli.hpp"
#include "../../include/kopruq/assembly/MatrixAssembler.hpp"
#include <QCloseEvent>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyle>
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

// User-requested (2026-11-18): PASS rows green, FAIL rows red, N/A/Unsupported rows orange, in
// every comparison table and the category summary table -- set explicitly on every cell rather
// than relying on any inherited/stylesheet color, so every compared value stays visible
// regardless of row state.
void colorizeRow(QTableWidget* table, int row, ComparisonStatus status) {
  QColor color;
  switch (status) {
    case ComparisonStatus::Pass: color = QColor(0x22, 0xC5, 0x5E); break;
    case ComparisonStatus::Fail: color = QColor(0xEF, 0x44, 0x44); break;
    case ComparisonStatus::NotApplicable:
    case ComparisonStatus::Unsupported: color = QColor(0xF5, 0x9E, 0x0B); break;
    default: return;
  }
  for (int col = 0; col < table->columnCount(); ++col)
    if (auto* item = table->item(row, col)) item->setForeground(color);
}

// Shared by Displacements and Reactions: identical 10-column layout (loadCase, id, component,
// reference, solver, signed/abs/rel diff, unit, status) -- see ResultsComparisonPanel.cpp.
// Cells that have no number (N/A/Unsupported rows: no finite solver value, so no diff to show)
// render as "—" rather than an empty string (2026-11-18, user-requested: no comparison cell
// should ever render visually blank).
// Sorting must stay off while rows are being filled: QTableWidget re-sorts after every setItem
// call once sorting is enabled, so a row can move mid-fill and scatter its own cells across other
// rows (2026-11-18, user-reported: tables showed only one fully-populated row, everything else
// blank except the sorted column).
void populateNodeBasedTable(QTableWidget* table, const std::vector<ComparisonRow>& rows) {
  table->setSortingEnabled(false);
  table->setRowCount(static_cast<int>(rows.size()));
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    const auto& r = rows[static_cast<std::size_t>(i)];
    int c = 0;
    const bool hasNumbers = r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail;
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.loadCase)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.id)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.component)));
    table->setItem(i, c++, new QTableWidgetItem(fmt(r.result.referenceValue)));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.solverValue) : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.signedDiff) : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.absDiff) : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? QString::number(r.result.relDiff * 100.0, 'f', 3) + "%" : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.unit)));
    table->setItem(i, c++, new QTableWidgetItem(statusText(r.result.status)));
    for (int col = 0; col < table->columnCount(); ++col)
      if (auto* item = table->item(i, col)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    colorizeRow(table, i, r.result.status);
  }
  table->setSortingEnabled(true);
}

// Beam forces and elastic links share a 9-column layout (loadCase, id, location/end, component,
// reference, solver, a single signed diff, unit, status) -- elastic links always have
// status == Unsupported, so their numeric cells stay blank rather than showing a misleading 0.
void populateBeamOrLinkTable(QTableWidget* table, const std::vector<ComparisonRow>& rows) {
  table->setSortingEnabled(false);
  table->setRowCount(static_cast<int>(rows.size()));
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    const auto& r = rows[static_cast<std::size_t>(i)];
    int c = 0;
    const bool hasNumbers = r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail;
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.loadCase)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.id)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.location)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.component)));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.referenceValue) : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.solverValue) : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.signedDiff) : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.unit)));
    table->setItem(i, c++, new QTableWidgetItem(statusText(r.result.status)));
    for (int col = 0; col < table->columnCount(); ++col)
      if (auto* item = table->item(i, col)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    colorizeRow(table, i, r.result.status);
  }
  table->setSortingEnabled(true);
}

// Eigenvalue (modal) comparison: same per-row fields as populateNodeBasedTable, minus "Yük
// Durumu" (modal analysis has no load case) -- `ComparisonRow::id` holds the 1-based mode number.
void populateEigenvalueTable(QTableWidget* table, const std::vector<ComparisonRow>& rows) {
  table->setSortingEnabled(false);
  table->setRowCount(static_cast<int>(rows.size()));
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    const auto& r = rows[static_cast<std::size_t>(i)];
    int c = 0;
    const bool hasNumbers = r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail;
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.id)));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.component)));
    table->setItem(i, c++, new QTableWidgetItem(fmt(r.result.referenceValue)));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.solverValue) : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.signedDiff) : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? fmt(r.result.absDiff) : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(hasNumbers ? QString::number(r.result.relDiff * 100.0, 'f', 3) + "%" : QStringLiteral("—")));
    table->setItem(i, c++, new QTableWidgetItem(QString::fromStdString(r.unit)));
    table->setItem(i, c++, new QTableWidgetItem(statusText(r.result.status)));
    for (int col = 0; col < table->columnCount(); ++col)
      if (auto* item = table->item(i, col)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    colorizeRow(table, i, r.result.status);
  }
  table->setSortingEnabled(true);
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
  // V2.2 layout revision (2026-11-17, user feedback): icon-based toolbar (Qt's built-in
  // QStyle::standardIcon pixmaps, tooltips carry the Turkish label -- no new SVG assets drawn
  // for this pass), a right-hand "Görünüm Seçenekleri" checkbox panel for the display toggles
  // (previously toolbar toggle buttons), and a full-width bottom selection-info bar, matching
  // the approved reference layout's structure. No new rendering features (no color-by-section,
  // thickness slider, load arrows, or deformed shape) were added -- those were explicitly out of
  // scope for this pass per the user's own choice.
  modelViewer_ = new ModelViewerWidget(this);
  auto* style = this->style();
  auto* viewerToolBar = new QToolBar(tr("Görünüm Araçları"), this);
  auto addIconAction = [&](const QString& tip, QStyle::StandardPixmap icon, auto&& slot) {
    QAction* a = viewerToolBar->addAction(style->standardIcon(icon), tip);
    a->setToolTip(tip);
    connect(a, &QAction::triggered, modelViewer_, slot);
    return a;
  };
  addIconAction(tr("Modele Sığdır"), QStyle::SP_TitleBarMaxButton, &ModelViewerWidget::fitToModel);
  addIconAction(tr("Görünümü Sıfırla"), QStyle::SP_BrowserReload, &ModelViewerWidget::resetView);
  viewerToolBar->addSeparator();
  addIconAction(tr("İzometrik"), QStyle::SP_ComputerIcon, &ModelViewerWidget::viewIsometric);
  addIconAction(tr("Üstten"), QStyle::SP_ArrowUp, &ModelViewerWidget::viewTop);
  addIconAction(tr("Önden"), QStyle::SP_ArrowRight, &ModelViewerWidget::viewFront);
  addIconAction(tr("Yandan"), QStyle::SP_ArrowForward, &ModelViewerWidget::viewSide);
  viewerToolBar->addSeparator();
  addIconAction(tr("Yakınlaştır"), QStyle::SP_ArrowUp, &ModelViewerWidget::zoomIn);
  addIconAction(tr("Uzaklaştır"), QStyle::SP_ArrowDown, &ModelViewerWidget::zoomOut);
  viewerToolBar->addSeparator();
  // V2.2 Stage D: load-case selector for the "Yükler" display toggle below -- populated only from
  // the currently loaded model's own kopruq::io::ModelInput::load_cases (loadViewerModel()),
  // never a fabricated/default list. Index 0 is always "Yok" (no case selected, no arrows drawn).
  viewerToolBar->addWidget(new QLabel(tr("Yük Durumu:"), this));
  loadCaseCombo_ = new QComboBox(this);
  loadCaseCombo_->addItem(tr("Yok"));
  loadCaseCombo_->setEnabled(false);
  viewerToolBar->addWidget(loadCaseCombo_);
  connect(loadCaseCombo_, &QComboBox::currentIndexChanged, modelViewer_, [this](int index) {
    modelViewer_->setActiveLoadCase(index - 1); // -1 for the leading "Yok" entry
  });

  // Display toggles, moved out of the toolbar into a dedicated right-hand panel.
  auto* optionsPanel = new QGroupBox(tr("Görünüm Seçenekleri"), this);
  auto* optionsLayout = new QVBoxLayout(optionsPanel);
  auto addToggle = [&](const QString& label, bool defaultOn, auto&& slot) {
    auto* box = new QCheckBox(label, optionsPanel);
    box->setChecked(defaultOn);
    connect(box, &QCheckBox::toggled, modelViewer_, slot);
    optionsLayout->addWidget(box);
    return box;
  };
  // V2.2 Stage B: ID labels default OFF (performance requirement for dense models).
  addToggle(tr("Düğüm Numaraları"), false, &ModelViewerWidget::setShowNodeIds);
  addToggle(tr("Eleman Numaraları"), false, &ModelViewerWidget::setShowElementIds);
  // V2.2 Stage C: Supports/Elastic Links default ON (cheap, structurally important); Local Axes
  // default OFF (visual clutter) -- matching ModelViewerWidget's own defaults.
  addToggle(tr("Mesnetler"), true, &ModelViewerWidget::setShowSupports);
  addToggle(tr("Elastik Bağlantılar"), true, &ModelViewerWidget::setShowElasticLinks);
  addToggle(tr("Lokal Eksenler"), false, &ModelViewerWidget::setShowLocalAxes);
  addToggle(tr("Kesit Görünümü"), true, &ModelViewerWidget::setShowSections);
  // V2.2 Stage D: default OFF -- only meaningful once a load case is picked from the toolbar.
  addToggle(tr("Yükler"), false, &ModelViewerWidget::setShowLoads);
  optionsLayout->addStretch(1);
  optionsPanel->setMaximumWidth(220);

  // V2.2 Stage B: full-width selection-info bar, filled only from ModelViewerWidget's own
  // selection signals -- its text is built from real model fields, never fabricated here.
  // Single-line strip (not a multi-line text block) per the approved reference layout.
  auto* viewerInfoBar = new QLabel(tr("Seçili Eleman/Düğüm Bilgisi: Seçim yok."), this);
  viewerInfoBar->setTextInteractionFlags(Qt::TextSelectableByMouse);
  viewerInfoBar->setContentsMargins(8, 4, 8, 4);
  connect(modelViewer_, &ModelViewerWidget::nodeInfoChanged, viewerInfoBar, &QLabel::setText);
  connect(modelViewer_, &ModelViewerWidget::elementInfoChanged, viewerInfoBar, &QLabel::setText);
  connect(modelViewer_, &ModelViewerWidget::selectionCleared, viewerInfoBar, [viewerInfoBar, this] { viewerInfoBar->setText(tr("Seçili Eleman/Düğüm Bilgisi: Seçim yok.")); });

  auto* viewerMiddle = new QWidget(this);
  auto* viewerMiddleLayout = new QHBoxLayout(viewerMiddle);
  viewerMiddleLayout->setContentsMargins(0, 0, 0, 0);
  viewerMiddleLayout->setSpacing(0);
  viewerMiddleLayout->addWidget(modelViewer_, 1);
  viewerMiddleLayout->addWidget(optionsPanel);

  auto* viewerPage = new QWidget(this);
  auto* viewerLayout = new QVBoxLayout(viewerPage);
  viewerLayout->setContentsMargins(0, 0, 0, 0);
  viewerLayout->setSpacing(0);
  viewerLayout->addWidget(viewerToolBar);
  viewerLayout->addWidget(viewerMiddle, 1);
  viewerLayout->addWidget(viewerInfoBar);

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
  lastLocalReactionRows_.clear();
  lastBeamForceRows_.clear();
  lastTrussForceRows_.clear();
  lastElasticLinkRows_.clear();
  lastEigenvalueRows_.clear();
  lastDiagnostics_.clear();
  resultsPanel_->displacementsTable()->setRowCount(0);
  resultsPanel_->reactionsTable()->setRowCount(0);
  resultsPanel_->localReactionsTable()->setRowCount(0);
  resultsPanel_->beamForcesTable()->setRowCount(0);
  resultsPanel_->trussForcesTable()->setRowCount(0);
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
  lastSpectrumResultsPath_.clear();
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
  loadCaseCombo_->blockSignals(true);
  loadCaseCombo_->clear();
  loadCaseCombo_->addItem(tr("Yok"));
  loadCaseCombo_->setCurrentIndex(0);
  loadCaseCombo_->setEnabled(false);
  loadCaseCombo_->blockSignals(false);
  modelHasSpectrum_ = false;
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
  modelHasSpectrum_ = !model.response_spectra.empty();
  try {
    kopruq::MatrixAssembler::assemble(model.model); // same validation gate as a solve
  } catch (const std::exception& e) {
    appendLog(tr("Model Görünümü: model geçerli değil (%1).").arg(QString::fromStdString(e.what())), /*Warning*/ 2);
    return;
  }
  modelViewer_->setModel(model);
  const QStringList cases = modelViewer_->loadCaseNames();
  if (!cases.isEmpty()) {
    loadCaseCombo_->blockSignals(true);
    loadCaseCombo_->addItems(cases);
    loadCaseCombo_->setEnabled(true);
    loadCaseCombo_->blockSignals(false);
  }
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

// Chained automatically by onRunFinished right after a successful "solve" when the model requests
// response-spectrum analysis (MIDAS SPLC): the CLI's `spectrum` command, whose results are merged
// into the comparison as MIDAS's "<case>(RS)" load cases (verification::appendSpectrumCases).
// Uses the same mass formulation as the modal run (the "Lumped Mass (MIDAS)" check box).
void MainWindow::startSpectrumRun() {
  const QString cli = SolverRunner::locateCli();
  const QString modelPath = fileInputPanel_->filePath(FileInputPanel::Row::MidasModel);
  const QString workDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/kopruq_verification";
  const QString outputPath = workDir + "/" + QFileInfo(modelPath).completeBaseName() + ".spectrum-results.json";

  activeRunMode_ = RunMode::Spectrum;
  QStringList extra;
  const bool lumped = fileInputPanel_->useLumpedMass();
  if (lumped) extra << "--mass-type" << "lumped";
  appendLog(lumped ? tr("Spektrum kütle tipi: Lumped (MIDAS NX varsayılanı)") : tr("Spektrum kütle tipi: Consistent (solver varsayılanı)"), /*Info*/ 0);
  solverRunner_->run(cli, modelPath, "midas-nx-json", outputPath, tr("Davranış Spektrumu Analizi"), "spectrum", extra);
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
      lastSpectrumResultsPath_.clear();
      lastModalResultsPath_.clear();
      if (modelHasSpectrum_) {
        appendLog(tr("[%1] Modelde davranış spektrumu tanımlı -- spektrum analizi de çalıştırılıyor...").arg(ts), /*Info*/ 0);
        startSpectrumRun();
        return; // stays busy; the Spectrum branch below chains the modal run if needed
      }
      if (fileInputPanel_->isUsable(FileInputPanel::Row::EigenvalueMode)) {
        appendLog(tr("[%1] Eigenvalue Mode (Excel) seçili -- modal analiz de çalıştırılıyor...").arg(ts), /*Info*/ 0);
        startModalRun();
        return; // stays "busy" (buttons disabled, progress indeterminate) until the modal run finishes
      }
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
  } else if (activeRunMode_ == RunMode::Spectrum) {
    // Like Modal: the static solve already succeeded; a failed spectrum run only leaves the
    // "(RS)" reference rows as N/A in the comparison.
    if (exitCode == 0) {
      lastSpectrumResultsPath_ = outputPath;
      executionLogPanel_->progressBar()->setValue(100);
      appendLog(tr("[%1] Spektrum analizi tamamlandı. Sonuçlar: %2").arg(ts, outputPath), /*Success*/ 1);
    } else {
      lastSpectrumResultsPath_.clear();
      executionLogPanel_->progressBar()->setValue(0);
      appendLog(tr("[%1] Spektrum analizi BAŞARISIZ (çıkış kodu %2) -- (RS) satırları N/A olacak.").arg(ts).arg(exitCode), /*Error*/ 3);
    }
    if (fileInputPanel_->isUsable(FileInputPanel::Row::EigenvalueMode)) {
      appendLog(tr("[%1] Eigenvalue Mode (Excel) seçili -- modal analiz de çalıştırılıyor...").arg(ts), /*Info*/ 0);
      executionLogPanel_->progressBar()->setRange(0, 0);
      startModalRun();
      return;
    }
    state_ = WorkflowState::Solved;
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
                           fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::ReactionsLocal) ||
                           fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::BeamForce) ||
                           fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::TrussForce) ||
                           fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::ElasticLink) ||
                           fileInputPanel_->isIncludedInComparison(FileInputPanel::Row::EigenvalueMode);
  if (!anyChecked) {
    appendLog(tr("En az bir Excel referans dosyası seçilip \"Karşılaştır\" işaretlenmeli."), /*Warning*/ 2);
    return;
  }

  nlohmann::json results;
  {
    std::ifstream in(kopruq::io::utf8Path(lastResultsPath_.toStdString()), std::ios::binary);
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

  // Response-spectrum results (chained `spectrum` run), merged as MIDAS's "<case>(RS)" load cases.
  if (!lastSpectrumResultsPath_.isEmpty()) {
    std::ifstream specIn(kopruq::io::utf8Path(lastSpectrumResultsPath_.toStdString()), std::ios::binary);
    try {
      nlohmann::json spectrum;
      if (!specIn) throw std::runtime_error("dosya açılamadı");
      specIn >> spectrum;
      kopruq::verification::appendSpectrumCases(results, spectrum);
      appendLog(tr("Spektrum sonuçları karşılaştırmaya eklendi (tepe değerler |KOPRUQ| ile |MIDAS| karşılaştırılır)."), /*Info*/ 0);
      // Static + spectrum combinations (MIDAS LCOM-GEN with RS terms): static + factor * |spectrum peak|.
      if (results.contains("spectrum_combinations") && !results.at("spectrum_combinations").empty()) {
        kopruq::verification::appendSpectrumCombinations(results);
        appendLog(tr("Statik + spektrum kombinasyonları karşılaştırmaya eklendi (statik + katsayı x |spektrum tepe değeri|)."), /*Info*/ 0);
      }
    } catch (const std::exception& e) {
      appendLog(tr("Spektrum sonuç dosyası okunamadı (%1): %2").arg(lastSpectrumResultsPath_, QString::fromStdString(e.what())), /*Warning*/ 2);
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
      {FileInputPanel::Row::ReactionsLocal, ReferenceFileKind::ReactionLocal, "Reaction(Local)", &lastLocalReactionsFile_},
      {FileInputPanel::Row::BeamForce, ReferenceFileKind::BeamForce, "Beam Force", &lastBeamForceFile_},
      {FileInputPanel::Row::TrussForce, ReferenceFileKind::TrussForce, "Truss Force", &lastTrussForceFile_},
      {FileInputPanel::Row::ElasticLink, ReferenceFileKind::ElasticLink, "Elastic Link", &lastElasticLinkFile_},
  };

  std::vector<ComparisonRow> displacementRows, reactionRows, localReactionRows, beamForceRows, trussForceRows, elasticLinkRows;
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
      case ReferenceFileKind::ReactionLocal: cmp = kopruq::verification::matchLocalReactions(results, *read.table, tol); break;
      case ReferenceFileKind::BeamForce: cmp = kopruq::verification::matchBeamForces(results, *read.table, tol); break;
      case ReferenceFileKind::TrussForce: cmp = kopruq::verification::matchTrussForces(results, *read.table, tol); break;
      case ReferenceFileKind::ElasticLink: cmp = kopruq::verification::matchElasticLinks(results, *read.table, tol); break;
    }
    for (const auto& d : cmp.diagnostics) allDiagnostics << QString::fromStdString(d);

    switch (spec.kind) {
      case ReferenceFileKind::Displacements: displacementRows = std::move(cmp.rows); break;
      case ReferenceFileKind::Reaction: reactionRows = std::move(cmp.rows); break;
      case ReferenceFileKind::ReactionLocal: localReactionRows = std::move(cmp.rows); break;
      case ReferenceFileKind::BeamForce: beamForceRows = std::move(cmp.rows); break;
      case ReferenceFileKind::TrussForce: trussForceRows = std::move(cmp.rows); break;
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
        std::ifstream modalIn(kopruq::io::utf8Path(lastModalResultsPath_.toStdString()), std::ios::binary);
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
  populateNodeBasedTable(resultsPanel_->localReactionsTable(), localReactionRows);
  populateBeamOrLinkTable(resultsPanel_->beamForcesTable(), beamForceRows);
  populateBeamOrLinkTable(resultsPanel_->trussForcesTable(), trussForceRows);
  populateBeamOrLinkTable(resultsPanel_->elasticLinksTable(), elasticLinkRows);
  populateEigenvalueTable(resultsPanel_->eigenvalueTable(), eigenvalueRows);

  auto* diag = resultsPanel_->diagnosticsTable();
  diag->setSortingEnabled(false);
  diag->setRowCount(static_cast<int>(allDiagnostics.size()));
  for (int i = 0; i < static_cast<int>(allDiagnostics.size()); ++i) {
    diag->setItem(i, 0, new QTableWidgetItem(tr("Karşılaştırma")));
    diag->setItem(i, 1, new QTableWidgetItem(tr("GUI-5")));
    diag->setItem(i, 2, new QTableWidgetItem(allDiagnostics[i]));
    diag->setItem(i, 3, new QTableWidgetItem(tr("Bilgi")));
    for (int col = 0; col < diag->columnCount(); ++col)
      if (auto* item = diag->item(i, col)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  }
  diag->setSortingEnabled(true);

  const struct { const char* name; const std::vector<ComparisonRow>* rows; } categories[] = {
      {"Deplasmanlar (Global)", &displacementRows},
      {"Mesnet Tepkimeleri (Global)", &reactionRows},
      {"Mesnet Tepkimeleri (Lokal)", &localReactionRows},
      {"Kiriş İç Kuvvetleri", &beamForceRows},
      {"Kafes Elemanı Kuvvetleri", &trussForceRows},
      {"Elastik Bağlantılar", &elasticLinkRows},
      {"Özdeğer (Eigenvalue)", &eigenvalueRows},
  };
  auto* catTable = resultsPanel_->categoryTable();
  catTable->setSortingEnabled(false);
  constexpr int kCategoryCount = 7;
  catTable->setRowCount(kCategoryCount);
  int totalAll = 0, passAll = 0, failAll = 0, naAll = 0;
  for (int i = 0; i < kCategoryCount; ++i) {
    const auto stats = kopruq::verification::summarizeCategory(*categories[i].rows);
    totalAll += stats.total; passAll += stats.pass; failAll += stats.fail; naAll += stats.na;
    const QString status = stats.total == 0 ? tr("—")
                            : stats.fail > 0 ? tr("FAIL")
                            : stats.na == stats.total ? tr("N/A")
                            : tr("PASS");
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
    // Category summary rows: same PASS=green/FAIL=red convention as the per-row tables.
    if (status == tr("PASS") || status == tr("FAIL") || status == tr("N/A")) {
      const QColor color = status == tr("PASS")   ? QColor(0x22, 0xC5, 0x5E)
                            : status == tr("FAIL") ? QColor(0xEF, 0x44, 0x44)
                                                    : QColor(0xF5, 0x9E, 0x0B);
      for (int col = 0; col < catTable->columnCount(); ++col)
        if (auto* item = catTable->item(i, col)) item->setForeground(color);
    }
  }
  catTable->setSortingEnabled(true);
  resultsPanel_->setSummaryCounts(totalAll, passAll, failAll, naAll);

  // Cache exactly what was just shown, verbatim, for GUI-6's export -- never recomputed there.
  lastDisplacementRows_ = std::move(displacementRows);
  lastReactionRows_ = std::move(reactionRows);
  lastLocalReactionRows_ = std::move(localReactionRows);
  lastBeamForceRows_ = std::move(beamForceRows);
  lastTrussForceRows_ = std::move(trussForceRows);
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
  reportData.localReactions = lastLocalReactionRows_;
  reportData.beamForces = lastBeamForceRows_;
  reportData.trussForces = lastTrussForceRows_;
  reportData.elasticLinks = lastElasticLinkRows_;
  for (const auto& d : lastDiagnostics_) reportData.diagnostics.push_back(d.toStdString());
  reportData.metadata.modelFileName = QFileInfo(fileInputPanel_->filePath(FileInputPanel::Row::MidasModel)).fileName().toStdString();
  reportData.metadata.solverVersion = lastSolverVersion_.toStdString();
  reportData.metadata.executionDateTimeIso = lastComparisonTimestamp_.toStdString();
  reportData.metadata.displacementsFileName = lastDisplacementsFile_.toStdString();
  reportData.metadata.reactionsFileName = lastReactionsFile_.toStdString();
  reportData.metadata.localReactionsFileName = lastLocalReactionsFile_.toStdString();
  reportData.metadata.beamForceFileName = lastBeamForceFile_.toStdString();
  reportData.metadata.trussForceFileName = lastTrussForceFile_.toStdString();
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
