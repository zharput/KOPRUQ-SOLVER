import { noHost } from '../../platform/SolverHost';

// Only analyses the solver really has are listed (linear static, modal, response spectrum); the
// controls are shown but inert until Phase 4 connects the CLI. No load-steps/tolerance/solver-type
// fields: the solver has no such parameters.
export function AnalysisControls() {
  return (
    <div>
      <h4>Analysis Case</h4>
      <div className="field"><label htmlFor="ac">Type</label>
        <select id="ac" disabled defaultValue="static"><option value="static">Linear Static</option><option>Modal</option><option>Response Spectrum</option></select></div>
      <h4>Solver Options</h4>
      <div className="field"><label htmlFor="mt">Mass (modal)</label>
        <select id="mt" disabled defaultValue="consistent"><option value="consistent">Consistent</option><option>Lumped</option></select></div>
      <div className="field"><label htmlFor="nm">Modes</label><input id="nm" type="number" disabled defaultValue={20} /></div>
      <h4>Analysis Control</h4>
      <button className="runbtn" disabled title="Not implemented yet (Phase 4)">Run Analysis</button>
      <h4>Analysis Status</h4>
      <div className="status"><ul>
        {['Model validation', 'Matrix assembly', 'Solution', 'Result processing'].map((s) => (
          <li key={s}><span>{s}</span><span className="pill">not run</span></li>))}
      </ul></div>
      <p className="empty" style={{ padding: '8px 0' }}>Solver connection: {noHost.kind === 'none' ? 'not connected (Phase 4)' : noHost.kind}</p>
    </div>
  );
}
