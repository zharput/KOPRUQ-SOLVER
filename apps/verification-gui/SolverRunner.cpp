#include "SolverRunner.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
#if defined(Q_OS_WIN)
constexpr const char* kCliName = "kopruq_solver_cli.exe";
#else
constexpr const char* kCliName = "kopruq_solver_cli";
#endif
}

SolverRunner::SolverRunner(QObject* parent) : QObject(parent) {
  process_ = new QProcess(this);
  connect(process_, &QProcess::readyReadStandardOutput, this, [this] {
    for (const auto& line : QString::fromUtf8(process_->readAllStandardOutput()).split('\n', Qt::SkipEmptyParts))
      emit logLine(line);
  });
  connect(process_, &QProcess::readyReadStandardError, this, [this] {
    for (const auto& line : QString::fromUtf8(process_->readAllStandardError()).split('\n', Qt::SkipEmptyParts))
      emit logLine(line);
  });
  connect(process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
    emit logLine(tr("CLI başlatılamadı: %1").arg(process_->errorString()));
  });
  connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          [this](int exitCode, QProcess::ExitStatus status) { emit finished(exitCode, status, pendingOutputPath_); });
}

SolverRunner::~SolverRunner() { cancel(); }

bool SolverRunner::isRunning() const { return process_->state() != QProcess::NotRunning; }

QString SolverRunner::locateCli() {
  const QString appDir = QCoreApplication::applicationDirPath();
  const QStringList candidates{
    appDir + "/" + kCliName,
    appDir + "/../release/" + kCliName,
    appDir + "/../../release/" + kCliName,
    appDir + "/../debug/" + kCliName,
    appDir + "/../../debug/" + kCliName,
  };
  for (const auto& c : candidates)
    if (QFileInfo::exists(c)) return QDir::cleanPath(c);
  const QString onPath = QStandardPaths::findExecutable(kCliName);
  return onPath; // empty if not found anywhere -- never fabricated
}

void SolverRunner::run(const QString& cliPath, const QString& modelPath, const QString& format,
                        const QString& outputPath, const QString& label) {
  if (isRunning()) { emit logLine(tr("Zaten bir işlem çalışıyor, önce onun bitmesi bekleniyor.")); return; }
  if (cliPath.isEmpty() || !QFileInfo::exists(cliPath)) {
    emit logLine(tr("kopruq_solver_cli bulunamadı -- GUI-7'de paketleme tamamlanana kadar CLI'nin "
                     "bu GUI'nin yanında veya PATH'te olması gerekiyor."));
    emit finished(/*exitCode=*/-1, QProcess::CrashExit, {});
    return;
  }

  pendingOutputPath_ = outputPath;
  QStringList args{"solve", modelPath, "-o", outputPath};
  if (format != "native") { args << "--format" << format; }

  emit logLine(QStringLiteral("[%1] %2 çalıştırılıyor: %3 %4")
                   .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                   .arg(label)
                   .arg(cliPath)
                   .arg(args.join(' ')));
  emit started();
  process_->start(cliPath, args);
}

void SolverRunner::cancel() {
  if (!isRunning()) return;
  process_->terminate();
  if (!process_->waitForFinished(2000)) process_->kill();
}
