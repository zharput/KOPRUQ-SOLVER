import { useCallback, useMemo } from 'react';
import type { ResultCase } from '../../results/types';
import { DEFAULT_TABLE_UI, type TableTab, useUi } from '../../state/store';
import { ResultTable } from './ResultTable';
import { buildForceRows, DISP_COLUMNS, envelopeRows, forceColumns, LINK_COLUMNS, linkRows, modalColumns, reactionColumns, reactionRows, SPECTRUM_COLUMNS, spectrumRows, type TaggedForceRow } from '../../results/tableModel';
import { ForceDiagram } from './ForceDiagram';

const NO_MODES: never[] = [];
/** Table <-> viewer/explorer synchronisation: a clicked row selects the node/element globally; the globally selected object marks (and scrolls to) its row. */
function useSel(kind: 'node' | 'element') {
  const sel = useUi((s) => s.selection); const select = useUi((s) => s.select);
  const isActive = useCallback((r: { id: number }) => !!sel && sel.kind === kind && sel.id === r.id, [sel, kind]);
  const click = useCallback((r: { id: number }) => select({ kind, id: r.id }), [select, kind]);
  return { isActive, click };
}
const idOf = (r: { id: number }) => r.id;
const forceId = (r: TaggedForceRow) => (r.tag ? `${r.id}-${r.tag}` : r.id);
const modeId = (m: { index: number }) => m.index;

function Displacements({ c }: { c: ResultCase }) {
  const sel = useSel('node'); return <ResultTable tableKey="disp" title="Displacements" isActive={sel.isActive} onRowClick={sel.click} rows={c.nodes} columns={DISP_COLUMNS} getId={idOf} testId="tbl-disp" />;
}
function Reactions({ c }: { c: ResultCase }) {
  const sel = useSel('node'); const rows = useMemo(() => reactionRows(c), [c]); const cols = useMemo(() => reactionColumns(rows.some((r) => r.reacLocal)), [rows]);
  if (!c.nodes.some((n) => n.reac)) return <div className="empty">This case has no reactions.</div>;
  return <ResultTable tableKey="reac" title="Reactions" isActive={sel.isActive} onRowClick={sel.click} rows={rows} columns={cols} getId={idOf} testId="tbl-reac" />;
}
function Forces({ c }: { c: ResultCase }) {
  const view = useUi((s) => s.view); const results = useUi((s) => s.results)!; const ui = useUi((s) => s.tableUi.forces) ?? DEFAULT_TABLE_UI; const patch = useUi((s) => s.patchTableUi);
  const sel = useSel('element'); const signed = results.kind === 'solve';                                   // spectrum peaks are magnitudes, mode shapes carry no forces: no envelopes there
  const pool = useMemo(() => results.cases.filter((x) => x.kind === (ui.envScope === 'cases' ? 'load case' : 'combination')), [results, ui.envScope]);
  const mode = signed && ui.envMode !== 'case' && pool.length ? ui.envMode : 'case';
  const rows = useMemo<TaggedForceRow[]>(() => (!view ? [] : mode === 'case' ? buildForceRows(view, c.elements) : envelopeRows(view, pool, mode)), [view, c, mode, pool]);
  const hidden = useMemo(() => [...(ui.endI ? [] : ['End-I']), ...(ui.endJ ? [] : ['End-J'])], [ui.endI, ui.endJ]);
  const cols = useMemo(() => forceColumns(mode === 'envelope'), [mode]); const tui = useUi((st) => st.tableUi[`forces-${mode}`]) ?? DEFAULT_TABLE_UI;
  if (!c.elements.length) return <div className="empty">This case has no element forces.</div>;
  const why = !signed ? 'Only for Linear Static results (signed load cases / combinations)' : undefined;
  return <ResultTable tableKey={`forces-${mode}`} title={mode === 'case' ? 'Element Forces' : mode === 'absmax' ? 'Element Forces (absolute max)' : 'Element Forces (envelope)'} isActive={sel.isActive} onRowClick={sel.click} rows={rows} columns={cols} getId={forceId} searchId={idOf} hiddenGroups={hidden} testId="tbl-forces" footer={
    <div className="ropts" aria-label="Result options">
      <b>Result Options</b>
      <label><input type="checkbox" checked={ui.endI} onChange={(e) => patch('forces', { endI: e.target.checked })} />Show End-I</label>
      <label><input type="checkbox" checked={ui.endJ} onChange={(e) => patch('forces', { endJ: e.target.checked })} />Show End-J</label>
      <label><input type="checkbox" checked={tui.selectedOnly} onChange={(e) => patch(`forces-${mode}`, { selectedOnly: e.target.checked })} />Show selected elements only</label>
      <label title={why}><input type="radio" name="envmode" disabled={!signed} checked={mode === 'case'} onChange={() => patch('forces', { envMode: 'case' })} />Selected case</label>
      <label title={why}><input type="radio" name="envmode" disabled={!signed || !pool.length} checked={mode === 'absmax'} onChange={() => patch('forces', { envMode: 'absmax' })} />Absolute maximum</label>
      <label title={why}><input type="radio" name="envmode" disabled={!signed || !pool.length} checked={mode === 'envelope'} onChange={() => patch('forces', { envMode: 'envelope' })} />Envelope (max / min)</label>
      {signed && <label>over <select aria-label="Envelope scope" value={ui.envScope} onChange={(e) => patch('forces', { envScope: e.target.value as 'cases' | 'combinations' })}><option value="cases">load cases</option><option value="combinations">combinations</option></select></label>}
      <span className="muted">{mode === 'case' ? 'Solver local axes; end forces = forces exerted by the nodes on the element ends (no sign change).' : `Per component over ${pool.length} signed ${ui.envScope === 'cases' ? 'load case(s)' : 'combination(s)'}; the result type of the other group is never mixed in.`}</span>
    </div>} />;
}
function Springs({ c }: { c: ResultCase }) {
  const rows = useMemo(() => linkRows(c.springs ?? []), [c]);
  if (!c.springs?.length) return <div className="empty">This case has no elastic-link results.</div>;
  return <><div className="muted pad">Forces exerted by the nodes on the link, in the link's own axes (SpringLink.hpp); not converted.</div><ResultTable tableKey="links" title="Spring Link Results" rows={rows} columns={LINK_COLUMNS} getId={idOf} testId="tbl-links" /></>;
}
function Modes({ c }: { c: ResultCase }) {
  const view = useUi((s) => s.view); const results = useUi((s) => s.results)!;
  const spectrum = results.kind === 'spectrum'; const rows = spectrum ? (c.modeRows ?? NO_MODES) : results.modes;
  const cols = useMemo(() => modalColumns(spectrum), [spectrum]); const sp = useMemo(() => spectrumRows(view, results.cases), [view, results]);
  return <div className="resbox">
    {spectrum && <><div className="muted pad">Response-spectrum cases (direction from the model's own spectrum definition; the modal combination method is not recorded in any result or model file, so it is not shown).</div>
      <div style={{ height: 150, display: 'flex', flex: '0 0 auto' }}><ResultTable tableKey="spcases" title="Response Spectrum Cases" rows={sp} columns={SPECTRUM_COLUMNS} getId={idOf} testId="tbl-spectra" /></div>
      <div className="muted pad">Modes of the selected case: <b>{c.name}</b></div></>}
    <ResultTable tableKey={spectrum ? 'modes-sp' : 'modes'} title={spectrum ? 'Modes (spectrum case)' : 'Modal Results'} rows={rows} columns={cols} getId={modeId} testId="tbl-modes" />
  </div>;
}
function Summary() {
  const { results, run } = useUi(); const r = results!;
  return <div className="pad" data-testid="results-summary">
    <div><b>{r.kind === 'solve' ? 'Static analysis' : r.kind === 'modal' ? 'Modal analysis' : 'Response-spectrum analysis'}</b> — {run.summary ?? ''}{r.solverVersion ? ` · solver ${r.solverVersion}` : ''}</div>
    <ul>{r.cases.map((c, i) => <li key={i}>{c.name} <span className="muted">({c.kind}, {c.nodes.length} nodes, {c.elements.length} elements)</span></li>)}</ul>
    {r.failed.map((f, i) => <div key={i} className="warn">{f.name}: {f.message}</div>)}</div>;
}

export function ResultsViewer({ tab }: { tab: TableTab }) {
  const { results, resultView } = useUi();
  if (tab === 'Diagrams') return <ForceDiagram />;
  if (!results) return <div className="empty" data-testid="results-empty">{tab}: no results. Run an analysis in the desktop app; the values of the last successful run appear here.</div>;
  if (!results.cases.length) return <div className="empty">The analysis produced no usable case.{results.failed.map((f) => ` ${f.name}: ${f.message}`)}</div>;
  const c = results.cases[Math.min(resultView.caseIndex, results.cases.length - 1)];
  return <div className="resbox">
    {tab === 'Results' ? <Summary /> : tab === 'Displacements' ? <Displacements c={c} /> : tab === 'Reactions' ? <Reactions c={c} /> : tab === 'Element Forces' ? <Forces c={c} />
      : tab === 'Springs' ? <Springs c={c} /> : results.modes.length || c.modeRows?.length ? <Modes c={c} /> : <div className="empty">No modal table for this analysis.</div>}</div>;
}
