import type { ElemValues, NodeValues, ResultCase, ResultSet } from './types';

const num = (a: unknown, n: number): number[] => (Array.isArray(a) && a.length === n && a.every((x) => typeof x === 'number') ? (a as number[]) : new Array(n).fill(NaN));

/** Parses a CLI result document; throws a readable error for anything that is not a known result schema. */
export function parseResults(text: string): ResultSet {
  let j: any; // eslint-disable-line
  try { j = JSON.parse(text); } catch (e) { throw new Error(`Result file is not valid JSON: ${(e as Error).message}`); }
  const version = j?.metadata?.solver_version as string | undefined;
  const failed: ResultSet['failed'] = [];
  if (j?.schema === 'kopruq-results') {
    const cases: ResultCase[] = [];
    for (const [list, kind] of [[j.load_cases, 'load case'], [j.combinations, 'combination']] as const)
      for (const c of list ?? []) {
        if (!c.status?.success) { failed.push({ name: c.name, message: c.status?.message ?? 'failed' }); continue; }
        cases.push({ name: c.name, kind,
          nodes: (c.nodes ?? []).map((n: any): NodeValues => ({ id: n.id, disp: num(n.displacement, 6), reac: num(n.reaction, 6), reacLocal: n.reaction_local ? num(n.reaction_local, 6) : undefined, restrained: n.restrained })),
          elements: (c.elements ?? []).map((e: any): ElemValues => ({ id: e.id, local: num(e.local_end_forces, 12) })) });
      }
    return { kind: 'solve', cases, modes: [], failed, solverVersion: version };
  }
  if (j?.schema === 'kopruq-modal-results') {
    if (!j.status?.success) throw new Error(j.status?.message ?? 'modal analysis failed');
    const modes = (j.modes ?? []).map((m: any) => ({ index: m.index, frequency: m.frequency_hz, omega: m.angular_frequency_rad_s, period: m.frequency_hz > 0 ? 1 / m.frequency_hz : NaN }));
    const cases: ResultCase[] = (j.modes ?? []).map((m: any, i: number): ResultCase => ({
      name: `Mode ${i + 1}  (${(+m.frequency_hz).toFixed(4)} Hz)`, kind: 'mode', elements: [], note: 'Mass-normalised mode shape (arbitrary amplitude); no reactions or forces',
      nodes: (m.mode_shape ?? []).map((s: any): NodeValues => ({ id: s.id, disp: num(s.shape, 6) })) }));
    return { kind: 'modal', cases, modes, failed, solverVersion: version };
  }
  if (j?.schema === 'kopruq-response-spectrum-results') {
    const cases: ResultCase[] = []; let totalMass: number | undefined; const modes: ResultSet['modes'] = [];
    for (const c of j.cases ?? []) {
      if (!c.status?.success) { failed.push({ name: c.name, message: c.status?.message ?? 'failed' }); continue; }
      totalMass = c.total_mass;
      cases.push({ name: c.name, kind: 'spectrum', peak: true, note: 'Combined peak values (SRSS/CQC magnitudes): not a signed field, deformed shape is a magnitude envelope',
        nodes: (c.combined_nodes ?? []).map((n: any): NodeValues => ({ id: n.id, disp: num(n.displacement, 6), reac: num(n.reaction, 6) })),
        elements: (c.combined_elements ?? []).map((e: any): ElemValues => ({ id: e.id, local: num(e.local_end_forces, 12) })) });
      if (!modes.length) (c.modes ?? []).forEach((m: any, i: number) => modes.push({ index: i, frequency: m.frequency_hz, omega: 2 * Math.PI * m.frequency_hz, period: m.period_s, effectiveMass: m.effective_mass, participation: m.mass_participation_ratio }));
    }
    return { kind: 'spectrum', cases, modes, totalMass, failed, solverVersion: version };
  }
  throw new Error(`Not a KOPRUQ result document (schema = ${JSON.stringify(j?.schema)})`);
}

export const COMPONENTS = ['UX', 'UY', 'UZ', 'RX', 'RY', 'RZ'] as const;
export const FORCE_COLS = ['N', 'Vy', 'Vz', 'T', 'My', 'Mz'] as const;
