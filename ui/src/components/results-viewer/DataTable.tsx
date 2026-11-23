import { type ColumnDef, flexRender, getCoreRowModel, getSortedRowModel, type SortingState, useReactTable } from '@tanstack/react-table';
import { useMemo, useRef, useState } from 'react';

const ROW_H = 22, OVERSCAN = 12;
export const fmt = (v: unknown) => typeof v === 'number' ? (Number.isFinite(v) ? (v === 0 ? '0' : Math.abs(v) >= 1e5 || Math.abs(v) < 1e-4 ? v.toExponential(5) : v.toPrecision(7).replace(/\.?0+$/, '')) : 'NaN') : String(v ?? '');

/** Sortable, windowed table (only the visible rows are in the DOM). Numbers are shown as the solver gave them (7 significant digits). */
export function DataTable<T>({ data, columns, testId }: { data: T[]; columns: ColumnDef<T, any>[]; testId?: string }) { // eslint-disable-line
  const [sorting, setSorting] = useState<SortingState>([]);
  const table = useReactTable({ data, columns, state: { sorting }, onSortingChange: setSorting, getCoreRowModel: getCoreRowModel(), getSortedRowModel: getSortedRowModel() });
  const rows = table.getRowModel().rows; const box = useRef<HTMLDivElement>(null); const [top, setTop] = useState(0);
  const h = box.current?.clientHeight || 300;
  const first = Math.max(0, Math.floor(top / ROW_H) - OVERSCAN), last = Math.min(rows.length, Math.ceil((top + h) / ROW_H) + OVERSCAN);
  const vis = useMemo(() => rows.slice(first, last), [rows, first, last]);
  return (
    <div className="dtable" ref={box} onScroll={(e) => setTop(e.currentTarget.scrollTop)} data-testid={testId} data-rows={rows.length}>
      <table>
        <thead>{table.getHeaderGroups().map((g) => <tr key={g.id}>{g.headers.map((hd) => (
          <th key={hd.id} onClick={hd.column.getToggleSortingHandler()} aria-sort={hd.column.getIsSorted() === 'asc' ? 'ascending' : hd.column.getIsSorted() === 'desc' ? 'descending' : 'none'}>
            {flexRender(hd.column.columnDef.header, hd.getContext())}{{ asc: ' ▲', desc: ' ▼' }[hd.column.getIsSorted() as string] ?? ''}</th>))}</tr>)}</thead>
        <tbody>
          {first > 0 && <tr style={{ height: first * ROW_H }}><td colSpan={columns.length} /></tr>}
          {vis.map((r) => <tr key={r.id} style={{ height: ROW_H }}>{r.getVisibleCells().map((c) => <td key={c.id} className={typeof c.getValue() === 'number' ? 'num' : ''}>{flexRender(c.column.columnDef.cell, c.getContext())}</td>)}</tr>)}
          {last < rows.length && <tr style={{ height: (rows.length - last) * ROW_H }}><td colSpan={columns.length} /></tr>}
        </tbody>
      </table>
    </div>
  );
}
