import { readFileSync } from 'node:fs';
import { act, fireEvent, render, screen, within } from '@testing-library/react';
import { vi } from 'vitest';
import App from '../src/App';
import { parseModelView } from '../src/model/load';
import { parseResults } from '../src/results/parse';
import { envelopeRows, linkRows, modalColumns, spectrumRows } from '../src/results/tableModel';
import { defaultResultView, useUi } from '../src/state/store';
import { label, toDisplay } from '../src/units/units';

vi.mock('../src/components/fem-viewer/FEMViewer', () => ({ FEMViewer: () => <div data-testid="fem-viewer" /> }));
const viewText = readFileSync('tests/data/cantilever_view.json', 'utf-8'); const view = parseModelView(viewText);
const ok = { success: true, code: 'Success', message: '' };
const el = (id: number, local: number[]) => ({ id, local_end_forces: local, global_end_forces: local });
const v12 = (a: number[]) => [...a, ...a.map((x) => -x)];
const staticDoc = (cases: [string, number[]][], combos: [string, number[]][] = [], springs = false) => JSON.stringify({ schema: 'kopruq-results', schema_version: 1, metadata: {}, status: ok,
  load_cases: cases.map(([name, f]) => ({ name, status: ok, nodes: [{ id: 1, displacement: [0, 0, 0, 0, 0, 0], reaction: [0, 0, 0, 0, 0, 0], restrained: [true, true, true, true, true, true] }], elements: [el(7, f)], ...(springs ? { springs: [el(3, f)] } : {}) })),
  combinations: combos.map(([name, f]) => ({ name, status: ok, nodes: [], elements: [el(7, f)] })) });
const modalDoc = JSON.stringify({ schema: 'kopruq-modal-results', schema_version: 1, metadata: {}, status: ok, modes: [{ index: 0, frequency_hz: 2, angular_frequency_rad_s: 4 * Math.PI, mode_shape: [{ id: 1, shape: [0, 0, 1, 0, 0, 0] }] }, { index: 1, frequency_hz: 5, angular_frequency_rad_s: 10 * Math.PI, mode_shape: [{ id: 1, shape: [0, 0, 2, 0, 0, 0] }] }] });
const spectrumDoc = JSON.stringify({ schema: 'kopruq-response-spectrum-results', schema_version: 2, metadata: {}, status: ok, cases: [{ name: 'SPY', status: ok, total_mass: 120, total_mass_participation_ratio: 0.92,
  modes: [{ frequency_hz: 2, period_s: 0.5, mass_participation_ratio: 0.6, effective_mass: 72, participation_factor: 8 }, { frequency_hz: 4, period_s: 0.25, mass_participation_ratio: 0.32, effective_mass: 38.4, participation_factor: 6 }],
  combined_nodes: [{ id: 1, displacement: [0, 0, 0.01, 0, 0, 0], reaction: [0, 0, 5, 0, 0, 0] }], combined_elements: [el(7, v12([1, 2, 3, 4, 5, 6]).slice(0, 12))] }] });

describe('units: mass', () => {
  it('kN s2/m (tonne) in every system', () => {
    expect(label('mass', 'kN.m')).toBe('kN·s²/m'); expect(label('mass', 'N.mm')).toBe('N·s²/mm'); expect(label('mass', 'kg.cm')).toBe('kgf·s²/cm');
    expect(toDisplay(1, 'mass', 'kN.m')).toBe(1); expect(toDisplay(9.80665, 'mass', 'Ton.m')).toBeCloseTo(1, 12);   // 9.80665 kN s2/m = 1 tf s2/m
    expect(toDisplay(1, 'mass', 'N.mm')).toBeCloseTo(1, 12);                                                         // 1000 N s2/m = 1 N s2/mm
    expect(toDisplay(1, 'mass', 'kg.cm')).toBeCloseTo(1000 / 9.80665 / 100, 12);
  });
});

describe('envelopes (signed static results only)', () => {
  const A = [10, -2, 3, 0, -50, 8, 1, 1, 1, 1, 1, 1], B = [-30, 5, -1, 2, 20, -9, 2, -7, 0, 0, 0, 0], C = [4, 4, -8, -1, 10, 1, 3, 3, 3, 3, 3, 3];
  const cases = parseResults(staticDoc([['A', A], ['B', B], ['C', C]])).cases;
  it('max / min per component over cases (hand computed)', () => {
    const r = envelopeRows(view, cases, 'envelope'); expect(r.length).toBe(2);
    const max = r.find((x) => x.tag === 'Max')!, min = r.find((x) => x.tag === 'Min')!;
    expect(max.i).toEqual([10, 5, 3, 2, 20, 8]); expect(min.i).toEqual([-30, -2, -8, -1, -50, -9]); expect(max.j).toEqual([3, 3, 3, 3, 3, 3]); expect(min.j).toEqual([1, -7, 0, 0, 0, 0]);
  });
  it('absolute maximum keeps the signed value of the largest magnitude', () => {
    const [r] = envelopeRows(view, cases, 'absmax'); expect(r.tag).toBeUndefined(); expect(r.i).toEqual([-30, 5, -8, 2, -50, -9]); expect(r.j).toEqual([3, -7, 3, 3, 3, 3]);
  });
});

describe('modal, spectrum and link tables', () => {
  it('modal: eigenvalue = omega^2, no participation columns (the modal file has none)', () => {
    const r = parseResults(modalDoc); const cols = modalColumns(false); expect(cols.map((c) => c.id)).toEqual(['id', 'f', 'T', 'lambda']);
    expect(cols.find((c) => c.id === 'lambda')!.get(r.modes[0], 'kN.m')).toBeCloseTo((4 * Math.PI) ** 2, 9); expect(r.modes[1].period).toBeCloseTo(0.2, 12);
  });
  it('spectrum: per-case modes with ratio and effective mass in the selected unit; direction from the model, method not invented', () => {
    const r = parseResults(spectrumDoc), c = r.cases[0]; expect(c.modeRows!.length).toBe(2); const cols = modalColumns(true);
    expect(cols.find((x) => x.id === 'meff')!.get(c.modeRows![0], 'Ton.m')).toBeCloseTo(72 / 9.80665, 12); expect(cols.find((x) => x.id === 'ratio')!.get(c.modeRows![1], 'N.mm')).toBe(0.32);
    const v = parseModelView(JSON.stringify({ ...JSON.parse(viewText), model: { ...JSON.parse(viewText).model, response_spectra: [{ name: 'SPY', direction: 'y' }] } }));
    const [row] = spectrumRows(v, r.cases); expect(row).toMatchObject({ name: 'SPY', direction: 'Y', modes: 2, totalMass: 120, ratio: 0.92 }); expect(spectrumRows(view, r.cases)[0].direction).toBe('—');
  });
  it('elastic links: link-local end forces are kept per case', () => {
    const r = parseResults(staticDoc([['A', [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]]], [], true)); const rows = linkRows(r.cases[0].springs!);
    expect(rows[0]).toMatchObject({ id: 3, i: [1, 2, 3, 4, 5, 6], j: [7, 8, 9, 10, 11, 12] });
  });
});

describe('Tables workspace: advanced categories', () => {
  const setup = (doc: string) => { useUi.setState({ view: null, results: null, stations: null, resultView: defaultResultView(), selection: null, tableUi: {}, unitSystem: 'kN.m', centerTab: 'tables' }); act(() => useUi.getState().loadText(viewText, 'c')); act(() => useUi.setState({ results: parseResults(doc), tableTab: 'Element Forces', centerTab: 'tables' })); render(<App />); };
  it('Spring / Link Results is disabled when the analysis has no links, enabled and filled when it has', () => {
    setup(staticDoc([['A', Array(12).fill(1)]])); expect((screen.getByRole('tab', { name: 'Spring / Link Results' }) as HTMLButtonElement).disabled).toBe(true);
  });
  it('link results table', () => {
    setup(staticDoc([['A', Array.from({ length: 12 }, (_, i) => i + 1)]], [], true)); fireEvent.click(screen.getByRole('tab', { name: 'Spring / Link Results' }));
    const t = screen.getByTestId('tbl-links'); expect(t.getAttribute('data-rows')).toBe('1'); expect(within(t).getAllByRole('columnheader', { name: /^End-/ }).length).toBe(2);
  });
  it('envelope options follow the result kind: radios work for static cases and combinations, disabled for spectrum peaks', () => {
    setup(staticDoc([['A', Array(12).fill(1)], ['B', Array(12).fill(-4)]], [['C1', Array(12).fill(9)]]));
    expect((screen.getByLabelText('Envelope (max / min)') as HTMLInputElement).disabled).toBe(false);
    fireEvent.click(screen.getByLabelText('Envelope (max / min)')); const t = screen.getByTestId('tbl-forces'); expect(t.getAttribute('data-rows')).toBe('2'); expect(within(t).getByText('Max')).toBeTruthy();
    fireEvent.change(screen.getByLabelText('Envelope scope'), { target: { value: 'combinations' } }); expect(within(screen.getByTestId('tbl-forces')).getAllByText('9').length).toBeGreaterThan(0);
    fireEvent.click(screen.getByLabelText('Absolute maximum')); expect(screen.getByTestId('tbl-forces').getAttribute('data-rows')).toBe('1');
  });
  it('spectrum results: envelopes disabled (peak magnitudes), Modal/Spectrum tab shows the participation of the selected case', () => {
    setup(spectrumDoc); expect((screen.getByLabelText('Envelope (max / min)') as HTMLInputElement).disabled).toBe(true); expect(screen.getByLabelText('Envelope (max / min)').closest('label')!.title).toMatch(/Linear Static/);
    fireEvent.click(screen.getByRole('tab', { name: 'Modal / Spectrum' })); expect(screen.getByTestId('tbl-spectra').getAttribute('data-rows')).toBe('1'); expect(screen.getByTestId('tbl-modes').getAttribute('data-rows')).toBe('2');
    expect(within(screen.getByTestId('tbl-modes')).getByText('Effective modal mass (kN·s²/m)')).toBeTruthy();
  });
  it('modal results: forces and reactions are disabled with a reason; the modal table lists frequency, period and eigenvalue', () => {
    setup(modalDoc); const f = screen.getByRole('tab', { name: 'Element Forces' }) as HTMLButtonElement; expect(f.disabled).toBe(true); expect(f.title).toMatch(/Mode shapes/);
    fireEvent.click(screen.getByRole('tab', { name: 'Modal / Spectrum' })); const t = screen.getByTestId('tbl-modes'); expect(within(t).getByText('Eigenvalue ω² (rad²/s²)')).toBeTruthy(); expect(t.getAttribute('data-rows')).toBe('2');
  });
});
