import { readFileSync } from 'node:fs';
import { act, fireEvent, render, screen } from '@testing-library/react';
import { availability, forceField, legendTicks, valueAt } from '../src/model/contourField';
import { buildLines } from '../src/model/geometry';
import { parseModelView } from '../src/model/load';
import { parseResults } from '../src/results/parse';
import { parseStations } from '../src/results/stations';
import { defaultResultView, useUi } from '../src/state/store';
import { ViewerOverlays } from '../src/components/fem-viewer/ResultOverlay';
import { toDisplay } from '../src/units/units';

const view = parseModelView(readFileSync('tests/data/cantilever_view.json', 'utf-8'));
const results = parseResults(readFileSync('tests/data/cantilever_results.json', 'utf-8'));
const stations = parseStations(readFileSync('tests/data/cantilever_stations.json', 'utf-8'));

describe('force contour field', () => {
  it('cantilever My = P (L - x) at every mesh vertex (closed form); T and N are zero', () => {
    const m = buildLines(view, 8), el = stations.cases[0].elements;
    const my = forceField(m, m.beamIds, el, 'My'), vz = forceField(m, m.beamIds, el, 'Vz'), n = forceField(m, m.beamIds, el, 'N');
    for (let i = 0; i < m.vXi.length; i++) { expect(my[i]).toBeCloseTo(1000 * (4 - 4 * m.vXi[i]), 6); expect(vz[i]).toBeCloseTo(-1000, 6); expect(Math.abs(n[i])).toBeLessThan(1e-9); }
    expect(forceField(m, [999], el, 'My').every(Number.isNaN)).toBe(true);   // a member without station data is not coloured with invented values
  });
  it('valueAt interpolates linearly and keeps a before/after jump', () => {
    const rows = [{ x: 0, f: [0, 0, 0, 0, 0, 0] }, { x: 2, f: [0, 0, 5, 0, 0, 0], side: 'before' as const }, { x: 2, f: [0, 0, -3, 0, 0, 0], side: 'after' as const }, { x: 4, f: [0, 0, -3, 0, 0, 0] }];
    expect(valueAt(rows, 1, 2)).toBeCloseTo(2.5, 12); expect(valueAt(rows, 3, 2)).toBeCloseTo(-3, 12); expect(valueAt(rows, 2 - 1e-9, 2)).toBeCloseTo(5, 6); expect(valueAt(rows, 2, 2)).toBe(-3); // exactly at the load: the value just after it expect(valueAt(rows, -1, 2)).toBe(0); expect(valueAt(rows, 9, 2)).toBe(-3);
  });
  it('unit-converted values (Ton.m): 4000 kNm = 4000/9.80665 tf.m', () => { expect(toDisplay(4000, 'moment', 'Ton.m')).toBeCloseTo(4000 / 9.80665, 9); });
  it('legend ticks are round, ordered and include both ends', () => {
    expect(legendTicks(0, 10)).toEqual([0, 2, 4, 6, 8, 10]); expect(legendTicks(-1000, 4000)).toEqual([-1000, 0, 1000, 2000, 3000, 4000]); expect(legendTicks(5, 5)).toEqual([5]);
    const t = legendTicks(0.013, 0.071); expect(t[0]).toBe(0.013); expect(t[t.length - 1]).toBe(0.071); for (let i = 1; i < t.length; i++) expect(t[i]).toBeGreaterThan(t[i - 1]);
  });
});

describe('result availability follows what the result files support', () => {
  it('static with stations: all; without stations: forces off; modal and spectrum: forces off with a reason', () => {
    expect(availability('My', results, stations.cases[0]).ok).toBe(true); expect(availability('My', results, undefined).ok).toBe(false); expect(availability('UZ', results, undefined).ok).toBe(true);
    expect(availability('N', { ...results, kind: 'modal' }, undefined).why).toMatch(/Mode shapes/); expect(availability('Mz', { ...results, kind: 'spectrum' }, undefined).why).toMatch(/peak/);
    expect(availability('UX', null, undefined).ok).toBe(false); expect(availability('none', null, undefined).ok).toBe(true);
  });
});

describe('viewer overlays', () => {
  beforeEach(() => { useUi.setState({ results, stations, resultView: defaultResultView(), unitSystem: 'kN.m' }); });
  it('top bar offers case, result and display; force results are enabled when stations exist', () => {
    render(<ViewerOverlays legend={null} />);
    expect((screen.getByRole('option', { name: 'Bending moment My' }) as HTMLOptionElement).disabled).toBe(false);
    fireEvent.change(screen.getByLabelText('Display mode'), { target: { value: 'def' } }); expect(useUi.getState().resultView.deformed).toBe(true);
    fireEvent.change(screen.getByLabelText('Deformation scale'), { target: { value: '25' } }); expect(useUi.getState().resultView.scale).toBe(25);
    fireEvent.change(screen.getByLabelText('Result type'), { target: { value: 'My' } }); expect(useUi.getState().resultView.contour).toBe('My');
  });
  it('without stations the member-force results are disabled with a reason', () => {
    act(() => useUi.setState({ stations: null })); render(<ViewerOverlays legend={null} />);
    const o = screen.getByRole('option', { name: /Axial force N/ }) as HTMLOptionElement; expect(o.disabled).toBe(true); expect(o.title).toMatch(/Station results/);
  });
  it('legend shows name, unit, ticks and the manual range inputs; the unit picker changes the system', () => {
    render(<ViewerOverlays legend={{ name: 'My', unit: 'kN·m', dataMin: 0, dataMax: 4000, lo: 0, hi: 4000 }} />);
    const lg = screen.getByTestId('legend'); expect(lg.textContent).toContain('My'); expect(lg.textContent).toContain('kN·m'); expect(lg.textContent).toContain('4000');
    fireEvent.click(screen.getByLabelText('Automatic range')); expect(useUi.getState().resultView).toMatchObject({ rangeAuto: false, rangeMin: 0, rangeMax: 4000 });
    fireEvent.change(screen.getByLabelText('Range maximum'), { target: { value: '3000' } }); expect(useUi.getState().resultView.rangeMax).toBe(3000);
    fireEvent.click(screen.getByLabelText('Unit system')); fireEvent.click(screen.getByRole('menuitemradio', { name: /Ton\.m/ }));
    expect(useUi.getState().unitSystem).toBe('Ton.m'); expect(screen.getByLabelText('Unit system').textContent).toContain('Ton.m');
  });
});
