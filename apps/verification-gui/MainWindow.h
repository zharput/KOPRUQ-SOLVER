#pragma once
// GUI-1: assembles the four approved layout areas into the main window. No business logic --
// see FileInputPanel/AnalysisControlPanel/ExecutionLogPanel/ResultsComparisonPanel.
#include <QMainWindow>

class FileInputPanel;
class AnalysisControlPanel;
class ExecutionLogPanel;
class ResultsComparisonPanel;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget* parent = nullptr);

private:
  FileInputPanel* fileInputPanel_{};
  AnalysisControlPanel* analysisControlPanel_{};
  ExecutionLogPanel* executionLogPanel_{};
  ResultsComparisonPanel* resultsPanel_{};
};
