#pragma once
// V2.0 revision. Sequential 4-step workflow (WorkflowState) replaces the old independent-flags
// enabling logic: Modeli Doğrula -> Solver'ı Çalıştır -> Sonuçları Karşılaştır -> Raporu Dışa
// Aktar must now happen in order within one "session" against the current MIDAS model file.
// Changing the model file fully resets to step 1; changing an Excel file/its "Karşılaştır"
// checkbox/a tolerance value only invalidates the comparison step (model+solve stay valid).
//
// Busy sub-states (VALIDATING/SOLVING/...) from the spec are represented here by
// SolverRunner::isRunning() rather than separate enum values -- Compare/Export are synchronous
// (no QProcess involved), so there is nothing async to name a sub-state for; a disclosed
// simplification, not a missing feature.
#include "ResultMatcher.h"
#include "ModelInfoPanel.h"
#include <QMainWindow>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <vector>

class FileInputPanel;
class AnalysisControlPanel;
class ExecutionLogPanel;
class ResultsComparisonPanel;
class SolverRunner;
class ModelViewerWidget;
class QCloseEvent;
class QTableWidget;
class QComboBox;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  // Modal is never user-triggered directly: startRun(Solve) chains into it automatically, right
  // after a successful solve, only when the Eigenvalue Mode (Excel) row has a usable file -- see
  // its own comment in MainWindow.cpp for why a second CLI invocation is the simplest correct
  // approach rather than teaching the CLI's own "solve" command to also run modal analysis.
  enum class RunMode { ValidateOnly, Solve, Spectrum, Modal };
  enum class WorkflowState { InputReady = 0, Validated = 1, Solved = 2, Compared = 3 };

  void applyWorkflowState();
  void startRun(RunMode mode);
  void startModalRun();
  void startSpectrumRun();
  void onRunFinished(int exitCode, QProcess::ExitStatus status, const QString& outputPath);
  void onCompare();
  void onExport();
  void onSolverLogLine(const QString& line);
  void appendLog(const QString& line, int level = 0); // level: ExecutionLogPanel::Level, int to avoid a header dependency here
  void onModelRowChanged();
  void onReferenceRowChanged();
  void onToleranceChanged();
  void invalidateComparison();   // Compared -> Solved (comparison tables/cache cleared)
  void invalidateSolveAndBeyond(); // Solved/Compared -> Validated (+ invalidateComparison)
  void fullReset();              // anything -> InputReady (+ invalidateSolveAndBeyond, model info panel reset)
  // V2.2 Stage A: (re)loads the "Model Görünümü" 3D viewer from the current MIDAS model row via
  // the same kopruq::io::readModelFile(..., "midas-nx-json", ...) path the CLI/solver use, and
  // the same MatrixAssembler::assemble() used before any solve as its validation gate -- never a
  // separate parser. Clears the viewer first so a failed import/validation never leaves a stale
  // model displayed.
  void loadViewerModel();

  FileInputPanel* fileInputPanel_{};
  AnalysisControlPanel* analysisControlPanel_{};
  ExecutionLogPanel* executionLogPanel_{};
  ModelInfoPanel* modelInfoPanel_{};
  ResultsComparisonPanel* resultsPanel_{};
  ModelViewerWidget* modelViewer_{};
  QComboBox* loadCaseCombo_{}; // Stage D: "Yük Durumu" selector for Model Görünümü's load arrows
  SolverRunner* solverRunner_{};

  WorkflowState state_{WorkflowState::InputReady};
  RunMode activeRunMode_{RunMode::Solve};
  QString lastResultsPath_; // discovered results.json from the most recent successful solve
  QString lastModalResultsPath_; // discovered kopruq-modal-results.json, empty if no Eigenvalue Mode row or that run failed
  QString lastSpectrumResultsPath_; // response-spectrum results of the chained `spectrum` run, empty if the model has none or it failed
  bool modelHasSpectrum_{false};    // the loaded MIDAS model requests response-spectrum analysis (SPLC), set by loadViewerModel()
  ModelInfoPanel::Info modelInfo_; // accumulated across the two "midas-nx-json ..." log lines

  // GUI-5's last comparison, cached so GUI-6's export reuses it verbatim instead of recomputing.
  std::vector<kopruq::verification::ComparisonRow> lastDisplacementRows_, lastReactionRows_, lastLocalReactionRows_, lastBeamForceRows_, lastTrussForceRows_, lastElasticLinkRows_, lastEigenvalueRows_;
  QStringList lastDiagnostics_;
  kopruq::verification::ToleranceSettings lastTolerance_;
  QString lastSolverVersion_;
  QString lastComparisonTimestamp_;
  QString lastDisplacementsFile_, lastReactionsFile_, lastLocalReactionsFile_, lastBeamForceFile_, lastTrussForceFile_, lastElasticLinkFile_, lastEigenvalueFile_;
};
