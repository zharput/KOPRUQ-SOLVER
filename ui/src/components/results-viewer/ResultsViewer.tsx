import type { ColumnDef } from '@tanstack/react-table';
import { useMemo } from 'react';
import { COMPONENTS, FORCE_COLS } from '../../results/parse';
import type { ModeRow, NodeValues, ResultCase } from '../../results/types';
import { type TableTab, useUi } from '../../state/store';
import { DataTable, fmt } from './DataTable';
import { ForceDiagram } from './ForceDiagram';

const num = <T,>(key: string, get: (r: T) => number): ColumnDef<T, number> => ({ id: key, header: key, accessorFn: get, cell: (c) => fmt(c.getValue()) });

function CaseBar() {
  const { results, resultView, setResultView } = useUi();
  if (!results) return null; const c = results.cases[resultView.caseIndex];
  return (
    <div className="casebar">
      <label>Case <select aria-label="Result case" value={resultView.caseIndex} onChange={(e) => setResultView({ caseIndex: +e.target.value })}>
        {results.cases.map((k, i) => <option key={i} value={i}>{k.name}{k.kind === 'combination' ? ' (combination)' : ''}</option>)}</select></label>
      {c?.note && <span className="muted">{c.note}</span>}
      {results.failed.length > 0 && <span className="warn" role="status">{results.failed.length} case(s) failed and are not listed: {results.failed.map((f) => `${f.name} (${f.message})`).join('; ')}</span>}
    </div>
  );
}

function Displacements({ c }: { c: ResultCase }) {
  const cols = useMemo<ColumnDef<NodeValues, any>[]>(() => [num<NodeValues>('Node', (n) => n.id), ...COMPONENTS.map((k, i) => num<NodeValues>(k, (n) => n.disp[i]))], []); // eslint-disable-line
  return <DataTable data={c.nodes} columns={cols} testId="tbl-disp" />;
}
function Reactions({ c }: { c: ResultCase }) {
  const rows = useMemo(() => c.nodes.filter((n) => n.restrained?.some(Boolean) || (n.reac && n.reac.some((v) => v !== 0))), [c]);
  const local = rows.some((r) => r.reacLocal);
  const cols = useMemo<ColumnDef<NodeValues, any>[]>(() => [num<NodeValues>('Node', (n) => n.id),
    ...(['FX', 'FY', 'FZ', 'MX', 'MY', 'MZ'] as const).map((k, i) => num<NodeValues>(k, (n) => n.reac![i])),
    ...(local ? (['FX', 'FY', 'FZ', 'MX', 'MY', 'MZ'] as const).map((k, i) => num<NodeValues>(`${k} (local)`, (n) => n.reacLocal?.[i] ?? NaN)) : [])], [local]); // eslint-disable-line
  if (!c.nodes.some((n) => n.reac)) return <div className="empty">This case has no reactions.</div>;
  return <DataTable data={rows} columns={cols} testId="tbl-reac" />;
}
function Forces({ c }: { c: ResultCase }) {
  type R = { id: number; end: 'i' | 'j'; v: number[] };
  const rows = useMemo<R[]>(() => c.elements.flatMap((e) => [{ id: e.id, end: 'i' as const, v: e.local.slice(0, 6) }, { id: e.id, end: 'j' as const, v: e.local.slice(6, 12) }]), [c]);
  const cols = useMemo<ColumnDef<R, any>[]>(() => [num<R>('Element', (r) => r.id), { id: 'End', header: 'End', accessorFn: (r) => r.end, cell: (x) => x.getValue() }, ...FORCE_COLS.map((k, i) => num<R>(k, (r) => r.v[i]))], []); // eslint-disable-line
  if (!c.elements.length) return <div className="empty">This case has no element forces.</div>;
  return <><div className="muted pad">Local end forces exactly as the solver reports them (forces acting on the element ends, kN / kNm; i = first node, j = second node).</div><DataTable data={rows} columns={cols} testId="tbl-forces" /></>;
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
  return <div className="resbox"><CaseBar />
    {tab === 'Results' ? <Summary /> : tab === 'Displacements' ? <Displacements c={c} /> : tab === 'Reactions' ? <Reactions c={c} /> : tab === 'Element Forces' ? <Forces c={c} />
      : results.modes.length ? <Modes /> : <div className="empty">No modal table for this analysis.</div>}</div>;
}
