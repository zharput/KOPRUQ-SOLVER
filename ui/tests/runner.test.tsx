import { readFileSync } from 'node:fs';
import { act, render, screen } from '@testing-library/react';
import { vi } from 'vitest';
import App from '../src/App';
import type { Finished, HostHandlers, RunRequest, SolverHost } from '../src/platform/SolverHost';
import { cancelAnalysis, connectHost, openModelPath, startAnalysis } from '../src/state/runner';
import { idleRun, useUi } from '../src/state/store';

vi.mock('../src/components/fem-viewer/FEMViewer', () => ({ FEMViewer: () => <div data-testid="fem-viewer" /> }));
const view = readFileSync('tests/data/m100_view.json', 'utf-8');
const midas = '{"DBG":{"NODE":{}}}';

// A scripted host: every run() records the request, then plays the events the test queued for it.
function fakeHost(files: Record<string, string>, script: (req: RunRequest, h: HostHandlers, id: number) => void | Promise<void>) {
  let handlers: HostHandlers | null = null, id = 0; const requests: RunRequest[] = [];
  const host: SolverHost = {
    kind: 'tauri', info: async () => ({ path: 'C:/x/cli.exe', version: 'KOPRUQ-SOLVER 9.9' }), pickModel: async () => null,
    readText: async (p) => { if (p in files) return files[p]; throw new Error('no such file ' + p); },
    run: async (req) => { requests.push(req); const my = ++id; setTimeout(() => void script(req, handlers!, my), 0); return my; },
    cancel: async () => { handlers!.onFinished({ runId: id, exit: -1, cancelled: true, outputPath: '', seconds: 0.1 }); return true; },
    subscribe: async (h) => { handlers = h; return () => {}; },
  };
  return { host, requests };
}
const finish = (h: HostHandlers, runId: number, exit: number, out: string, seconds = 0.5): void => h.onFinished({ runId, exit, cancelled: false, outputPath: out, seconds } satisfies Finished);

beforeEach(() => useUi.setState({ view: null, fileName: '', loadError: null, source: null, run: idleRun(), analysis: 'solve', selection: null }));

async function opened(files: Record<string, string>, script: Parameters<typeof fakeHost>[1]) {
  const f = fakeHost({ 'm.json': midas, 'view.json': view, ...files }, async (req, h, id) => { if (req.command === 'inspect') finish(h, id, 0, 'view.json'); else await script(req, h, id); });
  useUi.getState().setHost(f.host); await act(async () => { await connectHost(); });
  await act(async () => { await openModelPath('m.json'); });
  return f;
}

describe('desktop host integration (scripted fake host, no real solver)', () => {
  it('opening a MIDAS file runs inspect, loads the view and remembers the source; solver version is shown', async () => {
    render(<App />);
    const f = await opened({}, () => {});
    expect(f.requests[0]).toMatchObject({ command: 'inspect', modelPath: 'm.json', format: 'midas-nx-json' });
    expect(useUi.getState().view?.model.nodes.length).toBe(106);
    expect(useUi.getState().source).toEqual({ path: 'm.json', format: 'midas-nx-json' });
    expect(screen.getByTestId('solver-connection').textContent).toContain('KOPRUQ-SOLVER 9.9');
  });
  it('a successful run shows stage progress and "Completed" only after the result file says success', async () => {
    render(<App />);
    const ok = JSON.stringify({ schema: 'kopruq-results', status: { success: true }, load_cases: [{}, {}], combinations: [{}] });
    const f = await opened({ 'r.json': ok }, (_r, h, id) => { h.onLog(id, 'import: 106 nodes'); h.onProgress(id, { event: 'stage', name: 'read', state: 'done' }); h.onProgress(id, { event: 'stage', name: 'solve', state: 'start' }); setTimeout(() => finish(h, id, 0, 'r.json'), 5); });
    await act(async () => { await startAnalysis(); });
    expect(f.requests[1]).toMatchObject({ command: 'solve', modelPath: 'm.json' });
    expect(useUi.getState().run.state).toBe('done');
    expect(useUi.getState().run.log).toContain('import: 106 nodes');
    expect(screen.getByText(/Completed/).textContent).toContain('2 load case(s), 1 combination(s)');
  });
  it('exit 0 with a result that reports failure is FAILED, never Completed', async () => {
    render(<App />);
    const bad = JSON.stringify({ schema: 'kopruq-results', status: { success: false, message: 'singular system' }, load_cases: [] });
    await opened({ 'r.json': bad }, (_r, h, id) => finish(h, id, 0, 'r.json'));
    await act(async () => { await startAnalysis(); });
    expect(useUi.getState().run.state).toBe('failed'); expect(screen.queryByText(/Completed/)).toBeNull();
    expect(screen.getByRole('alert').textContent).toContain('singular system');
  });
  it('a non-zero exit, an unreadable result and a cancel are each reported as such', async () => {
    render(<App />);
    await opened({}, (_r, h, id) => finish(h, id, 1, 'missing.json'));
    await act(async () => { await startAnalysis(); });
    expect(useUi.getState().run.state).toBe('failed'); expect(useUi.getState().run.message).toMatch(/code 1/);
    await opened({}, (_r, h, id) => finish(h, id, 0, 'missing.json'));
    await act(async () => { await startAnalysis(); });
    expect(useUi.getState().run.message).toMatch(/could not be read/);
    await opened({}, () => {}); // never finishes by itself
    await act(async () => { const p = startAnalysis(); await new Promise((r) => setTimeout(r, 20)); await cancelAnalysis(); await p; });
    expect(useUi.getState().run.state).toBe('cancelled'); expect(screen.queryByText(/Completed/)).toBeNull();
    expect(Object.values(useUi.getState().run.stages)).not.toContain('failed');
  });
  it('modal and spectrum pass their options; solve passes no mass type', async () => {
    render(<App />);
    const ok = JSON.stringify({ schema: 'kopruq-modal-results', status: { success: true }, modes: [{}, {}, {}] });
    const f = await opened({ 'r.json': ok }, (_r, h, id) => finish(h, id, 0, 'r.json'));
    useUi.setState({ analysis: 'modal', modes: 7, massType: 'lumped' }); await act(async () => { await startAnalysis(); });
    expect(f.requests.at(-1)).toMatchObject({ command: 'modal', numModes: 7, massType: 'lumped' });
    useUi.setState({ analysis: 'solve' }); await act(async () => { await startAnalysis(); });
    expect(f.requests.at(-1)?.massType).toBeUndefined(); expect(f.requests.at(-1)?.numModes).toBeUndefined();
  });
  it('rejects an unrecognised file and a failing inspect with readable errors', async () => {
    render(<App />);
    const f = fakeHost({ 'x.json': '{"hello":1}', 'm.json': midas }, (_r, h, id) => finish(h, id, 3, ''));
    useUi.getState().setHost(f.host); await act(async () => { await connectHost(); });
    await act(async () => { await openModelPath('x.json'); });
    expect(screen.getByRole('alert').textContent).toContain('Unrecognised file');
    await act(async () => { await openModelPath('m.json'); });
    expect(screen.getByRole('alert').textContent).toContain('inspect exit code 3');
    expect(useUi.getState().view).toBeNull();
  });
  it('shows the message when the host rejects with a plain string (as Tauri does)', async () => {
    render(<App />);
    const f = fakeHost({}, () => {}); f.host.readText = () => Promise.reject('model.json: The system cannot find the file specified');
    useUi.getState().setHost(f.host); await act(async () => { await connectHost(); });
    await act(async () => { await openModelPath('model.json'); });
    expect(screen.getByRole('alert').textContent).toContain('cannot find the file');
  });
  it('in a plain browser the run button stays disabled and says why', async () => {
    useUi.getState().setHost((await import('../src/platform/SolverHost')).noHost); render(<App />);
    expect((screen.getByRole('button', { name: 'Run Analysis' }) as HTMLButtonElement).disabled).toBe(true);
    expect(screen.getByText('Desktop app required', { selector: 'p' })).toBeTruthy();
  });
});
