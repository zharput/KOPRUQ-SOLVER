#include "MainWindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <QTextStream>

// vcpkg's x64-linux triplet defaults to VCPKG_LIBRARY_LINKAGE=static (x64-windows defaults to
// dynamic), so on Linux there is no libqxcb.so for Qt to load at runtime at all -- the xcb
// platform plugin is a static library baked into the Qt6::Gui/Qt6::Widgets archives, and a
// static Qt build needs the plugin registered explicitly via Q_IMPORT_PLUGIN (confirmed by
// Qt's own "Using static linking will disable the use of dynamically loaded plugins. Make sure
// to import all needed static plugins..." build-time note; see docs/SOLVER_ROADMAP.md's GUI-7
// entry, item 13). Windows keeps loading qwindows.dll dynamically via windeployqt, unaffected.
#if defined(Q_OS_LINUX)
#include <QtPlugin>
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
#endif

namespace {
// Loads the QSS copied next to the executable by CMake's POST_BUILD step; falls back to a
// minimal inline dark theme if the resources directory wasn't deployed (e.g. run from the build
// tree before a packaging step), so the app is still usable rather than failing to start.
// V2.0: the QSS references the checkmark PNG (also copied by the same POST_BUILD step) via a
// {CHECK_ICON} placeholder, substituted here with an absolute, forward-slash path -- Qt's QSS
// url() needs a real file path (no data: URI support) and a relative path is unreliable since
// the GUI's working directory isn't guaranteed to be its own install directory.
QString loadStylesheet() {
  const QString dir = QCoreApplication::applicationDirPath();
  const QString path = dir + "/resources/dark.qss";
  const QString checkIcon = QDir::fromNativeSeparators(dir + "/resources/check.png");
  QFile file(path);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QTextStream(&file).readAll().replace("{CHECK_ICON}", checkIcon);
  }
  return "QWidget { background-color: #0B1220; color: #E5EAF2; }";
}

// Fusion is the only Qt style with QSS support consistent enough, on both Windows and Linux, for
// the checkbox checkmark glyph (and every other custom-colored control) to render reliably --
// the platform-native styles (windowsvista, etc.) are known to partially ignore
// QCheckBox::indicator QSS rules.
void applyFusionDarkPalette(QApplication& app) {
  app.setStyle(QStyleFactory::create("fusion"));
  QPalette pal;
  pal.setColor(QPalette::Window, QColor("#0B1220"));
  pal.setColor(QPalette::WindowText, QColor("#E5EAF2"));
  pal.setColor(QPalette::Base, QColor("#0D1523"));
  pal.setColor(QPalette::AlternateBase, QColor("#131F31"));
  pal.setColor(QPalette::ToolTipBase, QColor("#131F31"));
  pal.setColor(QPalette::ToolTipText, QColor("#E5EAF2"));
  pal.setColor(QPalette::Text, QColor("#E5EAF2"));
  pal.setColor(QPalette::Button, QColor("#131F31"));
  pal.setColor(QPalette::ButtonText, QColor("#E5EAF2"));
  pal.setColor(QPalette::BrightText, QColor("#EF4444"));
  pal.setColor(QPalette::Highlight, QColor("#2563EB"));
  pal.setColor(QPalette::HighlightedText, QColor("#E5EAF2"));
  pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#A6B2C3"));
  pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#A6B2C3"));
  app.setPalette(pal);
}
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("KOPRUQ-SOLVER Verification GUI");
  QCoreApplication::setOrganizationName("KOPRUQ-SOLVER");
  applyFusionDarkPalette(app);
  app.setStyleSheet(loadStylesheet());

  MainWindow window;
  window.show();
  return app.exec();
}
