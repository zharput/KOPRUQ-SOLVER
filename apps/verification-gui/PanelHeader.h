#pragma once
// V2.1: a small icon + bold text row, used in place of QGroupBox's native (icon-less, plain-text
// only) title on every panel, so each panel header can carry a light-blue line icon per the
// visual revision spec. The caller still constructs its QGroupBox with an empty title and
// inserts this as the first widget in its own layout.
#include <QWidget>

QWidget* makePanelHeader(const QString& iconName, const QString& text, QWidget* parent);
