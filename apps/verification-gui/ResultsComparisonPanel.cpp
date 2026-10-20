#include "ResultsComparisonPanel.h"
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

QTableWidget* ResultsComparisonPanel::buildTable(const QStringList& headers, QWidget* parent) {
  auto* table = new QTableWidget(0, headers.size(), parent);
  table->setHorizontalHeaderLabels(headers);
  table->horizontalHeader()->setStretchLastSection(true);
  table->verticalHeader()->setVisible(false);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSortingEnabled(true);
  return table;
}

QWidget* ResultsComparisonPanel::buildSummaryTab() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  auto* tiles = new QHBoxLayout();
  auto addTile = [&](const QString& title, QLabel*& valueOut) {
    auto* box = new QGroupBox(page);
    auto* inner = new QVBoxLayout(box);
    inner->addWidget(new QLabel(title, box));
    valueOut = new QLabel("0", box);
    QFont f = valueOut->font();
    f.setPointSize(f.pointSize() + 6);
    f.setBold(true);
    valueOut->setFont(f);
    inner->addWidget(valueOut);
    tiles->addWidget(box);
  };
  addTile(tr("Toplam Karşılaştırılan"), totalCompared_);
  addTile(tr("Başarılı (PASS)"), totalPass_);
  addTile(tr("Başarısız (FAIL)"), totalFail_);
  addTile(tr("Karşılaştırılamayan (N/A)"), totalNa_);
  layout->addLayout(tiles);

  categoryTable_ = buildTable({tr("Kategori"), tr("Toplam"), tr("Başarılı"), tr("Başarısız"), tr("N/A"),
                                tr("Maks. |Δ|"), tr("Maks. |Δ| %"), tr("Durum"), tr("Detay")}, page);
  layout->addWidget(categoryTable_, 1);
  return page;
}

ResultsComparisonPanel::ResultsComparisonPanel(QWidget* parent) : QWidget(parent) {
  auto* outer = new QVBoxLayout(this);

  auto* group = new QGroupBox(tr("4. Sonuç Karşılaştırma Tabloları"), this);
  auto* groupLayout = new QVBoxLayout(group);
  tabs_ = new QTabWidget(group);

  tabs_->addTab(buildSummaryTab(), tr("Özet"));

  displacementsTable_ = buildTable({tr("Yük Durumu"), tr("Düğüm ID"), tr("Bileşen"), tr("MIDAS Değeri"),
                                     tr("SOLVER Değeri"), tr("İşaretli Fark"), tr("Mutlak Fark"),
                                     tr("Bağıl Fark"), tr("Birim"), tr("Durum")}, group);
  tabs_->addTab(displacementsTable_, tr("Deplasmanlar (Global)"));

  reactionsTable_ = buildTable({tr("Yük Durumu"), tr("Düğüm ID"), tr("Bileşen"), tr("MIDAS Değeri"),
                                 tr("SOLVER Değeri"), tr("İşaretli Fark"), tr("Mutlak Fark"),
                                 tr("Bağıl Fark"), tr("Birim"), tr("Durum")}, group);
  tabs_->addTab(reactionsTable_, tr("Mesnet Tepkimeleri (Global)"));

  beamForcesTable_ = buildTable({tr("Yük Durumu"), tr("Eleman ID"), tr("Uç/İstasyon"), tr("Kuvvet Bileşeni"),
                                  tr("MIDAS Değeri"), tr("SOLVER Değeri"), tr("Fark"), tr("Birim"), tr("Durum")}, group);
  tabs_->addTab(beamForcesTable_, tr("Kiriş İç Kuvvetleri"));

  elasticLinksTable_ = buildTable({tr("Yük Durumu"), tr("Link ID"), tr("Uç"), tr("Bileşen"), tr("MIDAS Değeri"),
                                    tr("SOLVER Değeri"), tr("Fark"), tr("Birim"), tr("Durum")}, group);
  tabs_->addTab(elasticLinksTable_, tr("Elastik Bağlantılar"));

  diagnosticsTable_ = buildTable({tr("Kategori"), tr("Kaynak"), tr("Açıklama"), tr("Önem")}, group);
  tabs_->addTab(diagnosticsTable_, tr("Diagnostik"));

  groupLayout->addWidget(tabs_);
  outer->addWidget(group, 1);

  auto* settingsStrip = new QGroupBox(this);
  auto* settingsLayout = new QHBoxLayout(settingsStrip);
  settingsLayout->addWidget(new QLabel(tr("Karşılaştırma Ayarları"), settingsStrip));
  settingsLayout->addSpacing(16);
  settingsLayout->addWidget(new QLabel(tr("Mutlak Tolerans:"), settingsStrip));
  absTolField_ = new QLineEdit(settingsStrip);
  absTolField_->setFixedWidth(80);
  settingsLayout->addWidget(absTolField_);
  settingsLayout->addWidget(new QLabel(tr("Bağıl Tolerans:"), settingsStrip));
  relTolField_ = new QLineEdit(settingsStrip);
  relTolField_->setFixedWidth(80);
  settingsLayout->addWidget(relTolField_);
  settingsLayout->addStretch(1);
  saveSettingsButton_ = new QPushButton(tr("Ayarları Kaydet"), settingsStrip);
  saveSettingsButton_->setEnabled(false); // GUI-2: QSettings persistence, not wired yet.
  settingsLayout->addWidget(saveSettingsButton_);
  outer->addWidget(settingsStrip);
}

void ResultsComparisonPanel::setSummaryCounts(int total, int pass, int fail, int na) {
  totalCompared_->setText(QString::number(total));
  totalPass_->setText(QString::number(pass));
  totalFail_->setText(QString::number(fail));
  totalNa_->setText(QString::number(na));
}
