#include "MainWindow.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

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
