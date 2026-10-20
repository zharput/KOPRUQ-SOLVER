#include "FileInputPanel.h"
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QSettings>

namespace {
struct RowSpec {
  const char* label;
  const char* settingsKey;     // QSettings key under "FileInputPanel/"
  const char* dialogFilter;    // QFileDialog filter string
  QStringList allowedSuffixes; // lower-case, without the dot
};

// GUI-0 found the repo's sample exports (xmidas/*.xls) are legacy BIFF, not .xlsx; GUI-4's
// ExcelReferenceReader only reads .xlsx (OOXML), per the 2026-10-20 decision to avoid vendoring
// a second, higher-risk library just for the legacy format -- a .xls file is rejected right here
// at selection time (status Unsupported) rather than showing "Seçildi" for a file GUI-4 cannot
// actually parse. Re-export from MIDAS Civil NX as .xlsx.
const QStringList kExcelSuffixes{"xlsx"};
const char* kExcelFilter = "Excel dosyaları (*.xlsx)";

const std::array<RowSpec, static_cast<std::size_t>(FileInputPanel::Row::Count)> kRows{{
  {"MIDAS NX Model (JSON)", "midasModel", "MIDAS NX JSON (*.json)", {"json"}},
  {"Beam Force (Excel)", "beamForce", kExcelFilter, kExcelSuffixes},
  {"Displacements (Global) (Excel)", "displacements", kExcelFilter, kExcelSuffixes},
  {"Elastic Link (Excel)", "elasticLink", kExcelFilter, kExcelSuffixes},
  {"Reaction (Global) (Excel)", "reactions", kExcelFilter, kExcelSuffixes},
}};

QString statusText(FileInputPanel::Status s) {
  switch (s) {
    case FileInputPanel::Status::NotSelected: return QObject::tr("Seçilmedi");
    case FileInputPanel::Status::Selected: return QObject::tr("Seçildi");
    case FileInputPanel::Status::Invalid: return QObject::tr("Geçersiz (dosya bulunamadı)");
    case FileInputPanel::Status::Unsupported: return QObject::tr("Desteklenmiyor");
  }
  return {};
}

QString statusColor(FileInputPanel::Status s) {
  switch (s) {
    case FileInputPanel::Status::NotSelected: return "#8a93ab";
    case FileInputPanel::Status::Selected: return "#3ddc84";
    case FileInputPanel::Status::Invalid: return "#ff5c5c";
    case FileInputPanel::Status::Unsupported: return "#ffb454";
  }
  return "#8a93ab";
}
}

FileInputPanel::FileInputPanel(QWidget* parent) : QGroupBox(tr("1. Girdi Dosyaları"), parent) {
  auto* grid = new QGridLayout(this);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  grid->setColumnStretch(1, 1);

  for (std::size_t i = 0; i < kRowCount; ++i) {
    const auto row = static_cast<Row>(i);
    auto* label = new QLabel(kRows[i].label, this);
    auto* field = new QLineEdit(this);
    field->setReadOnly(true);
    field->setPlaceholderText(tr("Dosya seçilmedi"));
    auto* browse = new QPushButton(tr("..."), this);
    browse->setFixedWidth(32);
    auto* status = new QLabel(this);
    status->setObjectName("statusBadge");

    const int gridRow = static_cast<int>(i);
    grid->addWidget(label, gridRow, 0);
    grid->addWidget(field, gridRow, 1);
    grid->addWidget(browse, gridRow, 2);
    grid->addWidget(status, gridRow, 3);

    pathFields_[i] = field;
    browseButtons_[i] = browse;
    statusLabels_[i] = status;
    statuses_[i] = Status::NotSelected;

    connect(browse, &QPushButton::clicked, this, [this, row] { browseFor(row); });
  }

  restorePersistedPaths();
  for (std::size_t i = 0; i < kRowCount; ++i) revalidate(static_cast<Row>(i));
}

void FileInputPanel::browseFor(Row row) {
  const auto& spec = kRows[static_cast<std::size_t>(row)];
  const QString start = filePath(row).isEmpty() ? QDir::homePath() : QFileInfo(filePath(row)).absolutePath();
  const QString chosen = QFileDialog::getOpenFileName(this, tr("Dosya seç: %1").arg(spec.label), start, spec.dialogFilter);
  if (chosen.isEmpty()) return; // user cancelled -- leave the existing selection untouched
  setPath(row, chosen, /*persist=*/true);
}

void FileInputPanel::setPath(Row row, const QString& path, bool persist) {
  pathField(row)->setText(path);
  revalidate(row);
  if (persist) {
    QSettings settings;
    settings.setValue(QStringLiteral("FileInputPanel/") + kRows[static_cast<std::size_t>(row)].settingsKey, path);
  }
}

void FileInputPanel::revalidate(Row row) {
  const std::size_t i = static_cast<std::size_t>(row);
  const QString path = filePath(row);
  Status s;
  if (path.isEmpty()) {
    s = Status::NotSelected;
  } else if (!kRows[i].allowedSuffixes.contains(QFileInfo(path).suffix().toLower())) {
    s = Status::Unsupported;
  } else if (!QFileInfo::exists(path)) {
    s = Status::Invalid;
  } else {
    s = Status::Selected;
  }
  statuses_[i] = s;
  statusLabels_[i]->setText(statusText(s));
  statusLabels_[i]->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(statusColor(s)));
  statusLabels_[i]->setToolTip(path);
  emit selectionChanged();
}

void FileInputPanel::restorePersistedPaths() {
  QSettings settings;
  for (std::size_t i = 0; i < kRowCount; ++i) {
    const QString path = settings.value(QStringLiteral("FileInputPanel/") + kRows[i].settingsKey).toString();
    if (!path.isEmpty()) pathFields_[i]->setText(path);
  }
}
