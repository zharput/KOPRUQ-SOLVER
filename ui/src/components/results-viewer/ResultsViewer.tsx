import type { ColumnDef } from '@tanstack/react-table';
import { useMemo } from 'react';
import type { ModeRow, ResultCase } from '../../results/types';
import { defaultTableUi, type TableTab, useUi } from '../../state/store';
import { DataTable, fmt } from './DataTable';
import { ResultTable } from './ResultTable';
import { buildForceRows, DISP_COLUMNS, FORCE_COLUMNS, reactionColumns, reactionRows } from '../../results/tableModel';
import { ForceDiagram } from './ForceDiagram';

const num = <T,>(key: string, get: (r: T) => number): ColumnDef<T, number> => ({ id: key, header: key, accessorFn: get, cell: (c) => fmt(c.getValue()) });

const idOf = (r: { id: number }) => r.id;

function Displacements({ c }: { c: ResultCase }) {
  return <ResultTable tableKey="disp" rows={c.nodes} columns={DISP_COLUMNS} getId={idOf} testId="tbl-disp" />;
}
function Reactions({ c }: { c: ResultCase }) {
  const rows = useMemo(() => reactionRows(c), [c]); const cols = useMemo(() => reactionColumns(rows.some((r) => r.reacLocal)), [rows]);
  if (!c.nodes.some((n) => n.reac)) return <div className="empty">This case has no reactions.</div>;
  return <ResultTable tableKey="reac" rows={rows} columns={cols} getId={idOf} testId="tbl-reac" />;
}
function Forces({ c }: { c: ResultCase }) {
  const view = useUi((s) => s.view); const ui = useUi((s) => s.tableUi.forces) ?? defaultTableUi(); const patch = useUi((s) => s.patchTableUi);
  const rows = useMemo(() => (view ? buildForceRows(view, c.elements) : []), [view, c]);
  if (!c.elements.length) return <div className="empty">This case has no element forces.</div>;
  const hidden = [...(ui.endI ? [] : ['End-I']), ...(ui.endJ ? [] : ['End-J'])];
  return <ResultTable tableKey="forces" rows={rows} columns={FORCE_COLUMNS} getId={idOf} hiddenGroups={hidden} testId="tbl-forces" footer={
    <div className="ropts" aria-label="Result options">
      <b>Result Options</b>
      <label><input type="checkbox" checked={ui.endI} onChange={(e) => patch('forces', { endI: e.target.checked })} />Show End-I</label>
      <label><input type="checkbox" checked={ui.endJ} onChange={(e) => patch('forces', { endJ: e.target.checked })} />Show End-J</label>
      <label><input type="checkbox" checked={ui.selectedOnly} onChange={(e) => patch('forces', { selectedOnly: e.target.checked })} />Show selected elements only</label>
      <span className="muted">Solver local axes; end forces = forces exerted by the nodes on the element ends (no sign change).</span>
    </div>} />;
}
function Modes() {
  const results = useUi((s) => s.results)!;
  const hasMass = results.modes.some((m) => m.effectiveMass !== undefined);
  const cols = useMemo<ColumnDef<ModeRow, any>[]>(() => [num<ModeRow>('Mode', (m) => m.index + (results.kind === 'modal' ? 0 : 1)), num<ModeRow>('f (Hz)', (m) => m.frequency), num<ModeRow>('ω (rad/s)', (m) => m.omega), num<ModeRow>('T (s)', (m) => m.period),
    ...(hasMass ? [num<ModeRow>('Eff. mass', (m) => m.effectiveMass ?? NaN), num<ModeRow>('Mass ratio', (m) => m.participation ?? NaN)] : [])], [results, hasMass]); // eslint-disable-line
  return <>{results.totalMass !== undefined && <div className="muted pad">Total mass {fmt(results.totalMass)}</div>}<DataTable data={results.modes} columns={cols} testId="tbl-modes" /></>;
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
      : results.modes.length ? <Modes /> : <div className="empty">No modal table for this analysis.</div>}</div>;
}
