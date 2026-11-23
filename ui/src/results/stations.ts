// Element station results: the CLI's `stations` document (kopruq-element-stations v1, schemas/element_stations.schema.json).
// f = [N, Vy, Vz, T, My, Mz] in the solver's local axes: force/moment exerted on the +x face of the piece [0,x]
// by the part beyond x. Values are shown as the solver gives them (no MIDAS sign flip).
export interface StationRow { x: number; f: number[]; side?: 'before' | 'after' }
export interface StationElement { id: number; length: number; rows: StationRow[]; closureForce: number; closureMoment: number }
export interface StationCase { name: string; kind: 'load case' | 'combination'; elements: Map<number, StationElement> }
export interface StationSet { cases: StationCase[]; failed: { name: string; message: string }[]; convention: string }

export function parseStations(text: string): StationSet {
  let j: any; // eslint-disable-line
  try { j = JSON.parse(text); } catch (e) { throw new Error(`Station file is not valid JSON: ${(e as Error).message}`); }
  if (j?.schema !== 'kopruq-element-stations') throw new Error(`Not a station result document (schema = ${JSON.stringify(j?.schema)})`);
  const cases: StationCase[] = [], failed: StationSet['failed'] = [];
  for (const [list, kind] of [[j.load_cases, 'load case'], [j.combinations, 'combination']] as const)
    for (const c of list ?? []) {
      if (!c.status?.success) { failed.push({ name: c.name, message: c.status?.message ?? 'failed' }); continue; }
      const elements = new Map<number, StationElement>();
      for (const e of c.elements ?? []) elements.set(e.id, { id: e.id, length: e.length, closureForce: e.closure_force, closureMoment: e.closure_moment,
        rows: (e.stations ?? []).map((s: any): StationRow => ({ x: s.x, f: s.f, side: s.side })) });
      cases.push({ name: c.name, kind, elements });
    }
  return { cases, failed, convention: j.metadata?.convention ?? '' };
}

export const FORCE_LABELS = ['N (kN)', 'Vy (kN)', 'Vz (kN)', 'T (kNm)', 'My (kNm)', 'Mz (kNm)'] as const;
/** Points [x, value] of component `k` for ECharts (a point load's before/after pair draws the jump as a vertical step). */
export const diagramPoints = (rows: StationRow[], k: number): [number, number][] => rows.map((r) => [r.x, r.f[k]]);
/** Plot values of a component: floating-point residue (below 1e-9 of the largest value of any component of the member, absolute floor 1e-9) is drawn as 0. */
export const cleanForPlot = (pts: [number, number][], scale: number): [number, number][] => { const t = Math.max(1e-9, 1e-9 * scale); return pts.map(([x, v]) => [x, Math.abs(v) < t ? 0 : v]); };
export const rowsScale = (rows: StationRow[]) => rows.reduce((m, r) => Math.max(m, ...r.f.map(Math.abs)), 0);
/** Largest closure error over all elements of a case (equilibrium of end forces and member loads). */
export const maxClosure = (c: StationCase) => { let f = 0, m = 0; for (const e of c.elements.values()) { f = Math.max(f, e.closureForce); m = Math.max(m, e.closureMoment); } return { force: f, moment: m }; };
