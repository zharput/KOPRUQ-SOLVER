import { readFileSync } from 'node:fs';
import { memberAxes } from '../src/model/geometry';
import { endState, memberDeformation } from '../src/model/deform';
import { parseResults } from '../src/results/parse';

// Cantilever L=4 m, E=2e8 kN/m2, Iy=1e-4 m4, tip load P=1000 kN in -z (CLI output in tests/data). Closed form (independent of the code):
// w(x) = -P x^2 (3L - x) / (6 E I), tip w = -P L^3/(3EI), tip theta_y = -dw/dx = P L^2/(2EI)
const P = 1000, L = 4, EI = 2e8 * 1e-4;
const wExact = (x: number) => -P * x * x * (3 * L - x) / (6 * EI);

describe('parseResults', () => {
  const rs = parseResults(readFileSync('tests/data/cantilever_results.json', 'utf-8'));
  it('reads the static case with solver values untouched', () => {
    expect(rs.kind).toBe('solve'); expect(rs.cases.length).toBe(1); expect(rs.failed).toEqual([]);
    const c = rs.cases[0], tip = c.nodes.find((n) => n.id === 2)!;
    expect(tip.disp[2]).toBeCloseTo(-P * L ** 3 / (3 * EI), 12);
    expect(tip.disp[4]).toBeCloseTo(P * L ** 2 / (2 * EI), 12);
    expect(c.elements[0].local.length).toBe(12);
  });
  it('rejects documents that are not result files', () => {
    expect(() => parseResults('{"schema":"kopruq-model"}')).toThrow(/Not a KOPRUQ result/);
    expect(() => parseResults('{oops')).toThrow(/not valid JSON/);
  });
  it('lists failed cases instead of showing numbers for them', () => {
    const r = parseResults(JSON.stringify({ schema: 'kopruq-results', load_cases: [{ name: 'bad', status: { success: false, message: 'singular' } }], combinations: [] }));
    expect(r.cases.length).toBe(0); expect(r.failed).toEqual([{ name: 'bad', message: 'singular' }]);
  });
  it('parses the real model-100 modal and spectrum documents when present (local build output)', () => {
    // large real files are not part of the repository; this check only runs where the build output exists
    for (const f of ['modal_consistent', 'spectrum_consistent']) {
      let t: string; try { t = readFileSync(`../build/gui-release/m100r1/${f}.json`, 'utf-8'); } catch { continue; }
      const r = parseResults(t); expect(r.cases.length).toBeGreaterThan(0); expect(r.modes.length).toBeGreaterThan(0);
      expect(r.cases[0].nodes[0].disp.every(Number.isFinite)).toBe(true);
    }
  });
});

describe('memberDeformation (Hermite, solver conventions)', () => {
  const ax = memberAxes([0, 0, 0], [4, 0, 0], [0, 1, 0])!;
  const e1 = endState([0, 0, 0, 0, 0, 0]), e2 = endState([0, 0, -P * L ** 3 / (3 * EI), 0, P * L ** 2 / (2 * EI), 0]);
  it('reproduces the cantilever elastic curve exactly along the member', () => {
    for (const xi of [0, 0.125, 0.25, 0.5, 0.75, 1]) {
      const d = memberDeformation(ax, e1, e2, xi);
      expect(d.u[2]).toBeCloseTo(wExact(xi * L), 12);
      expect(d.theta[1]).toBeCloseTo(P * (xi * L) * (2 * L - xi * L) / (2 * EI), 12); // -dw/dx = P x (2L-x)/(2EI)
    }
  });
  it('end values equal the node values; axial and twist are linear', () => {
    const a = endState([1, 2, 3, 0.1, 0.2, 0.3]), b = endState([5, 4, 3, 0.5, 0.2, 0.1]);
    expect(memberDeformation(ax, a, b, 0).u).toEqual([1, 2, 3]); expect(memberDeformation(ax, a, b, 1).u[0]).toBeCloseTo(5, 12);
    const m = memberDeformation(ax, a, b, 0.5); expect(m.u[0]).toBeCloseTo(3, 12); expect(m.theta[0]).toBeCloseTo(0.3, 12);
  });
  it('a rigid-body rotation about z gives a straight line (v = theta*x) with constant slope', () => {
    const th = 0.01, r1 = endState([0, 0, 0, 0, 0, th]), r2 = endState([0, th * L, 0, 0, 0, th]);
    for (const xi of [0.2, 0.5, 0.9]) { const d = memberDeformation(ax, r1, r2, xi); expect(d.u[1]).toBeCloseTo(th * xi * L, 12); expect(d.theta[2]).toBeCloseTo(th, 12); }
  });
  it('applies the rigid offset: u_c = u + theta x offset', () => {
    const e = endState([0, 0, 0, 0, 0, 0.1], [1, 0, 0]); // theta_z=0.1 about z, offset along x -> u_y = 0.1
    expect(e.u[1]).toBeCloseTo(0.1, 14);
  });
});

import { buildLines } from '../src/model/geometry';
import { beamStates, displace, rampColor, suggestScale } from '../src/model/displaced';
import { parseModelView } from '../src/model/load';

describe('displaced mesh (cantilever from the CLI, closed form)', () => {
  const view = parseModelView(readFileSync('tests/data/cantilever_view.json', 'utf-8'));
  const rs = parseResults(readFileSync('tests/data/cantilever_results.json', 'utf-8'));
  const disp = new Map(rs.cases[0].nodes.map((n) => [n.id, n.disp]));
  it('line vertices follow the exact elastic curve at scale 1 and scale 10', () => {
    const lines = buildLines(view, 8), st = beamStates(view, disp);
    for (const sc of [1, 10]) {
      const d = displace(lines, st, sc);
      for (let i = 0; i < lines.vXi.length; i++) { const x = lines.vXi[i] * L; expect(d.positions[i * 3 + 2]).toBeCloseTo(sc * wExact(x), 5); expect(d.positions[i * 3]).toBeCloseTo(x, 6); }
      expect(Math.max(...d.u)).toBeCloseTo(P * L ** 3 / (3 * EI), 5);
    }
  });
  it('scale 0 gives the undeformed geometry; suggested scale keeps the picture readable', () => {
    const lines = buildLines(view, 4), d = displace(lines, beamStates(view, disp), 0);
    for (let i = 0; i < d.positions.length; i++) expect(d.positions[i]).toBeCloseTo(lines.positions[i], 6);
    expect(suggestScale(100, 0.01)).toBe(800); expect(suggestScale(4, 1.0667)).toBe(1); expect(suggestScale(4, 0)).toBe(1);
  });
  it('colour ramp end points', () => { expect(rampColor(0)).toEqual([37, 99, 235]); expect(rampColor(1)).toEqual([239, 68, 68]); });
});
