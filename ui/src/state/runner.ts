import type { Finished, SourceFormat } from '../platform/SolverHost';
import { parseStations } from '../results/stations';
import { idleRun, useUi } from './store';

// Orchestration between the store and the SolverHost. Nothing here reports success on its own: a run is
// "done" only when the CLI exited with 0 AND its result file could be read and says status.success.
const finished = new Map<number, Finished>();
const waiters = new Map<number, (f: Finished) => void>();
const waitFinished = (id: number) => finished.has(id) ? Promise.resolve(finished.get(id)!) : new Promise<Finished>((res) => waiters.set(id, res));
/** Tauri rejects with the command's plain error string, not an Error object. */
const errText = (e: unknown) => (e instanceof Error ? e.message : String(e));
const baseName = (p: string) => p.split(/[\\/]/).pop() ?? p;
let unsubscribe: (() => void) | null = null;

/** Subscribes to host events once and asks the host which solver it found. Safe to call again (re-subscribes). */
export async function connectHost(): Promise<void> {
  const { host, set, patchRun } = useUi.getState();
  unsubscribe?.();
  unsubscribe = await host.subscribe({
    onLog: (_id, line) => patchRun((r) => (r.state === 'running' ? { ...r, log: [...r.log, line] } : r)),
    onProgress: (_id, ev) => patchRun((r) => {
      if (r.state !== 'running' || ev.event !== 'stage' || typeof ev.name !== 'string') return r;
      return { ...r, stages: { ...r.stages, [ev.name]: ev.state === 'start' ? 'running' : 'done' } };
    }),
    onFinished: (f) => { const w = waiters.get(f.runId); if (w) { waiters.delete(f.runId); w(f); } else finished.set(f.runId, f); },
  });
  if (host.kind === 'none') { set({ solver: { state: 'missing', error: 'Desktop app required' } }); return; }
  try { const i = await host.info(); set({ solver: { state: 'connected', version: i.version, path: i.path } }); }
  catch (e) { set({ solver: { state: 'missing', error: errText(e) } }); }
}

/** Opens a model chosen in the desktop app: MIDAS/native model -> `inspect` -> viewer; a model-view JSON loads directly. */
export async function openModelPath(path: string): Promise<void> {
  const { host, loadText } = useUi.getState();
  try {
    const text = await host.readText(path);
    let doc: Record<string, unknown>; try { doc = JSON.parse(text); } catch (e) { throw new Error(`Not valid JSON: ${(e as Error).message}`); }
    if (doc.schema === 'kopruq-model-view') { loadText(text, baseName(path)); useUi.getState().set({ source: null, run: idleRun() }); return; }
    const format: SourceFormat | null = 'DBG' in doc ? 'midas-nx-json' : doc.schema === 'kopruq-model' ? 'native' : null;
    if (!format) throw new Error('Unrecognised file: expected a MIDAS NX JSON export, a kopruq-model or a kopruq-model-view document');
    const id = await host.run({ command: 'inspect', modelPath: path, format });
    const f = await waitFinished(id);
    if (f.exit !== 0) throw new Error(`The solver could not read the model (inspect exit code ${f.exit})`);
    loadText(await host.readText(f.outputPath), baseName(path));
    if (!useUi.getState().loadError) useUi.getState().set({ source: { path, format }, run: idleRun() });
  } catch (e) { useUi.setState({ loadError: errText(e) }); }
}

export async function pickAndOpen(): Promise<void> {
  const p = await useUi.getState().host.pickModel();
  if (p) await openModelPath(p);
}

export async function startAnalysis(): Promise<void> {
  const s = useUi.getState();
  if (!s.source || s.run.state === 'running') return;
  const command = s.analysis;
  s.set({ run: { ...idleRun(), state: 'running', command, stages: { read: 'pending', solve: 'pending', write: 'pending' } } });
  try {
    const id = await s.host.run({ command, modelPath: s.source.path, format: s.source.format, numModes: command === 'modal' ? s.modes : undefined, massType: command === 'solve' ? undefined : s.massType });
    const f = await waitFinished(id);
    let text: string | null = null, err: string | null = null;
    if (!f.cancelled && f.exit === 0) { try { text = await s.host.readText(f.outputPath); } catch (e) { err = `The result file could not be read: ${errText(e)}`; } }
    useUi.getState().finishRun(f, text, err);
    if (command === 'solve') await runStations(s.source.path, s.source.format);
  } catch (e) { useUi.getState().set({ run: { ...useUi.getState().run, state: 'failed', message: errText(e) } }); }
}

export const cancelAnalysis = () => useUi.getState().host.cancel();

/** After a successful static run: `stations` (same model, own CLI process) for the force diagrams. Failure only disables the diagrams. */
async function runStations(path: string, format: SourceFormat): Promise<void> {
  const st = useUi.getState(); if (st.run.state !== 'done') return;
  try {
    const id = await st.host.run({ command: 'stations', modelPath: path, format });
    const f = await waitFinished(id);
    if (f.cancelled) { useUi.setState({ stationsNote: 'Station results were cancelled.' }); return; }
    if (f.exit !== 0) throw new Error(`the solver exited with code ${f.exit}`);
    useUi.setState({ stations: parseStations(await st.host.readText(f.outputPath)), stationsNote: null });
  } catch (e) { useUi.setState({ stations: null, stationsNote: `Station results could not be computed: ${errText(e)}` }); }
}
