import { type TableTab, useUi } from '../../state/store';
import { ResultsViewer } from './ResultsViewer';

// Result categories of the Tables workspace. `todo` entries are listed but disabled with the reason (never filled with invented data).
const CATEGORIES: { tab: TableTab | null; label: string; why?: string }[] = [
  { tab: 'Element Forces', label: 'Element Forces' }, { tab: 'Displacements', label: 'Displacements' }, { tab: 'Reactions', label: 'Reactions' },
  { tab: null, label: 'Element Stresses', why: 'The result files carry no stresses (no section moduli in the model); not available' },
  { tab: 'Diagrams', label: 'Diagrams' }, { tab: 'Tables', label: 'Modal / Spectrum' }, { tab: 'Results', label: 'Summary' },
];

/** Centre-workspace result tables: category tabs over the result views (same store state as the 3D viewer, nothing is lost when switching). */
export function TablesWorkspace() {
  const { tableTab, setTableTab } = useUi();
  return (
    <div className="tablesws" data-testid="tables-workspace">
      <div className="subtabs" role="tablist" aria-label="Result categories">
        {CATEGORIES.map((c) => <button key={c.label} role="tab" className={`tab${c.tab === tableTab ? ' active' : ''}`} aria-selected={c.tab === tableTab} disabled={!c.tab} title={c.why} onClick={() => c.tab && setTableTab(c.tab)}>{c.label}</button>)}
      </div>
      <div className="tbody"><ResultsViewer tab={tableTab} /></div>
    </div>
  );
}
