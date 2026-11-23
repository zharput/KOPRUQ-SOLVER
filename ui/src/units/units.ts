// Central unit management (display layer only).
// The solver, its JSON files and this app's stores work in m, kN, kNm, kN/m2, rad. A unit system only changes how a
// value is DISPLAYED: shown = solverValue * factor(kind, system). Nothing here touches the model or the results.
//
// "Ton" is the metric ton-force (tf) and "kg" the kilogram-force (kgf), not masses.
// Exact definitions: 1 kgf = 9.80665 N (standard gravity), 1 tf = 1000 kgf = 9.80665 kN.
export type UnitSystemId = 'kN.m' | 'Ton.m' | 'N.mm' | 'kg.cm';
export const UNIT_SYSTEMS: UnitSystemId[] = ['kN.m', 'Ton.m', 'N.mm', 'kg.cm'];
export const DEFAULT_UNIT_SYSTEM: UnitSystemId = 'kN.m';

/** Physical dimension of a quantity. `none` (strains, ratios, radians, counts, ids) is never converted. */
export type Quantity = 'force' | 'length' | 'moment' | 'stress' | 'distLoad' | 'area' | 'inertia' | 'unitWeight' | 'mass' | 'none';

export const KGF_IN_N = 9.80665;
const KN_PER_TF = KGF_IN_N; // 1 tf = 1000 kgf = 9.80665 kN
const KN_PER_KGF = KGF_IN_N / 1000;

interface Base { forceKn: number; lengthM: number; force: string; length: string }
// size of the display force unit in kN, size of the display length unit in m
const BASE: Record<UnitSystemId, Base> = {
  'kN.m': { forceKn: 1, lengthM: 1, force: 'kN', length: 'm' },
  'Ton.m': { forceKn: KN_PER_TF, lengthM: 1, force: 'tf', length: 'm' },
  'N.mm': { forceKn: 1e-3, lengthM: 1e-3, force: 'N', length: 'mm' },
  'kg.cm': { forceKn: KN_PER_KGF, lengthM: 1e-2, force: 'kgf', length: 'cm' },
};

// exponent of (force, length) in each quantity; mass = force * s^2 / length (the solver's mass unit kN*s2/m is the tonne; time is in seconds in every system)
const DIM: Record<Quantity, [number, number]> = {
  force: [1, 0], length: [0, 1], moment: [1, 1], stress: [1, -2], distLoad: [1, -1], area: [0, 2], inertia: [0, 4], unitWeight: [1, -3], mass: [1, -1], none: [0, 0],
};

/** Multiplier turning a solver value (kN, m) into the display unit of `system`. */
export function factor(q: Quantity, system: UnitSystemId): number {
  const b = BASE[system], [df, dl] = DIM[q];
  // value in kN^df m^dl  ->  / (forceKn^df * lengthM^dl) in display units
  return 1 / (Math.pow(b.forceKn, df) * Math.pow(b.lengthM, dl));
}
export const toDisplay = (v: number, q: Quantity, system: UnitSystemId) => (q === 'none' ? v : v * factor(q, system));
export const fromDisplay = (v: number, q: Quantity, system: UnitSystemId) => (q === 'none' ? v : v / factor(q, system));

const sup = (n: number) => (n === 2 ? '²' : n === 3 ? '³' : '⁴');
/** Unit label, e.g. label('moment','Ton.m') = 'tf·m', label('stress','N.mm') = 'N/mm²'. '' for dimensionless quantities. */
export function label(q: Quantity, system: UnitSystemId): string {
  const { force: f, length: l } = BASE[system];
  switch (q) {
    case 'force': return f; case 'length': return l; case 'moment': return `${f}·${l}`; case 'stress': return `${f}/${l}${sup(2)}`;
    case 'distLoad': return `${f}/${l}`; case 'area': return `${l}${sup(2)}`; case 'inertia': return `${l}${sup(4)}`; case 'unitWeight': return `${f}/${l}${sup(3)}`; case 'mass': return `${f}·s²/${l}`;
    default: return '';
  }
}

/** Column-header text, e.g. header('My','moment','kg.cm') = 'My (kgf·cm)'; no brackets for dimensionless quantities. */
export const header = (name: string, q: Quantity, system: UnitSystemId) => (q === 'none' ? name : `${name} (${label(q, system)})`);

/** Quantity of the standard result columns. Rotations (RX..RZ) are radians and are not converted. */
export const RESULT_QUANTITY = {
  UX: 'length', UY: 'length', UZ: 'length', RX: 'none', RY: 'none', RZ: 'none',
  N: 'force', Vy: 'force', Vz: 'force', T: 'moment', My: 'moment', Mz: 'moment',
  FX: 'force', FY: 'force', FZ: 'force', MX: 'moment', MY: 'moment', MZ: 'moment',
} as const satisfies Record<string, Quantity>;

export const isUnitSystem = (s: unknown): s is UnitSystemId => typeof s === 'string' && (UNIT_SYSTEMS as string[]).includes(s);

const KEY = 'kopruq.unitSystem';
/** Saved choice (application setting); every access is guarded because storage can be unavailable. */
export function loadUnitSystem(): UnitSystemId {
  try { const v = localStorage.getItem(KEY); if (isUnitSystem(v)) return v; } catch { /* storage unavailable */ }
  return DEFAULT_UNIT_SYSTEM;
}
export function saveUnitSystem(s: UnitSystemId): void { try { localStorage.setItem(KEY, s); } catch { /* storage unavailable */ } }
