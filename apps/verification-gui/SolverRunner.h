#pragma once
// GUI-3: real `kopruq_solver_cli` invocation via QProcess, asynchronous (never blocks the GUI
// thread). No FEM computation happens here or anywhere else in the GUI -- every number this app
// will ever show comes from this CLI's own stdout/results.json, per the architecture split in
// docs/SOLVER_ROADMAP.md's "QT6 VERIFICATION GUI" section 4.
#include <QObject>
#include <QProcess>
#include <QString>

class SolverRunner : public QObject {
  Q_OBJECT
public:
  explicit SolverRunner(QObject* parent = nullptr);
  ~SolverRunner() override;

  bool isRunning() const;

  // Searches a handful of locations a developer build or a packaged deployment could plausibly
  // put kopruq_solver_cli[.exe] in, next to or near this GUI executable, then falls back to
  // PATH. Returns an empty string, never a guessed/fabricated path, if none of them exist --
  // GUI-7 is responsible for a deployment layout that makes this unambiguous; until then this
  // is a best-effort developer convenience, not a guarantee.
  static QString locateCli();

  // Runs `cli solve <modelPath> -o <outputPath> [--format <format>]` (format == "native" omits
  // the flag). `label` is cosmetic, only used in log lines, so the same runner can serve both
  // the "Modeli Doğrula" and "Solver'ı Çalıştır" actions without duplicating process plumbing --
  // the two differ only in how the caller interprets the exit code, not in what actually runs.
  void run(const QString& cliPath, const QString& modelPath, const QString& format,
           const QString& outputPath, const QString& label);

  // Terminates an in-flight run, if any -- used when the window closes or a new run is
  // requested, never silently left as an orphaned background process.
  void cancel();

signals:
  void logLine(const QString& line);
  void started();
  // success is exitCode == 0 for a "solve" the caller cares about actually succeeding;
  // callers that only need validity (exit code != 3) interpret exitCode themselves.
  void finished(int exitCode, QProcess::ExitStatus status, const QString& outputPath);

private:
  QProcess* process_{};
  QString pendingOutputPath_;
};
