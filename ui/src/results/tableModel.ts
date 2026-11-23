// Pure helpers behind the result tables: row building from result cases, unit-aware columns, filters, id search, TSV.
// Values stay in solver units in the rows; conversion to the display unit happens in the column accessors only.
import { type ElemValues, type NodeValues, type ResultCase } from './types';
import { memberAxesOfBeam } from '../model/geometry';
import type { ModelView, Vec3 } from '../model/types';
import { header, type Quantity, toDisplay, type UnitSystemId } from '../units/units';

export interface Col<T> { id: string; name: string; q: Quantity; group?: string; width?: number; get: (r: T, unit: UnitSystemId) => number | string; text?: boolean }
export const numCol = <T,>(id: string, name: string, q: Quantity, get: (r: T) => number, o: Partial<Col<T>> = {}): Col<T> => ({ id, name, q, get: (r, u) => toDisplay(get(r), q, u), ...o });
export const textCol = <T,>(id: string, name: string, get: (r: T) => string | number, o: Partial<Col<T>> = {}): Col<T> => ({ id, name, q: 'none', get: (r) => get(r), text: true, ...o });
/** Header text with the unit of the active system; group headers carry the end name only. */
export const colHeader = <T,>(c: Col<T>, unit: UnitSystemId) => header(c.name, c.q, unit);

// ---- element forces ----
export interface ForceRow { id: number; type: string; section: number; length: number; i: number[]; j: number[] }
/** One row per beam that has end forces in the case; length = member-axis length (offsets included), section = section id of the model. */
export function buildForceRows(view: ModelView, elements: ElemValues[]): ForceRow[] {
  const nodePos = new Map<number, Vec3>(view.model.nodes.map((n) => [n.id, [n.x, n.y, n.z] as Vec3]));
  const beams = new Map(view.model.beams.map((b) => [b.id, b]));
  const rows: ForceRow[] = [];
  for (const e of elements) {
    const b = beams.get(e.id); let len = NaN;
    if (b) { const ax = memberAxesOfBeam(b, nodePos); len = Math.hypot(ax.b[0] - ax.a[0], ax.b[1] - ax.a[1], ax.b[2] - ax.a[2]); }
    rows.push({ id: e.id, type: b ? 'Beam' : '?', section: b?.section ?? NaN, length: len, i: e.local.slice(0, 6), j: e.local.slice(6, 12) });
  }
  return rows;
}
const F = ['N', 'Vy', 'Vz', 'T', 'My', 'Mz'] as const, FQ: Quantity[] = ['force', 'force', 'force', 'moment', 'moment', 'moment'];
export const FORCE_COLUMNS: Col<ForceRow>[] = [
  numCol<ForceRow>('id', 'Element ID', 'none', (r) => r.id, { width: 78 }), textCol<ForceRow>('type', 'Type', (r) => r.type, { width: 62 }),
  numCol<ForceRow>('section', 'Section', 'none', (r) => r.section, { width: 70 }), numCol<ForceRow>('length', 'L', 'length', (r) => r.length, { width: 82 }),
  ...(['i', 'j'] as const).flatMap((end) => F.map((n, k) => numCol<ForceRow>(`${end}_${n}`, n, FQ[k], (r) => r[end][k], { group: end === 'i' ? 'End-I' : 'End-J', width: 96 }))),
];

// ---- nodal tables ----
export const DISP_COLUMNS: Col<NodeValues>[] = [
  numCol<NodeValues>('id', 'Node ID', 'none', (r) => r.id, { width: 78 }),
  ...(['UX', 'UY', 'UZ'] as const).map((n, k) => numCol<NodeValues>(n, n, 'length', (r) => r.disp[k], { width: 110 })),
  ...(['RX', 'RY', 'RZ'] as const).map((n, k) => numCol<NodeValues>(n, `${n} (rad)`, 'none', (r) => r.disp[3 + k], { width: 110 })),
];
export const reactionColumns = (withLocal: boolean): Col<NodeValues>[] => [
  numCol<NodeValues>('id', 'Node ID', 'none', (r) => r.id, { width: 78 }),
  ...(['FX', 'FY', 'FZ'] as const).map((n, k) => numCol<NodeValues>(n, n, 'force', (r) => r.reac?.[k] ?? NaN, { width: 100 })),
  ...(['MX', 'MY', 'MZ'] as const).map((n, k) => numCol<NodeValues>(n, n, 'moment', (r) => r.reac?.[3 + k] ?? NaN, { width: 100 })),
  ...(withLocal ? [...(['FX', 'FY', 'FZ'] as const).map((n, k) => numCol<NodeValues>(`l${n}`, `${n}`, 'force', (r) => r.reacLocal?.[k] ?? NaN, { group: 'Local axes', width: 100 })),
                   ...(['MX', 'MY', 'MZ'] as const).map((n, k) => numCol<NodeValues>(`l${n}`, `${n}`, 'moment', (r) => r.reacLocal?.[3 + k] ?? NaN, { group: 'Local axes', width: 100 }))] : []),
];
/** Support-reaction rows: restrained nodes (or nodes with a reaction); solver order. */
export const reactionRows = (c: ResultCase): NodeValues[] => c.nodes.filter((n) => n.restrained?.some(Boolean) || (n.reac && n.reac.some((v) => v !== 0)));

// ---- filters ----
export type Predicate = (v: number | string) => boolean;
const NUM = '[-+]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][-+]?\\d+)?';
/** Column filter text: ">5", "<=-2.5", ">=1e3", "10..20" (inclusive), "=7"; anything else = case-insensitive substring of the shown text. Empty = no filter. */
export function parseFilter(text: string): Predicate | null {
  const t = text.trim(); if (!t) return null;
  let m = new RegExp(`^(>=|<=|>|<|=)\\s*(${NUM})$`).exec(t);
  if (m) { const x = parseFloat(m[2]); const op = m[1]; return (v) => { const n = typeof v === 'number' ? v : NaN; return op === '>' ? n > x : op === '<' ? n < x : op === '>=' ? n >= x : op === '<=' ? n <= x : n === x; }; }
  m = new RegExp(`^(${NUM})\\s*\\.\\.\\s*(${NUM})$`).exec(t);
  if (m) { const a = Math.min(parseFloat(m[1]), parseFloat(m[2])), b = Math.max(parseFloat(m[1]), parseFloat(m[2])); return (v) => typeof v === 'number' && v >= a && v <= b; }
  const s = t.toLowerCase(); return (v) => String(v).toLowerCase().includes(s);
}
/** Id search "1, 5, 10-20" -> predicate on the id (ranges inclusive); null when empty or malformed pieces only. */
export function parseIdQuery(text: string): ((id: number) => boolean) | null {
  const parts = text.split(/[\s,;]+/).filter(Boolean); if (!parts.length) return null;
  const ranges: [number, number][] = [];
  for (const p of parts) { const m = /^(\d+)(?:-(\d+))?$/.exec(p); if (!m) return () => false; const a = +m[1], b = m[2] ? +m[2] : a; ranges.push([Math.min(a, b), Math.max(a, b)]); }
  return (id) => ranges.some(([a, b]) => id >= a && id <= b);
}

// ---- copy / export text ----
export const toTsv = (headers: string[], rows: (number | string)[][]): string => [headers, ...rows.map((r) => r.map(String))].map((r) => r.join('\t')).join('\n');
