#include "MainWindow.h"
#include "AnalysisControlPanel.h"
#include "ExecutionLogPanel.h"
#include "FileInputPanel.h"
#include "ResultsComparisonPanel.h"
#include <QSplitter>
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
}
