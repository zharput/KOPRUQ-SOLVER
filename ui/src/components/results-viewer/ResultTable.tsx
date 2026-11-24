import { type ColumnDef, type ColumnFiltersState, flexRender, getCoreRowModel, getFilteredRowModel, getSortedRowModel, type SortingState, useReactTable, type VisibilityState } from '@tanstack/react-table';
import { type ReactNode, useEffect, useMemo, useRef, useState } from 'react';
import { type Col, colHeader, parseFilter, parseIdQuery, toTsv } from '../../results/tableModel';
import { DEFAULT_TABLE_UI, useUi } from '../../state/store';

const NO_GROUPS: string[] = [];
import { fmt } from './DataTable';
import { download, type ExportDoc, safeFileName, toCsv, toXlsx } from '../../results/exportTable';

const ROW_H = 22, OVERSCAN = 12, SEL_W = 30;
type Id = number | string;

/** Engineering result table: group headers, unit-aware headers, numeric sort, per-column filters ("> 5", "10..20", text), id search,
 *  multi-row selection, column visibility and resizing, sticky headers, windowed rows, copy. All view state lives in the store (per `tableKey`). */
export function ResultTable<T>({ tableKey, title, rows, columns, getId, searchId, testId, hiddenGroups = NO_GROUPS, onRowClick, isActive, footer }: {
  tableKey: string; title: string; isActive?: (r: T) => boolean; rows: T[]; columns: Col<T>[]; getId: (r: T) => Id; searchId?: (r: T) => number; testId?: string; hiddenGroups?: string[]; onRowClick?: (r: T) => void; footer?: ReactNode;
}) {
  const unit = useUi((s) => s.unitSystem); const ui = useUi((s) => s.tableUi[tableKey]) ?? DEFAULT_TABLE_UI; const patch = useUi((s) => s.patchTableUi);
  const [menu, setMenu] = useState(false); const [top, setTop] = useState(0); const box = useRef<HTMLDivElement>(null);
  const selected = useMemo(() => new Set<Id>(ui.selected), [ui.selected]);

  const idPred = useMemo(() => parseIdQuery(ui.idQuery), [ui.idQuery]);
  const data = useMemo(() => rows.filter((r) => {
    const id = getId(r); const sid = searchId ? searchId(r) : id; if (idPred && !(typeof sid === 'number' && idPred(sid))) return false; if (ui.selectedOnly && !selected.has(id)) return false; return true;
  }), [rows, idPred, ui.selectedOnly, selected, getId, searchId, unit]); // `unit` in the deps: a new data array makes the table drop its per-row value cache (cells are computed from the unit)

  const defs = useMemo<ColumnDef<T, any>[]>(() => { // eslint-disable-line
    const leaf = (c: Col<T>): ColumnDef<T, any> => ({ id: c.id, header: colHeader(c, unit), accessorFn: (r) => c.get(r, unit), size: ui.widths[c.id] ?? c.width ?? 96, minSize: 40, sortDescFirst: false,
      sortingFn: c.text ? 'alphanumeric' : 'basic', filterFn: (row, id, value: string) => { const p = parseFilter(value); return p ? p(row.getValue(id)) : true; }, meta: c });
    const out: ColumnDef<T, any>[] = []; // consecutive columns with the same `group` form one group header
    for (const c of columns) {
      const last = out[out.length - 1] as { header?: string; columns?: ColumnDef<T, any>[] } | undefined;
      if (c.group) { if (last && last.header === c.group && last.columns) last.columns.push(leaf(c)); else out.push({ id: `g_${c.group}`, header: c.group, columns: [leaf(c)] } as ColumnDef<T, any>); }
      else out.push(leaf(c));
    }
    return out;
  }, [columns, unit, ui.widths]);

  const visibility = useMemo<VisibilityState>(() => { const v: VisibilityState = {}; for (const c of columns) v[c.id] = !ui.hidden.includes(c.id) && !(c.group && hiddenGroups.includes(c.group)); return v; }, [columns, ui.hidden, hiddenGroups]);
  const sorting: SortingState = ui.sorting; const filters = useMemo<ColumnFiltersState>(() => Object.entries(ui.filters).filter(([, v]) => v.trim()).map(([id, value]) => ({ id, value })), [ui.filters]);

  const table = useReactTable({
    data, columns: defs, state: { sorting, columnFilters: filters, columnVisibility: visibility }, columnResizeMode: 'onChange', enableColumnResizing: true, autoResetPageIndex: false,
    onSortingChange: (u) => patch(tableKey, { sorting: typeof u === 'function' ? u(sorting) : u }),
    onColumnSizingChange: (u) => { const cur = Object.fromEntries(table.getAllLeafColumns().map((c) => [c.id, c.getSize()])); const nx = typeof u === 'function' ? u(cur) : u; patch(tableKey, { widths: { ...ui.widths, ...nx } }); },
    getCoreRowModel: getCoreRowModel(), getSortedRowModel: getSortedRowModel(), getFilteredRowModel: getFilteredRowModel(), getRowId: (r) => String(getId(r)),
  });
  const all = table.getRowModel().rows, leaves = table.getVisibleLeafColumns();
  const h = box.current?.clientHeight || 300; const first = Math.max(0, Math.floor(top / ROW_H) - OVERSCAN), last = Math.min(all.length, Math.ceil((top + h) / ROW_H) + OVERSCAN);
  const vis = all.slice(first, last);
  const allSel = all.length > 0 && all.every((r) => selected.has(getId(r.original)));
  const setSel = (ids: Id[]) => patch(tableKey, { selected: ids });
  const toggle = (id: Id) => setSel(selected.has(id) ? ui.selected.filter((x) => x !== id) : [...ui.selected, id]);

  // rows that Copy / Export use: the selected rows when there are any, else every shown row; visible columns; values in the active unit
  const buildDoc = (): { headers: string[]; rows: (number | string)[][] } => {
    const src = (selected.size ? all.filter((r) => selected.has(getId(r.original))) : all).map((r) => leaves.map((c) => { const v = r.getValue(c.id); return typeof v === 'number' ? v : String(v); }));
    return { headers: leaves.map((c) => { const m = c.columnDef.meta as Col<T>; return (m.group ? `${m.group} ` : '') + colHeader(m, unit); }), rows: src };
  };
  const copy = async () => { const d = buildDoc(); const text = toTsv(d.headers, d.rows); try { await navigator.clipboard.writeText(text); } catch { /* clipboard unavailable: nothing to do */ } return text; };
  const st = useUi.getState;
  const exportDoc = (): ExportDoc => {
    const s0 = st(), r = s0.results, c = r?.cases[Math.min(s0.resultView.caseIndex, Math.max(0, (r?.cases.length ?? 1) - 1))], d = buildDoc();
    return { meta: [['Model Name', s0.fileName], ['Analysis Case', r ? (r.kind === 'solve' ? 'Linear Static' : r.kind === 'modal' ? 'Modal' : 'Response Spectrum') : ''], ['Load Case / Combination', c ? `${c.name}${c.kind === 'combination' ? ' (combination)' : ''}` : ''],
      ['Result Type', title], ['Unit System', unit], ['Rows', `${d.rows.length}${selected.size ? ' (selected)' : ''}`]], headers: d.headers, rows: d.rows };
  };
  const base = () => safeFileName(`${st().fileName.replace(/\.json$/i, '')}_${title}_${st().results?.cases[st().resultView.caseIndex]?.name ?? ''}_${unit}`);
  const exportCsv = () => download(`${base()}.csv`, toCsv(exportDoc()), 'text/csv;charset=utf-8');
  const exportXlsx = () => download(`${base()}.xlsx`, toXlsx(exportDoc(), title), 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet');
  // scroll the row of the externally selected object (3D viewer / Model Explorer) into view; the table is windowed, so the position is computed
  const activeIdx = isActive ? all.findIndex((r) => isActive(r.original)) : -1;
  const activeKey = activeIdx >= 0 ? getId(all[activeIdx].original) : null;
  useEffect(() => { const el = box.current; if (!el || activeIdx < 0) return; const y = activeIdx * ROW_H, h2 = el.clientHeight || 300; if (y < el.scrollTop + ROW_H * 2 || y > el.scrollTop + h2 - ROW_H * 3) el.scrollTop = Math.max(0, y - h2 / 2); }, [activeKey]); // eslint-disable-line
  const total = leaves.reduce((a, c) => a + c.getSize(), SEL_W);
  const hasGroups = table.getHeaderGroups().length > 1;

  return (
    <div className="rtable" data-testid={testId} data-rows={all.length} tabIndex={0} onKeyDown={(e) => { if ((e.ctrlKey || e.metaKey) && e.key === 'c' && !(e.target instanceof HTMLInputElement)) { e.preventDefault(); void copy(); } }}>
      <div className="rtools">
        <input aria-label="Search by ID" placeholder="Search ID (1, 5, 10-20)" value={ui.idQuery} onChange={(e) => patch(tableKey, { idQuery: e.target.value })} />
        <button className={`tbtn${ui.showFilters ? ' on' : ''}`} aria-pressed={ui.showFilters} onClick={() => patch(tableKey, { showFilters: !ui.showFilters })}>Filters</button>
        <span className="menuwrap"><button className="tbtn" aria-expanded={menu} onClick={() => setMenu(!menu)}>Columns</button>
          {menu && <div className="colmenu" role="menu">{columns.map((c) => <label key={c.id}><input type="checkbox" checked={!ui.hidden.includes(c.id)} onChange={() => patch(tableKey, { hidden: ui.hidden.includes(c.id) ? ui.hidden.filter((x) => x !== c.id) : [...ui.hidden, c.id] })} />{c.group ? `${c.group} ` : ''}{colHeader(c, unit)}</label>)}</div>}</span>
        <button className="tbtn" onClick={() => void copy()} title="Copy the selected rows (or all shown rows) as tab-separated text; Ctrl+C in the table does the same">Copy</button>
        <button className="tbtn" onClick={exportCsv} title="Export the selected rows (or all shown rows) as CSV, in the active unit system">CSV</button>
        <button className="tbtn" onClick={exportXlsx} title="Export the selected rows (or all shown rows) as an Excel workbook, in the active unit system">Excel</button>
        {ui.showFilters && Object.keys(ui.filters).length > 0 && <button className="tbtn" onClick={() => patch(tableKey, { filters: {} })}>Clear filters</button>}
        <span className="muted">{all.length} of {rows.length} rows{selected.size ? ` · ${selected.size} selected` : ''}</span>
        {isActive && activeIdx < 0 && useUi.getState().selection && <span className="muted" data-testid="hidden-selection">· the selected object is not in this table (or hidden by search / filters)</span>}
      </div>
      <div className="rscroll" ref={box} onScroll={(e) => setTop(e.currentTarget.scrollTop)}>
        <table style={{ width: total }}>
          <colgroup><col style={{ width: SEL_W }} />{leaves.map((c) => <col key={c.id} style={{ width: c.getSize() }} />)}</colgroup>
          <thead>
            {table.getHeaderGroups().map((g, gi) => (
              <tr key={g.id} className={hasGroups && gi === 0 ? 'ghead' : ''}>
                {gi === (hasGroups ? 1 : 0) ? <th className="selcol" rowSpan={hasGroups ? 1 : 1}><input type="checkbox" aria-label="Select all shown rows" checked={allSel} onChange={() => setSel(allSel ? [] : all.map((r) => getId(r.original)))} /></th> : <th className="selcol" />}
                {g.headers.map((hd) => hd.isPlaceholder ? <th key={hd.id} /> : (
                  <th key={hd.id} colSpan={hd.colSpan} aria-sort={hd.column.getIsSorted() === 'asc' ? 'ascending' : hd.column.getIsSorted() === 'desc' ? 'descending' : 'none'}
                      className={hd.colSpan > 1 ? 'grp' : ''} onClick={hd.column.getCanSort() && hd.colSpan === 1 ? hd.column.getToggleSortingHandler() : undefined}>
                    {flexRender(hd.column.columnDef.header, hd.getContext())}{{ asc: ' ▲', desc: ' ▼' }[hd.column.getIsSorted() as string] ?? ''}
                    {hd.colSpan === 1 && <span className="rz" onMouseDown={hd.getResizeHandler()} onTouchStart={hd.getResizeHandler()} onClick={(e) => e.stopPropagation()} />}
                  </th>))}
              </tr>))}
            {ui.showFilters && <tr className="frow"><th className="selcol" />{leaves.map((c) => <th key={c.id}><input aria-label={`Filter ${c.id}`} placeholder=">5, 1..9, text" value={ui.filters[c.id] ?? ''} onChange={(e) => patch(tableKey, { filters: { ...ui.filters, [c.id]: e.target.value } })} /></th>)}</tr>}
          </thead>
          <tbody>
            {first > 0 && <tr style={{ height: first * ROW_H }}><td colSpan={leaves.length + 1} /></tr>}
            {vis.map((r) => { const id = getId(r.original), on = selected.has(id);
              return (<tr key={r.id} style={{ height: ROW_H }} className={`${on ? 'sel' : ''}${isActive?.(r.original) ? ' active' : ''}`} data-row-id={String(id)} onClick={(e) => { if (e.ctrlKey || e.metaKey) toggle(id); else setSel([id]); onRowClick?.(r.original); }}>
                <td className="selcol"><input type="checkbox" aria-label={`Select ${id}`} checked={on} onClick={(e) => e.stopPropagation()} onChange={() => toggle(id)} /></td>
                {r.getVisibleCells().map((c) => { const v = c.getValue(); const m = c.column.columnDef.meta as Col<T>; return <td key={c.id} className={m.text ? '' : 'num'}>{typeof v === 'number' ? (m.id === 'id' || m.q === 'none' && Number.isInteger(v) ? String(v) : fmt(v)) : String(v)}</td>; })}
              </tr>); })}
            {last < all.length && <tr style={{ height: (all.length - last) * ROW_H }}><td colSpan={leaves.length + 1} /></tr>}
          </tbody>
        </table>
        {all.length === 0 && <div className="empty">No rows match the current search / filters.</div>}
      </div>
      {footer}
    </div>
  );
}
