#include "FileInputPanel.h"
#include <QGridLayout>

namespace {
struct RowSpec { const char* label; const char* defaultStatus; };
constexpr std::array<RowSpec, static_cast<std::size_t>(FileInputPanel::Row::Count)> kRows{{
  {"MIDAS NX Model (JSON)", "Seçilmedi"},
  {"Beam Force (Excel)", "Seçilmedi"},
  {"Displacements (Global) (Excel)", "Seçilmedi"},
  {"Elastic Link (Excel)", "Seçilmedi"},
  {"Reaction (Global) (Excel)", "Seçilmedi"},
}};
}

FileInputPanel::FileInputPanel(QWidget* parent) : QGroupBox(tr("1. Girdi Dosyaları"), parent) {
  auto* grid = new QGridLayout(this);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(6);
  grid->setColumnStretch(1, 1);

  for (std::size_t i = 0; i < kRowCount; ++i) {
    auto* label = new QLabel(kRows[i].label, this);
    auto* field = new QLineEdit(this);
    field->setReadOnly(true);
    field->setPlaceholderText(tr("Dosya seçilmedi"));
    auto* browse = new QPushButton(tr("..."), this);
    browse->setFixedWidth(32);
    auto* status = new QLabel(kRows[i].defaultStatus, this);
    status->setObjectName("statusBadge");

    const int row = static_cast<int>(i);
    grid->addWidget(label, row, 0);
    grid->addWidget(field, row, 1);
    grid->addWidget(browse, row, 2);
    grid->addWidget(status, row, 3);

    pathFields_[i] = field;
    browseButtons_[i] = browse;
    statusLabels_[i] = status;
  }
}
