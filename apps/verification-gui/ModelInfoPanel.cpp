#include "ModelInfoPanel.h"
#include "PanelHeader.h"
#include <QGridLayout>
#include <QVBoxLayout>

namespace {
QString orNA(bool has, const QString& text) { return has ? text : QObject::tr("N/A"); }
QLabel* addRow(QGridLayout* grid, int row, const QString& label, QWidget* parent) {
  grid->addWidget(new QLabel(label, parent), row, 0);
  auto* value = new QLabel(QObject::tr("N/A"), parent);
  value->setStyleSheet("font-weight: 600;");
  grid->addWidget(value, row, 1);
  return value;
}
}

ModelInfoPanel::ModelInfoPanel(QWidget* parent) : QGroupBox(parent) {
  auto* outer = new QVBoxLayout(this);
  outer->addWidget(makePanelHeader("box", tr("Model Bilgileri (JSON)"), this));
  auto* grid = new QGridLayout();
  grid->setHorizontalSpacing(12);
  grid->setVerticalSpacing(4);
  grid->setColumnStretch(1, 1);

  int r = 0;
  modelNameValue_ = addRow(grid, r++, tr("Model Adı"), this);
  unitSystemValue_ = addRow(grid, r++, tr("Birim Sistemi"), this);
  nodeCountValue_ = addRow(grid, r++, tr("Düğüm Sayısı"), this);
  elementCountValue_ = addRow(grid, r++, tr("Eleman Sayısı"), this);
  materialCountValue_ = addRow(grid, r++, tr("Malzeme Sayısı"), this);
  sectionCountValue_ = addRow(grid, r++, tr("Kesit Sayısı"), this);
  loadCaseCountValue_ = addRow(grid, r++, tr("Yük Durumu Sayısı"), this);
  combinationCountValue_ = addRow(grid, r++, tr("Yük Kombinasyonu Sayısı"), this);
  elasticLinkCountValue_ = addRow(grid, r++, tr("Elastik Bağlantı Sayısı"), this);
  extentsValue_ = addRow(grid, r++, tr("Model Boyutları (X/Y/Z, m)"), this);

  statusBanner_ = new QLabel(this);
  statusBanner_->setWordWrap(true);
  statusBanner_->setVisible(false);
  statusBanner_->setStyleSheet("padding: 6px; border-radius: 4px; font-weight: 600;");

  outer->addLayout(grid);
  outer->addStretch(1);
  outer->addWidget(statusBanner_);
}

void ModelInfoPanel::setData(const Info& info) {
  modelNameValue_->setText(orNA(info.hasModelName, info.modelName));
  unitSystemValue_->setText(orNA(info.hasUnitSystem, info.unitSystem));
  nodeCountValue_->setText(orNA(info.hasNodeCount, QString::number(info.nodeCount)));
  elementCountValue_->setText(orNA(info.hasElementCount, QString::number(info.elementCount)));
  materialCountValue_->setText(orNA(info.hasMaterialCount, QString::number(info.materialCount)));
  sectionCountValue_->setText(orNA(info.hasSectionCount, QString::number(info.sectionCount)));
  loadCaseCountValue_->setText(orNA(info.hasLoadCaseCount, QString::number(info.loadCaseCount)));
  combinationCountValue_->setText(orNA(info.hasCombinationCount, QString::number(info.combinationCount)));
  elasticLinkCountValue_->setText(orNA(info.hasElasticLinkCount, QString::number(info.elasticLinkCount)));
  extentsValue_->setText(orNA(info.hasExtents,
      QStringLiteral("%1 / %2 / %3").arg(info.extentX, 0, 'f', 2).arg(info.extentY, 0, 'f', 2).arg(info.extentZ, 0, 'f', 2)));
}

void ModelInfoPanel::setStatus(const QString& text, bool isError) {
  statusBanner_->setText(text);
  statusBanner_->setStyleSheet(QStringLiteral("padding: 6px; border-radius: 4px; font-weight: 600; color: white; background-color: %1;")
                                    .arg(isError ? "#EF4444" : "#22C55E"));
  statusBanner_->setVisible(true);
}

void ModelInfoPanel::reset() {
  setData(Info{});
  statusBanner_->setVisible(false);
}
