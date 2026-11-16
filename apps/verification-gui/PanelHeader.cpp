#include "PanelHeader.h"
#include "IconProvider.h"

#include <QHBoxLayout>
#include <QLabel>

namespace {
constexpr int kIconSize = 17; // spec: panel header icons 16-18px
constexpr const char* kHeaderIconColor = "#38BDF8";
}

QWidget* makePanelHeader(const QString& iconName, const QString& text, QWidget* parent) {
  auto* row = new QWidget(parent);
  auto* layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 6);
  layout->setSpacing(9); // spec: 8-10px between icon and title text

  auto* icon = new QLabel(row);
  icon->setPixmap(icons::tintedPixmap(iconName, QColor(kHeaderIconColor), kIconSize));
  icon->setFixedSize(kIconSize, kIconSize);
  layout->addWidget(icon);

  auto* label = new QLabel(text, row);
  label->setStyleSheet(QStringLiteral("font-weight: 600; font-size: 14px; color: #A6B2C3;"));
  layout->addWidget(label);
  layout->addStretch(1);

  return row;
}
