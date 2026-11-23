import { type TableTab, useUi } from '../../state/store';
import { UNIT_SYSTEMS, type UnitSystemId } from '../../units/units';
import { ResultsViewer } from './ResultsViewer';

// Result categories of the Tables workspace. Entries without a tab are listed but disabled with the reason (never filled with invented data).
export const CATEGORIES: { tab: TableTab | null; label: string; why?: string }[] = [
  { tab: 'Element Forces', label: 'Element Forces' }, { tab: 'Displacements', label: 'Displacements' }, { tab: 'Reactions', label: 'Reactions' },
  { tab: null, label: 'Element Stresses', why: 'The result files carry no stresses (no section moduli in the model); not available' },
  { tab: 'Diagrams', label: 'Diagrams' }, { tab: 'Tables', label: 'Modal / Spectrum' }, { tab: 'Results', label: 'Summary' },
];

/** Top filters: Load Case, Load Combination, Result Type, Unit System. One case is active at a time (the viewer shows the same one). */
function Filters() {
  const { results, resultView, setResultView, tableTab, setTableTab, unitSystem, setUnitSystem } = useUi();
  const cases = results?.cases ?? []; const active = cases[Math.min(resultView.caseIndex, Math.max(0, cases.length - 1))];
  const plain = cases.map((c, i) => ({ c, i })).filter((x) => x.c.kind !== 'combination'), combos = cases.map((c, i) => ({ c, i })).filter((x) => x.c.kind === 'combination');
  const caseLabel = results?.kind === 'modal' ? 'Mode' : 'Load Case';
  return (
    <div className="tfilters" data-testid="table-filters">
      <label>{caseLabel}
        <select aria-label="Load case" disabled={!plain.length} value={active && active.kind !== 'combination' ? resultView.caseIndex : ''} onChange={(e) => e.target.value !== '' && setResultView({ caseIndex: +e.target.value })}>
          {(!active || active.kind === 'combination') && <option value="">—</option>}{plain.map(({ c, i }) => <option key={i} value={i}>{c.name}</option>)}</select></label>
      <label>Load Combination
        <select aria-label="Load combination" disabled={!combos.length} value={active?.kind === 'combination' ? resultView.caseIndex : ''} onChange={(e) => e.target.value !== '' && setResultView({ caseIndex: +e.target.value })}>
          {active?.kind !== 'combination' && <option value="">{combos.length ? '—' : 'none defined'}</option>}{combos.map(({ c, i }) => <option key={i} value={i}>{c.name}</option>)}</select></label>
      <label>Result Type
        <select aria-label="Result type" value={tableTab} onChange={(e) => setTableTab(e.target.value as TableTab)}>
          {CATEGORIES.map((c) => <option key={c.label} value={c.tab ?? ''} disabled={!c.tab} title={c.why}>{c.label}</option>)}</select></label>
      <label>Unit System
        <select aria-label="Unit system" value={unitSystem} onChange={(e) => setUnitSystem(e.target.value as UnitSystemId)}>{UNIT_SYSTEMS.map((u) => <option key={u} value={u}>{u}</option>)}</select></label>
      {results && results.failed.length > 0 && <span className="warn" role="status">{results.failed.length} case(s) failed and are not listed: {results.failed.map((f) => `${f.name} (${f.message})`).join('; ')}</span>}
      {active?.note && <span className="muted">{active.note}</span>}
    </div>
  );
}

/** Centre-workspace result tables: filters on top, category tabs, result views (same store state as the 3D viewer; nothing is lost when switching). */
export function TablesWorkspace() {
  const { tableTab, setTableTab } = useUi();
  return (
    <div className="tablesws" data-testid="tables-workspace">
      <Filters />
      <div className="subtabs" role="tablist" aria-label="Result categories">
        {CATEGORIES.map((c) => <button key={c.label} role="tab" className={`tab${c.tab === tableTab ? ' active' : ''}`} aria-selected={c.tab === tableTab} disabled={!c.tab} title={c.why} onClick={() => c.tab && setTableTab(c.tab)}>{c.label}</button>)}
      </div>
      <div className="tbody"><ResultsViewer tab={tableTab} /></div>
    </div>
  );
}
