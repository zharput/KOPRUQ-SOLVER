#pragma once
// GUI-1 assembled the four approved layout areas. GUI-2 gave FileInputPanel real file-selection
// behavior. GUI-3 wires the two solver-related actions to the real CLI via SolverRunner. GUI-4
// added ExcelReferenceReader. GUI-5 wires "Sonuçları Karşılaştır" to the real
// ResultMatcher/ComparisonEngine and populates the result tables -- Excel report export (GUI-6)
// still does not exist.
#include <QMainWindow>
#include <QProcess>
#include <QString>

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
  void appendLog(const QString& line);

  FileInputPanel* fileInputPanel_{};
  AnalysisControlPanel* analysisControlPanel_{};
  ExecutionLogPanel* executionLogPanel_{};
  ResultsComparisonPanel* resultsPanel_{};
  SolverRunner* solverRunner_{};

  RunMode activeRunMode_{RunMode::Solve};
  QString lastResultsPath_; // discovered results.json from the most recent successful solve
};
