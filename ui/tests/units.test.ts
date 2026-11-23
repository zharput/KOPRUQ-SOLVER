import { DEFAULT_UNIT_SYSTEM, factor, fromDisplay, header, isUnitSystem, label, loadUnitSystem, type Quantity, RESULT_QUANTITY, saveUnitSystem, toDisplay, UNIT_SYSTEMS } from '../src/units/units';
import { useUi } from '../src/state/store';

// Expected values below are derived by hand from the definitions 1 kgf = 9.80665 N, 1 tf = 1000 kgf, 1 kN = 1000 N, 1 m = 100 cm = 1000 mm
// (solver values are kN, m, kNm, kN/m2).
const near = (a: number, b: number, rel = 1e-12) => expect(Math.abs(a - b)).toBeLessThanOrEqual(rel * Math.max(1, Math.abs(b)));

describe('unit conversion (display layer)', () => {
  it('kN.m is the identity', () => { for (const q of ['force', 'length', 'moment', 'stress', 'distLoad', 'area', 'inertia', 'unitWeight'] as Quantity[]) expect(factor(q, 'kN.m')).toBe(1); });
  it('Ton.m: 9.80665 kN = 1 tf; moments follow the force factor', () => {
    near(toDisplay(9.80665, 'force', 'Ton.m'), 1); near(toDisplay(98.0665, 'moment', 'Ton.m'), 10); near(toDisplay(2.5, 'length', 'Ton.m'), 2.5);
    near(toDisplay(9.80665, 'stress', 'Ton.m'), 1); near(toDisplay(9.80665, 'distLoad', 'Ton.m'), 1);
  });
  it('N.mm: 1 kN = 1000 N, 1 m = 1000 mm', () => {
    near(toDisplay(1, 'force', 'N.mm'), 1e3); near(toDisplay(1, 'length', 'N.mm'), 1e3); near(toDisplay(1, 'moment', 'N.mm'), 1e6);   // 1 kNm = 1e6 N.mm
    near(toDisplay(3e7, 'stress', 'N.mm'), 3e4);        // E = 3e7 kN/m2 = 30 GPa = 3e4 N/mm2
    near(toDisplay(1, 'distLoad', 'N.mm'), 1);          // 1 kN/m = 1000 N / 1000 mm = 1 N/mm
    near(toDisplay(1, 'area', 'N.mm'), 1e6); near(toDisplay(1, 'inertia', 'N.mm'), 1e12); near(toDisplay(25, 'unitWeight', 'N.mm'), 25e-6); // 25 kN/m3 = 25e-6 N/mm3
  });
  it('kg.cm: 1 kN = 1000/9.80665 kgf, 1 m = 100 cm', () => {
    near(toDisplay(1, 'force', 'kg.cm'), 1000 / 9.80665); near(toDisplay(1, 'length', 'kg.cm'), 100);
    near(toDisplay(1, 'moment', 'kg.cm'), 1000 / 9.80665 * 100);
    near(toDisplay(1, 'stress', 'kg.cm'), 1000 / 9.80665 / 1e4);   // 1 kN/m2 = 0.0101972 kgf/cm2
    near(toDisplay(30e3, 'stress', 'kg.cm'), 30e3 * 1000 / 9.80665 / 1e4); // 30 MPa ~ 305.9 kgf/cm2
    expect(toDisplay(30e3, 'stress', 'kg.cm')).toBeGreaterThan(305.9); expect(toDisplay(30e3, 'stress', 'kg.cm')).toBeLessThan(306.0);
  });
  it('kgf is a force, not a mass: 1 kgf <-> 9.80665 N exactly; round trip is exact for all systems and quantities', () => {
    near(toDisplay(9.80665e-3, 'force', 'kg.cm'), 1);
    for (const s of UNIT_SYSTEMS) for (const q of ['force', 'length', 'moment', 'stress', 'distLoad', 'area', 'inertia', 'unitWeight'] as Quantity[]) for (const v of [-123.456, 0, 1e-7, 4.2e9]) near(fromDisplay(toDisplay(v, q, s), q, s), v, 1e-13);
  });
  it('dimensionless quantities are never converted', () => { for (const s of UNIT_SYSTEMS) { expect(toDisplay(0.37, 'none', s)).toBe(0.37); expect(fromDisplay(0.37, 'none', s)).toBe(0.37); expect(label('none', s)).toBe(''); } });
  it('result columns: rotations stay radians in every system; displacements follow length; forces follow force', () => {
    for (const r of ['RX', 'RY', 'RZ'] as const) for (const s of UNIT_SYSTEMS) expect(toDisplay(0.5, RESULT_QUANTITY[r], s)).toBe(0.5);
    expect(RESULT_QUANTITY.UZ).toBe('length'); expect(RESULT_QUANTITY.My).toBe('moment'); expect(RESULT_QUANTITY.Vy).toBe('force'); expect(RESULT_QUANTITY.MX).toBe('moment');
  });
  it('labels and headers', () => {
    expect(header('My', 'moment', 'kN.m')).toBe('My (kN·m)'); expect(header('My', 'moment', 'Ton.m')).toBe('My (tf·m)'); expect(header('N', 'force', 'kg.cm')).toBe('N (kgf)');
    expect(label('stress', 'N.mm')).toBe('N/mm²'); expect(label('length', 'N.mm')).toBe('mm'); expect(label('distLoad', 'kg.cm')).toBe('kgf/cm'); expect(header('RX', 'none', 'kN.m')).toBe('RX');
  });
});

describe('unit setting', () => {
  it('is saved and reloaded; unknown or unavailable storage falls back to kN.m', () => {
    saveUnitSystem('N.mm'); expect(loadUnitSystem()).toBe('N.mm');
    localStorage.setItem('kopruq.unitSystem', 'furlongs'); expect(loadUnitSystem()).toBe(DEFAULT_UNIT_SYSTEM); expect(isUnitSystem('furlongs')).toBe(false);
    const get = Storage.prototype.getItem; Storage.prototype.getItem = () => { throw new Error('blocked'); };
    try { expect(loadUnitSystem()).toBe(DEFAULT_UNIT_SYSTEM); } finally { Storage.prototype.getItem = get; }
  });
  it('the store keeps the choice without touching results', () => {
    useUi.getState().setUnitSystem('Ton.m'); expect(useUi.getState().unitSystem).toBe('Ton.m'); expect(loadUnitSystem()).toBe('Ton.m');
    useUi.getState().setUnitSystem('kN.m');
  });
});
