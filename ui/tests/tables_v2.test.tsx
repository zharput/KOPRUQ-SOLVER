import { readFileSync } from 'node:fs';
import { act, fireEvent, render, screen, within } from '@testing-library/react';
import { vi } from 'vitest';
import App from '../src/App';
import { parseModelView } from '../src/model/load';
import { parseResults } from '../src/results/parse';
import { buildForceRows, colHeader, FORCE_COLUMNS, parseFilter, parseIdQuery, reactionRows, toTsv } from '../src/results/tableModel';
import { defaultResultView, useUi } from '../src/state/store';
import { toDisplay } from '../src/units/units';

vi.mock('../src/components/fem-viewer/FEMViewer', () => ({ FEMViewer: () => <div data-testid="fem-viewer" /> }));
const viewText = readFileSync('tests/data/cantilever_view.json', 'utf-8'), resText = readFileSync('tests/data/cantilever_results.json', 'utf-8');
const view = parseModelView(viewText), results = parseResults(resText);

describe('table model (pure)', () => {
  it('force rows: ids, type, section, member length and end forces exactly from the solver (cantilever L = 4 m, P = 1000 kN)', () => {
    const r = buildForceRows(view, results.cases[0].elements); expect(r.length).toBe(1);
    expect(r[0]).toMatchObject({ id: 7, type: 'Beam', section: 1 }); expect(r[0].length).toBeCloseTo(4, 12);
    expect(r[0].i).toEqual([0, 0, 1000, 0, -4000, 0]); expect(r[0].j).toEqual([0, 0, -1000, 0, 0, 0]);
  });
  it('column values follow the unit system; headers carry the unit; rotations and ids are not converted', () => {
    const row = buildForceRows(view, results.cases[0].elements)[0], my = FORCE_COLUMNS.find((c) => c.id === 'i_My')!, len = FORCE_COLUMNS.find((c) => c.id === 'length')!, id = FORCE_COLUMNS.find((c) => c.id === 'id')!;
    expect(my.get(row, 'kN.m')).toBe(-4000); expect(my.get(row, 'Ton.m')).toBeCloseTo(-4000 / 9.80665, 9); expect(my.get(row, 'N.mm')).toBeCloseTo(-4e9, 3);
    expect(len.get(row, 'kg.cm')).toBeCloseTo(400, 9); expect(id.get(row, 'N.mm')).toBe(7);
    expect(colHeader(my, 'Ton.m')).toBe('My (tf·m)'); expect(colHeader(FORCE_COLUMNS.find((c) => c.id === 'j_N')!, 'N.mm')).toBe('N (N)'); expect(colHeader(len, 'N.mm')).toBe('L (mm)');
    expect(toDisplay(1000, 'force', 'kg.cm')).toBeCloseTo(1000 * 1000 / 9.80665, 6);
  });
  it('column filters: comparisons, ranges, text', () => {
    expect(parseFilter('')).toBeNull(); const gt = parseFilter('> 5')!; expect(gt(6)).toBe(true); expect(gt(5)).toBe(false);
    expect(parseFilter('<=-2.5')!(-2.5)).toBe(true); expect(parseFilter('10..20')!(15)).toBe(true); expect(parseFilter('20..10')!(25)).toBe(false); expect(parseFilter('=7')!(7)).toBe(true);
    expect(parseFilter('Bea')!('Beam')).toBe(true); expect(parseFilter('1e3')!(1000)).toBe(false); expect(parseFilter('>1e3')!(2000)).toBe(true);
  });
  it('id search: lists and inclusive ranges; malformed text matches nothing', () => {
    const p = parseIdQuery('1, 5, 10-12')!; expect([1, 2, 5, 10, 11, 12, 13].filter(p)).toEqual([1, 5, 10, 11, 12]); expect(parseIdQuery('  ')).toBeNull(); expect(parseIdQuery('abc')!(1)).toBe(false);
  });
  it('tsv and reaction rows', () => { expect(toTsv(['a', 'b'], [[1, 'x'], [2.5, 'y']])).toBe('a\tb\n1\tx\n2.5\ty'); expect(reactionRows(results.cases[0]).map((n) => n.id)).toEqual([1]); });
});

describe('Tables workspace', () => {
  beforeEach(() => { useUi.setState({ view: null, results: null, stations: null, resultView: defaultResultView(), selection: null, tableUi: {}, unitSystem: 'kN.m', tableTab: 'Element Forces', centerTab: 'tables' }); act(() => useUi.getState().loadText(viewText, 'c')); act(() => useUi.setState({ results, tableTab: 'Element Forces', centerTab: 'tables' })); });
  it('top filters list cases/combinations, result type and unit; changing the unit rewrites headers and values without re-analysis', () => {
    render(<App />);
    expect(within(screen.getByTestId('table-filters')).getByLabelText('Load case')).toBeTruthy(); expect((screen.getByLabelText('Load combination') as HTMLSelectElement).disabled).toBe(true);
    const t = screen.getByTestId('tbl-forces'); expect(within(t).getAllByText('My (kN·m)').length).toBe(2); expect(within(t).getByText('-4000')).toBeTruthy();
    fireEvent.change(screen.getByLabelText('Unit system'), { target: { value: 'Ton.m' } });
    expect(within(t).getAllByText('My (tf·m)').length).toBe(2); expect(within(t).getByText(String(Number((-4000 / 9.80665).toPrecision(7))))).toBeTruthy();
    expect(useUi.getState().results!.cases[0].elements[0].local[4]).toBe(-4000);   // the stored result is untouched
  });
  it('End-I / End-J groups, group headers, option toggles and column visibility', () => {
    render(<App />);
    const t = screen.getByTestId('tbl-forces'); expect(within(t).getByRole('columnheader', { name: 'End-I' })).toBeTruthy(); expect(within(t).getByRole('columnheader', { name: 'End-J' })).toBeTruthy();
    fireEvent.click(screen.getByLabelText('Show End-J')); expect(within(t).queryByRole('columnheader', { name: 'End-J' })).toBeNull(); expect(within(t).getAllByText('My (kN·m)').length).toBe(1);
    fireEvent.click(screen.getByRole('button', { name: 'Columns' })); expect(within(t).queryByRole('columnheader', { name: 'Section' })).not.toBeNull(); fireEvent.click(screen.getByLabelText(/Section/)); expect(within(t).queryByRole('columnheader', { name: 'Section' })).toBeNull();
  });
  it('search, column filter, multi-selection and "selected only" persist across tab switches', () => {
    render(<App />);
    fireEvent.change(screen.getByLabelText('Search by ID'), { target: { value: '99' } }); expect(screen.getByTestId('tbl-forces').getAttribute('data-rows')).toBe('0');
    fireEvent.change(screen.getByLabelText('Search by ID'), { target: { value: '5-9' } }); expect(screen.getByTestId('tbl-forces').getAttribute('data-rows')).toBe('1');
    fireEvent.click(screen.getByRole('button', { name: 'Filters' })); fireEvent.change(screen.getByLabelText('Filter length'), { target: { value: '>5' } }); expect(screen.getByTestId('tbl-forces').getAttribute('data-rows')).toBe('0');
    fireEvent.change(screen.getByLabelText('Filter length'), { target: { value: '3..5' } }); expect(screen.getByTestId('tbl-forces').getAttribute('data-rows')).toBe('1');
    fireEvent.click(screen.getByLabelText('Select 7')); expect(useUi.getState().tableUi.forces.selected).toEqual([7]);
    fireEvent.click(screen.getByRole('tab', { name: 'Displacements' })); fireEvent.click(screen.getByRole('tab', { name: 'Element Forces' }));
    expect((screen.getByLabelText('Search by ID') as HTMLInputElement).value).toBe('5-9'); expect((screen.getByLabelText('Select 7') as HTMLInputElement).checked).toBe(true);
    fireEvent.click(screen.getByLabelText('Show selected elements only')); expect(screen.getByTestId('tbl-forces').getAttribute('data-rows')).toBe('1');
  });
  it('rotations are shown in rad without a unit conversion in every system; sorting by a header works', () => {
    act(() => useUi.setState({ tableTab: 'Displacements', unitSystem: 'N.mm' })); render(<App />);
    const t = screen.getByTestId('tbl-disp'); expect(within(t).getByText('UZ (mm)')).toBeTruthy(); expect(within(t).getByText('RY (rad)')).toBeTruthy();
    expect(within(t).getByText('-1066.667')).toBeTruthy();   // -1.066667 m = -1066.667 mm
    expect(within(t).getByText('0.4')).toBeTruthy();         // RY unchanged
    fireEvent.click(within(t).getByText('UZ (mm)')); expect(useUi.getState().tableUi.disp.sorting[0]).toEqual({ id: 'UZ', desc: false });
  });
  it('copy puts the selected rows, with unit headers, on the clipboard as tab-separated text', async () => {
    const write = vi.fn().mockResolvedValue(undefined); Object.defineProperty(navigator, 'clipboard', { value: { writeText: write }, configurable: true });
    act(() => useUi.setState({ unitSystem: 'Ton.m' })); render(<App />);
    fireEvent.click(screen.getByLabelText('Select 7')); fireEvent.click(screen.getByRole('button', { name: 'Copy' })); await vi.waitFor(() => expect(write).toHaveBeenCalled());
    const text = write.mock.calls[0][0] as string; const [h, row] = text.split('\n');
    expect(h.split('\t')).toContain('End-I My (tf·m)'); expect(h.split('\t')[0]).toBe('Element ID'); expect(Number(row.split('\t')[h.split('\t').indexOf('End-I My (tf·m)')])).toBeCloseTo(-4000 / 9.80665, 9);
  });
});
