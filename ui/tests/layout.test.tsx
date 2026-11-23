import { readFileSync } from 'node:fs';
import { act, fireEvent, render, screen } from '@testing-library/react';
import { vi } from 'vitest';
import App from '../src/App';
import { defaultResultView, useUi } from '../src/state/store';

vi.mock('../src/components/fem-viewer/FEMViewer', () => ({ FEMViewer: () => <div data-testid="fem-viewer" /> }));
const view = readFileSync('tests/data/cantilever_view.json', 'utf-8'), res = readFileSync('tests/data/cantilever_results.json', 'utf-8');
beforeEach(() => { useUi.setState({ view: null, results: null, stations: null, resultView: defaultResultView(), selection: null, tableTab: 'Results', centerTab: '3d' }); act(() => useUi.getState().loadText(view, 'c')); });

describe('GUI V2 layout', () => {
  it('has the 3D Model and Tables workspace tabs and no bottom results panel', () => {
    render(<App />);
    const tabs = screen.getByRole('tablist', { name: 'Workspace tabs' }).querySelectorAll('[role=tab]');
    expect([...tabs].map((t) => t.textContent)).toEqual(['3D Model', 'Tables']);
    expect(screen.queryByLabelText('Results panel')).toBeNull();
  });
  it('switching tabs keeps both views mounted and loses neither results nor selection', () => {
    render(<App />);
    act(() => { useUi.getState().finishRun({ runId: 1, exit: 0, cancelled: false, seconds: 0.1, outputPath: 'x' } as never, res, null); useUi.getState().select({ kind: 'element', id: 7 }); });
    fireEvent.click(screen.getByRole('tab', { name: 'Tables' }));
    expect(screen.getByTestId('tables-workspace')).toBeTruthy(); expect(screen.getByTestId('fem-viewer')).toBeTruthy(); // 3D scene stays mounted (hidden)
    expect(screen.getByTestId('tbl-disp')).toBeTruthy();
    fireEvent.click(screen.getByRole('tab', { name: '3D Model' }));
    expect(useUi.getState().results).not.toBeNull(); expect(useUi.getState().selection).toEqual({ kind: 'element', id: 7 });
  });
  it('lists every result category; unsupported ones are disabled with a reason', () => {
    render(<App />); fireEvent.click(screen.getByRole('tab', { name: 'Tables' }));
    for (const n of ['Element Forces', 'Displacements', 'Reactions', 'Diagrams', 'Modal / Spectrum', 'Summary']) expect((screen.getByRole('tab', { name: n }) as HTMLButtonElement).disabled).toBe(false);
    const st = screen.getByRole('tab', { name: 'Element Stresses' }) as HTMLButtonElement; expect(st.disabled).toBe(true); expect(st.title).toMatch(/no stresses/);
  });
  it('the solver log lives in the Analysis panel', () => {
    render(<App />); expect(screen.getByText(/Solver log/)).toBeTruthy();
  });
});
