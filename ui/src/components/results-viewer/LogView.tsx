import { useUi } from '../../state/store';

/** The CLI's own stderr lines of the current/last run (import report, warnings, failure reason). */
export function LogView() {
  const log = useUi((s) => s.run.log);
  if (!log.length) return <div className="empty" data-testid="log-empty">No log yet. Run an analysis in the desktop app.</div>;
  return <pre className="log" data-testid="log">{log.join('\n')}</pre>;
}
