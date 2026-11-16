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
class QCloseEvent;
class QTableWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);

protected:
  void closeEvent(QCloseEvent* event) override;

private:
  enum class RunMode { ValidateOnly, Solve };
  enum class WorkflowState { InputReady = 0, Validated = 1, Solved = 2, Compared = 3 };

  void applyWorkflowState();
  void startRun(RunMode mode);
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

  FileInputPanel* fileInputPanel_{};
  AnalysisControlPanel* analysisControlPanel_{};
  ExecutionLogPanel* executionLogPanel_{};
  ModelInfoPanel* modelInfoPanel_{};
  ResultsComparisonPanel* resultsPanel_{};
  SolverRunner* solverRunner_{};

  WorkflowState state_{WorkflowState::InputReady};
  RunMode activeRunMode_{RunMode::Solve};
  QString lastResultsPath_; // discovered results.json from the most recent successful solve
  ModelInfoPanel::Info modelInfo_; // accumulated across the two "midas-nx-json ..." log lines

  // GUI-5's last comparison, cached so GUI-6's export reuses it verbatim instead of recomputing.
  std::vector<kopruq::verification::ComparisonRow> lastDisplacementRows_, lastReactionRows_, lastBeamForceRows_, lastElasticLinkRows_;
  QStringList lastDiagnostics_;
  kopruq::verification::ToleranceSettings lastTolerance_;
  QString lastSolverVersion_;
  QString lastComparisonTimestamp_;
  QString lastDisplacementsFile_, lastReactionsFile_, lastBeamForceFile_, lastElasticLinkFile_;
};
