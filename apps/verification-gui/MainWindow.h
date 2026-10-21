#pragma once
// GUI-1 assembled the four approved layout areas. GUI-2 gave FileInputPanel real file-selection
// behavior. GUI-3 wires the two solver-related actions to the real CLI via SolverRunner. GUI-4
// added ExcelReferenceReader. GUI-5 wires "Sonuçları Karşılaştır" to the real
// ResultMatcher/ComparisonEngine and populates the result tables. GUI-6 wires "Raporu Dışa
// Aktar" to VerificationReportExporter, reusing the SAME comparison results GUI-5 already
// computed (cached as members here) rather than recomputing anything.
#include "ResultMatcher.h"
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

  void updateActionAvailability();
  void startRun(RunMode mode);
  void onRunFinished(int exitCode, QProcess::ExitStatus status, const QString& outputPath);
  void onCompare();
  void onExport();
  void appendLog(const QString& line);

  FileInputPanel* fileInputPanel_{};
  AnalysisControlPanel* analysisControlPanel_{};
  ExecutionLogPanel* executionLogPanel_{};
  ResultsComparisonPanel* resultsPanel_{};
  SolverRunner* solverRunner_{};

  RunMode activeRunMode_{RunMode::Solve};
  QString lastResultsPath_; // discovered results.json from the most recent successful solve

  // GUI-5's last comparison, cached so GUI-6's export reuses it verbatim instead of recomputing.
  bool hasComparisonResults_{false};
  std::vector<kopruq::verification::ComparisonRow> lastDisplacementRows_, lastReactionRows_, lastBeamForceRows_, lastElasticLinkRows_;
  QStringList lastDiagnostics_;
  kopruq::verification::ToleranceSettings lastTolerance_;
  QString lastSolverVersion_;
  QString lastComparisonTimestamp_;
  QString lastDisplacementsFile_, lastReactionsFile_, lastBeamForceFile_, lastElasticLinkFile_;
};
