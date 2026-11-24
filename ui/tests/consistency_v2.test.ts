// Phase 7 (GUI V2): consistency between what the tables/export show and the raw CLI documents, and the unit tables against an
// independent list of physical constants. Real model-100 documents are read when the local build output exists (they are not in the repository).
import { existsSync, readFileSync } from 'node:fs';
import { buildForceRows, DISP_COLUMNS, FORCE_COLUMNS, reactionColumns, reactionRows } from '../src/results/tableModel';
import { parseModelView } from '../src/model/load';
import { parseResults } from '../src/results/parse';
import { parseStations } from '../src/results/stations';
import { factor, header, type Quantity, toDisplay, UNIT_SYSTEMS, type UnitSystemId } from '../src/units/units';

// independent: size of one display unit expressed in kN and m (definitions only, not copied from units.ts)
const KGF = 9.80665e-3; // kN
const UNIT: Record<UnitSystemId, { f: number; l: number; fl: string; ll: string }> = { 'kN.m': { f: 1, l: 1, fl: 'kN', ll: 'm' }, 'Ton.m': { f: 1000 * KGF, l: 1, fl: 'tf', ll: 'm' }, 'N.mm': { f: 1e-3, l: 1e-3, fl: 'N', ll: 'mm' }, 'kg.cm': { f: KGF, l: 1e-2, fl: 'kgf', ll: 'cm' } };
const EXP: Record<Exclude<Quantity, 'none'>, [number, number]> = { force: [1, 0], length: [0, 1], moment: [1, 1], stress: [1, -2], distLoad: [1, -1], area: [0, 2], inertia: [0, 4], unitWeight: [1, -3], mass: [1, -1] };

describe('unit tables against an independent constant list', () => {
  it('every quantity in every system: value * (kN^a m^b) / (unitF^a unitL^b)', () => {
    for (const s of UNIT_SYSTEMS) for (const [q, [a, b]] of Object.entries(EXP) as [Exclude<Quantity, 'none'>, [number, number]][]) {
      const expected = 1 / (Math.pow(UNIT[s].f, a) * Math.pow(UNIT[s].l, b)); expect(Math.abs(factor(q, s) / expected - 1)).toBeLessThan(1e-13);
    }
  });
  it('every result column header names the right unit; rotations and ids carry none', () => {
    for (const s of UNIT_SYSTEMS) {
      const U = UNIT[s];
      for (const c of FORCE_COLUMNS) { const h = header(c.name, c.q, s); if (c.id === 'id' || c.id === 'section') expect(h).toBe(c.name); else if (c.q === 'force') expect(h.endsWith(`(${U.fl})`)).toBe(true); else if (c.q === 'moment') expect(h.endsWith(`(${U.fl}·${U.ll})`)).toBe(true); else if (c.q === 'length') expect(h).toBe(`${c.name} (${U.ll})`); }
      for (const c of DISP_COLUMNS) if (c.name.startsWith('R')) expect(header(c.name, c.q, s)).toBe(c.name);
    }
  });
});

describe('cantilever documents: table values equal the raw JSON values (kN, m)', () => {
  const view = parseModelView(readFileSync('tests/data/cantilever_view.json', 'utf-8')), raw = JSON.parse(readFileSync('tests/data/cantilever_results.json', 'utf-8'));
  const results = parseResults(readFileSync('tests/data/cantilever_results.json', 'utf-8'));
  it('end forces, displacements and reactions pass through unchanged in kN.m and scale exactly in the others', () => {
    const c = results.cases[0], rc = raw.load_cases[0]; const fr = buildForceRows(view, c.elements)[0], lf: number[] = rc.elements[0].local_end_forces;
    for (const s of UNIT_SYSTEMS) {
      FORCE_COLUMNS.filter((x) => x.id.startsWith('i_') || x.id.startsWith('j_')).forEach((col, k) => { const end = col.id[0] === 'i' ? 0 : 6, idx = end + (k % 6); expect(col.get(fr, s)).toBeCloseTo(toDisplay(lf[idx], col.q, s), 9); });
      for (const n of c.nodes) { const rn = rc.nodes.find((x: { id: number }) => x.id === n.id); DISP_COLUMNS.slice(1).forEach((col, k) => expect(col.get(n, s)).toBeCloseTo(toDisplay(rn.displacement[k], col.q, s), 12)); }
      const rr = reactionRows(c); reactionColumns(false).slice(1).forEach((col, k) => expect(col.get(rr[0], s)).toBeCloseTo(toDisplay(rc.nodes[0].reaction[k], col.q, s), 9));
    }
  });
  it('the cantilever statics hold in the table: end-I moment = P L, end-I shear = P, and node 2 is not a support row', () => {
    const fr = buildForceRows(view, results.cases[0].elements)[0]; expect(Math.abs(fr.i[4])).toBeCloseTo(1000 * 4, 9); expect(Math.abs(fr.i[2])).toBeCloseTo(1000, 9); expect(reactionRows(results.cases[0]).map((n) => n.id)).toEqual([1]);
  });
});

const M = '../build/gui-release/m100r1/';
const have = existsSync(M + 'results.json') && existsSync(M + 'stations.json') && existsSync(M + 'view.json');
describe.skipIf(!have)('model 100 (local build output)', () => {
  const view = have ? parseModelView(readFileSync(M + 'view.json', 'utf-8')) : null!, results = have ? parseResults(readFileSync(M + 'results.json', 'utf-8')) : null!, st = have ? parseStations(readFileSync(M + 'stations.json', 'utf-8')) : null!;
  it('every case: one force row per beam, all values finite, equal to the raw end forces', () => {
    const raw = JSON.parse(readFileSync(M + 'results.json', 'utf-8')); expect(results.cases.length).toBe(7); expect(results.failed).toEqual([]);
    for (const [ci, c] of results.cases.entries()) {
      const src = ci < raw.load_cases.length ? raw.load_cases[ci] : raw.combinations[ci - raw.load_cases.length]; const rows = buildForceRows(view, c.elements);
      expect(rows.length).toBe(view.model.beams.length); expect(src.elements.length).toBe(rows.length);
      rows.forEach((r, k) => { expect(r.i.concat(r.j)).toEqual(src.elements[k].local_end_forces); expect(Number.isFinite(r.length)).toBe(true); });
    }
  });
  it('station documents match the result documents by name and the station ends reproduce the end forces', () => {
    expect(st.cases.map((c) => c.name)).toEqual(results.cases.map((c) => c.name));
    for (const [ci, c] of results.cases.entries()) for (const e of c.elements) { const s = st.cases[ci].elements.get(e.id)!; const a = s.rows[0].f, b = s.rows[s.rows.length - 1].f;
      for (let k = 0; k < 6; k++) { expect(Math.abs(a[k] + e.local[k])).toBeLessThan(1e-6 * (1 + Math.abs(e.local[k]))); }   // R(0) = -f1, M(0) = -m1
      for (let k = 0; k < 6; k++) { expect(Math.abs(b[k] - e.local[6 + k])).toBeLessThan(1e-6 * (1 + Math.abs(e.local[6 + k]))); } } // value at L = end force j (no loads at L)
  });
});
