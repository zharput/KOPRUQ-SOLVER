import { readFileSync } from 'node:fs';
import { act, render, screen } from '@testing-library/react';
import { vi } from 'vitest';
import App from '../src/App';
import { diagramPoints, maxClosure, parseStations } from '../src/results/stations';
import { parseResults } from '../src/results/parse';
import { defaultResultView, useUi } from '../src/state/store';

vi.mock('../src/components/fem-viewer/FEMViewer', () => ({ FEMViewer: () => <div data-testid="fem-viewer" /> }));
const charts: unknown[] = [];
vi.mock('echarts/core', () => ({ use: () => {}, init: () => ({ setOption: (o: unknown) => charts.push(o), resize: () => {}, dispose: () => {} }) }));
vi.mock('echarts/charts', () => ({ LineChart: {}, BarChart: {} }));
vi.mock('echarts/components', () => ({ GridComponent: {}, MarkLineComponent: {}, TooltipComponent: {} }));
vi.mock('echarts/renderers', () => ({ CanvasRenderer: {} }));
const view = readFileSync('tests/data/cantilever_view.json', 'utf-8'), res = readFileSync('tests/data/cantilever_results.json', 'utf-8'), st = readFileSync('tests/data/cantilever_stations.json', 'utf-8');
globalThis.ResizeObserver ??= class { observe() {} disconnect() {} unobserve() {} } as never;

describe('station results', () => {
  it('parses the CLI document; the cantilever moment My = P(L - x) (closed form) and shear Vz = -P', () => {
    const s = parseStations(st), el = s.cases[0].elements.get(7)!;
    expect(el.length).toBe(4);
    for (const r of el.rows) { expect(r.f[4]).toBeCloseTo(1000 * (4 - r.x), 9); expect(r.f[2]).toBeCloseTo(-1000, 9); }
    expect(diagramPoints(el.rows, 4)[0]).toEqual([0, 4000]); expect(maxClosure(s.cases[0]).force).toBeLessThan(1e-9);
  });
  it('rejects other documents and lists failed cases', () => {
    expect(() => parseStations('{"schema":"kopruq-results"}')).toThrow(/Not a station/);
    const s = parseStations(JSON.stringify({ schema: 'kopruq-element-stations', load_cases: [{ name: 'x', status: { success: false, message: 'singular' } }], combinations: [] }));
    expect(s.cases.length).toBe(0); expect(s.failed[0].name).toBe('x');
  });
  it('Diagrams tab draws six charts for the selected element; honest messages otherwise', () => {
    useUi.setState({ view: null, results: null, stations: null, stationsNote: null, resultView: defaultResultView(), selection: null, tableTab: 'Diagrams' });
    act(() => useUi.getState().loadText(view, 'c'));
    render(<App />);
    expect(screen.getByTestId('diagrams-na').textContent).toMatch(/no results/);
    act(() => useUi.setState({ results: parseResults(res), stationsNote: 'Station results could not be computed: boom' }));
    expect(screen.getByTestId('diagrams-na').textContent).toMatch(/boom/);
    charts.length = 0; act(() => useUi.setState({ stations: parseStations(st), stationsNote: null, selection: { kind: 'element', id: 7 } }));
    expect(screen.getByTestId('diagrams')).toBeTruthy(); expect(charts.length).toBe(6);
    act(() => useUi.setState({ results: { ...parseResults(res), kind: 'modal' } }));
    expect(screen.getByTestId('diagrams-na').textContent).toMatch(/Linear Static/);
  });
});
