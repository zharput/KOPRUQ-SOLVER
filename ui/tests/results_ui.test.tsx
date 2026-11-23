import { readFileSync } from 'node:fs';
import { act, fireEvent, render, screen, within } from '@testing-library/react';
import { vi } from 'vitest';
import App from '../src/App';
import { defaultResultView, useUi } from '../src/state/store';
import { parseResults } from '../src/results/parse';

vi.mock('../src/components/fem-viewer/FEMViewer', () => ({ FEMViewer: () => <div data-testid="fem-viewer" /> }));
const view = readFileSync('tests/data/cantilever_view.json', 'utf-8'), res = readFileSync('tests/data/cantilever_results.json', 'utf-8');
beforeEach(() => { useUi.setState({ view: null, results: null, resultView: defaultResultView(), tableTab: 'Results', selection: null }); act(() => useUi.getState().loadText(view, 'cantilever')); });

describe('result tables', () => {
  it('without results the tables say so and Deformed is disabled', () => {
    render(<App />);
    expect(screen.getByTestId('results-empty')).toBeTruthy();
    expect((screen.getByRole('button', { name: 'Deformed' }) as HTMLButtonElement).disabled).toBe(true);
  });
  it('a successful run fills Displacements with the solver values; new model clears them', () => {
    render(<App />);
    act(() => useUi.getState().finishRun({ runId: 1, exit: 0, cancelled: false, seconds: 0.1, outputPath: 'x' } as never, res, null));
    expect(useUi.getState().tableTab).toBe('Displacements');
    const t = screen.getByTestId('tbl-disp'); expect(t.getAttribute('data-rows')).toBe('2');
    expect(within(t).getByText('-1.066667')).toBeTruthy(); // tip UZ = -P L^3/(3EI)
    expect((screen.getByRole('button', { name: 'Deformed' }) as HTMLButtonElement).disabled).toBe(false);
    fireEvent.click(screen.getByRole('button', { name: 'Deformed' })); expect(useUi.getState().resultView.deformed).toBe(true);
    act(() => useUi.getState().loadText(view, 'again')); expect(useUi.getState().results).toBeNull(); expect(useUi.getState().resultView.deformed).toBe(false);
  });
  it('element forces show 2 rows per element with the solver sign convention; reactions only at supports', () => {
    render(<App />); act(() => { useUi.setState({ results: parseResults(res) }); useUi.getState().setTableTab('Element Forces'); });
    const t = screen.getByTestId('tbl-forces'); expect(t.getAttribute('data-rows')).toBe('2');
    expect(within(t).getByText('-4000')).toBeTruthy();
    act(() => useUi.getState().setTableTab('Reactions')); expect(screen.getByTestId('tbl-reac').getAttribute('data-rows')).toBe('1');
  });
  it('an unparsable result file fails the run instead of showing nothing', () => {
    render(<App />); useUi.getState().patchRun((r) => ({ ...r, state: 'running' }));
    act(() => useUi.getState().finishRun({ runId: 1, exit: 0, cancelled: false, seconds: 0.1, outputPath: 'x' } as never, '{"schema":"other"}', null));
    expect(useUi.getState().run.state).toBe('failed'); expect(useUi.getState().results).toBeNull();
  });
});
