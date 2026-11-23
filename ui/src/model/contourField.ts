// Result contour choices of the 3D viewer, which result files support them, and the per-vertex value fields.
import type { ResultSet } from '../results/types';
import type { StationCase, StationElement, StationRow } from '../results/stations';
import type { Quantity } from '../units/units';

export type ContourId = 'none' | 'U' | 'UX' | 'UY' | 'UZ' | 'N' | 'Vy' | 'Vz' | 'T' | 'My' | 'Mz';
export const FORCE_CONTOURS: ContourId[] = ['N', 'Vy', 'Vz', 'T', 'My', 'Mz'];
export const FORCE_INDEX: Partial<Record<ContourId, number>> = { N: 0, Vy: 1, Vz: 2, T: 3, My: 4, Mz: 5 };
export const CONTOUR_QUANTITY: Record<ContourId, Quantity> = { none: 'none', U: 'length', UX: 'length', UY: 'length', UZ: 'length', N: 'force', Vy: 'force', Vz: 'force', T: 'moment', My: 'moment', Mz: 'moment' };
export const CONTOUR_LABEL: Record<ContourId, string> = {
  none: 'None', U: 'Resultant displacement |U|', UX: 'Displacement UX', UY: 'Displacement UY', UZ: 'Displacement UZ',
  N: 'Axial force N', Vy: 'Shear Vy', Vz: 'Shear Vz', T: 'Torsion T', My: 'Bending moment My', Mz: 'Bending moment Mz',
};
export const CONTOUR_SHORT: Record<ContourId, string> = { none: '', U: '|U|', UX: 'UX', UY: 'UY', UZ: 'UZ', N: 'N', Vy: 'Vy', Vz: 'Vz', T: 'T', My: 'My', Mz: 'Mz' };

/** Whether a contour can be drawn for the selected case; `why` explains a disabled choice (nothing is ever invented). */
export function availability(id: ContourId, results: ResultSet | null, stationCase: StationCase | undefined): { ok: boolean; why?: string } {
  if (!results) return { ok: id === 'none' };
  if (id === 'none' || id === 'U' || id === 'UX' || id === 'UY' || id === 'UZ') return { ok: true };
  if (results.kind === 'modal') return { ok: false, why: 'Mode shapes carry no member forces' };
  if (results.kind === 'spectrum') return { ok: false, why: 'Spectrum values are peak magnitudes without an equilibrium state along the member' };
  if (!stationCase) return { ok: false, why: 'Station results are not available for this case' };
  return { ok: true };
}

/** Value at x along a member from its stations, linear between stations (a point load's before/after pair draws the jump). */
export function valueAt(rows: StationRow[], x: number, k: number): number {
  if (!rows.length) return NaN;
  if (x <= rows[0].x) return rows[0].f[k];
  const last = rows[rows.length - 1]; if (x >= last.x) return last.f[k];
  let lo = 0, hi = rows.length - 1;
  while (hi - lo > 1) { const mid = (lo + hi) >> 1; if (rows[mid].x <= x) lo = mid; else hi = mid; }
  const a = rows[lo], b = rows[hi]; if (b.x === a.x) return a.f[k];
  return a.f[k] + (b.f[k] - a.f[k]) * (x - a.x) / (b.x - a.x);
}

/** One value per mesh vertex (beam index, position xi) for force component `id`; NaN where a beam has no station data. */
export function forceField(mesh: { vBeam: Int32Array; vXi: Float32Array }, beamIds: number[], elements: Map<number, StationElement>, id: ContourId): Float32Array {
  const k = FORCE_INDEX[id]; const out = new Float32Array(mesh.vBeam.length).fill(NaN); if (k === undefined) return out;
  for (let i = 0; i < out.length; i++) { const e = elements.get(beamIds[mesh.vBeam[i]]); if (e) out[i] = valueAt(e.rows, mesh.vXi[i] * e.length, k); }
  return out;
}

/** About n round tick values between lo and hi (inclusive ends are always the range ends). */
export function legendTicks(lo: number, hi: number, n = 6): number[] {
  if (!(hi > lo)) return [lo];
  const raw = (hi - lo) / (n - 1), mag = Math.pow(10, Math.floor(Math.log10(raw))), f = raw / mag;
  const step = (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * mag;
  const t = [lo]; for (let v = Math.ceil(lo / step) * step; v < hi - step * 1e-9; v += step) if (v > lo + (hi - lo) * 0.07 && v < hi - (hi - lo) * 0.07) t.push(Number(v.toPrecision(12)));   // not so close to an end that the labels overlap
  t.push(hi); return t;
}
