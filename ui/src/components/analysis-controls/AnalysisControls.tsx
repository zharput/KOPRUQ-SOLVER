import { LogView } from '../results-viewer/LogView';
import { cancelAnalysis, startAnalysis } from '../../state/runner';
import { type AnalysisKind, type StageState, useUi } from '../../state/store';

const STAGES: [string, string][] = [['read', 'Read model'], ['solve', 'Solve'], ['write', 'Write results']];
const pill = (s: StageState | undefined) => (s === 'done' ? 'done' : s === 'running' ? 'running…' : s === 'failed' ? 'failed' : 'not run');

// Only analyses the solver really has. Status reflects what the CLI reports (read / solve / write);
// "Completed" appears only after the CLI exited 0 and its result file says success.
export function AnalysisControls() {
  const { host, solver, source, analysis, modes, massType, run, set, view } = useUi();
  const desktop = host.kind === 'tauri' && solver.state === 'connected';
  const canRun = desktop && !!source && run.state !== 'running';
  const why = host.kind !== 'tauri' ? 'Desktop app required' : solver.state !== 'connected' ? 'Solver not found' : !source ? (view ? 'Open the model file itself (not a model-view file) to analyse it' : 'Open a model first') : '';
  const running = run.state === 'running';
  return (
    <div>
      <h4>Analysis Case</h4>
      <div className="field"><label htmlFor="ac">Type</label>
        <select id="ac" value={analysis} disabled={running} onChange={(e) => set({ analysis: e.target.value as AnalysisKind })}>
          <option value="solve">Linear Static</option><option value="modal">Modal</option><option value="spectrum">Response Spectrum</option></select></div>
      <h4>Solver Options</h4>
      <div className="field"><label htmlFor="mt">Mass (modal / spectrum)</label>
        <select id="mt" value={massType} disabled={running || analysis === 'solve'} onChange={(e) => set({ massType: e.target.value as 'consistent' | 'lumped' })}>
          <option value="consistent">Consistent</option><option value="lumped">Lumped</option></select></div>
      <div className="field"><label htmlFor="nm">Modes (modal)</label>
        <input id="nm" type="number" min={1} max={10000} value={modes} disabled={running || analysis !== 'modal'} onChange={(e) => set({ modes: Math.max(1, Math.min(10000, Number(e.target.value) || 1)) })} /></div>
      <h4>Analysis Control</h4>
      {running
        ? <button className="runbtn live" onClick={() => void cancelAnalysis()}>Cancel Analysis</button>
        : <button className="runbtn live" disabled={!canRun} title={why || 'Run the solver on the open model'} onClick={() => void startAnalysis()}>Run Analysis</button>}
      <h4>Analysis Status</h4>
      <div className="status"><ul>
        {STAGES.map(([k, label]) => (<li key={k}><span>{label}</span><span className={`pill${run.stages[k] === 'failed' ? ' bad' : run.stages[k] === 'done' ? ' ok' : ''}`}>{pill(run.stages[k])}</span></li>))}
      </ul></div>
      {run.state === 'done' && <p className="okline">Completed ({run.seconds?.toFixed(2)} s) — {run.summary}</p>}
      {run.state === 'failed' && <p className="badline" role="alert">Failed: {run.message}</p>}
      {run.state === 'cancelled' && <p className="empty" style={{ padding: '8px 0' }}>Cancelled.</p>}
      <p className="empty" style={{ padding: '8px 0' }} data-testid="solver-connection">
        Solver: {solver.state === 'connected' ? `${solver.version}` : solver.state === 'missing' ? `not available — ${solver.error ?? ''}` : 'checking…'}</p>
      <details className="logbox" open={run.state === 'failed' || undefined}><summary>Solver log{run.log.length ? ` (${run.log.length})` : ''}</summary><LogView /></details>
      {!canRun && !running && <p className="empty" style={{ padding: 0 }}>{why}</p>}
    </div>
  );
}
