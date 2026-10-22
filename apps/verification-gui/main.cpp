#include "MainWindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
QString loadStylesheet() {
  const QString path = QCoreApplication::applicationDirPath() + "/resources/dark.qss";
  QFile file(path);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) return QTextStream(&file).readAll();
  return "QWidget { background-color: #0f1420; color: #e4e8f1; }";
}
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName("KOPRUQ-SOLVER Verification GUI");
  QCoreApplication::setOrganizationName("KOPRUQ-SOLVER");
  app.setStyleSheet(loadStylesheet());

  MainWindow window;
  window.show();
  return app.exec();
}
