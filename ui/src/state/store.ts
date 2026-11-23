import { create } from 'zustand';
import { parseModelView } from '../model/load';
import { noHost, type Command, type Finished, type SolverHost, type SourceFormat } from '../platform/SolverHost';
import type { ModelView } from '../model/types';
import type { ContourId } from '../model/contourField';
import { parseResults } from '../results/parse';
import type { ResultSet } from '../results/types';
import type { StationSet } from '../results/stations';
import { loadUnitSystem, saveUnitSystem, type UnitSystemId } from '../units/units';

export type Selection = { kind: 'node' | 'element'; id: number } | null;
export interface TableUi { sorting: { id: string; desc: boolean }[]; filters: Record<string, string>; hidden: string[]; widths: Record<string, number>; selected: (number | string)[]; idQuery: string; showFilters: boolean; endI: boolean; endJ: boolean; selectedOnly: boolean }
export const defaultTableUi = (): TableUi => ({ sorting: [{ id: 'id', desc: false }], filters: {}, hidden: [], widths: {}, selected: [], idQuery: '', showFilters: false, endI: true, endJ: true, selectedOnly: false });
export type CenterTab = '3d' | 'tables';
export type TableTab = 'Results' | 'Displacements' | 'Reactions' | 'Element Forces' | 'Diagrams' | 'Tables';
export type CameraPreset = 'iso' | 'front' | 'top' | 'side' | 'fit';
export interface ShowFlags { nodes: boolean; elements: boolean; supports: boolean; links: boolean; solid: boolean }

export type StageState = 'pending' | 'running' | 'done' | 'failed';
export type RunState = 'idle' | 'running' | 'done' | 'failed' | 'cancelled';
export interface RunInfo { state: RunState; command: Command | null; stages: Record<string, StageState>; log: string[]; seconds?: number; message?: string; resultPath?: string; summary?: string }
export const idleRun = (): RunInfo => ({ state: 'idle', command: null, stages: {}, log: [] });
export type Contour = ContourId;
// range*: legend range in DISPLAY units; used when rangeAuto is false
export interface ResultView { caseIndex: number; deformed: boolean; scale: number; contour: Contour; rangeAuto: boolean; rangeMin: number; rangeMax: number }
export const defaultResultView = (): ResultView => ({ caseIndex: 0, deformed: false, scale: 50, contour: 'none', rangeAuto: true, rangeMin: 0, rangeMax: 1 });
export type AnalysisKind = 'solve' | 'modal' | 'spectrum';

interface UiState {
  host: SolverHost; solver: { state: 'unknown' | 'connected' | 'missing'; version?: string; path?: string; error?: string };
  source: { path: string; format: SourceFormat } | null; analysis: AnalysisKind; modes: number; massType: 'consistent' | 'lumped'; run: RunInfo; activeRun: number;
  setHost: (h: SolverHost) => void; patchRun: (f: (r: RunInfo) => RunInfo) => void; finishRun: (f: Finished, resultText: string | null, readError: string | null) => void;
  set: (p: Partial<Pick<UiState, 'analysis' | 'modes' | 'massType' | 'solver' | 'source' | 'run' | 'activeRun'>>) => void;
  stations: StationSet | null; stationsNote: string | null;
  tableUi: Record<string, TableUi>; patchTableUi: (key: string, p: Partial<TableUi>) => void;   // per-table view state survives tab switches
  unitSystem: UnitSystemId; setUnitSystem: (u: UnitSystemId) => void;   // display only; results stay in solver units
  results: ResultSet | null; resultView: ResultView; setResultView: (p: Partial<ResultView>) => void;
  view: ModelView | null; fileName: string; loadError: string | null;
  selection: Selection; search: string; tableTab: TableTab; centerTab: CenterTab; setCenterTab: (t: CenterTab) => void;
  show: ShowFlags;
  camera: { preset: CameraPreset; n: number };   // n increments on every request so the same preset can be re-applied
  openRequest: number;                            // incremented to ask the shell to show the file dialog
  loadText: (text: string, name: string) => void;
  loadSample: () => Promise<void>;
  select: (s: Selection) => void; setSearch: (s: string) => void; setTableTab: (t: TableTab) => void;
  toggle: (k: keyof ShowFlags) => void; setCamera: (p: CameraPreset) => void; requestOpen: () => void;
}

/** One-line description of a CLI result document (schema names are those of the existing result schemas). */
export function describeResult(j: { schema?: string; load_cases?: unknown[]; combinations?: unknown[]; modes?: unknown[]; cases?: unknown[] }): string {
  if (j.schema === 'kopruq-results') return `${j.load_cases?.length ?? 0} load case(s), ${j.combinations?.length ?? 0} combination(s)`;
  if (j.schema === 'kopruq-modal-results') return `${j.modes?.length ?? 0} mode(s)`;
  if (j.schema === 'kopruq-response-spectrum-results') return `${j.cases?.length ?? 0} spectrum case(s)`;
  return String(j.schema ?? 'result');
}

export const SAMPLE_URL = 'samples/m100_view.json';

export const useUi = create<UiState>((set, get) => ({
  host: noHost, solver: { state: 'unknown' }, source: null, analysis: 'solve', modes: 20, massType: 'consistent', run: idleRun(), activeRun: 0,
  setHost: (host) => set({ host }), patchRun: (f) => set((s) => ({ run: f(s.run) })), set: (p) => set(p as Partial<UiState>),
  finishRun: (f, text, err) => set((s) => {
    const ok = !f.cancelled && f.exit === 0 && !err;
    let summary: string | undefined, failedMsg = err ?? undefined, results: ResultSet | null = null;
    if (ok && text) { try { const j = JSON.parse(text); summary = describeResult(j); if (j.status && j.status.success === false) { failedMsg = j.status.message ?? 'analysis reported failure'; } else results = parseResults(text); } catch (e) { failedMsg = `Result file is not valid JSON: ${(e as Error).message}`; } }
    const state: RunState = f.cancelled ? 'cancelled' : (ok && !failedMsg ? 'done' : 'failed');
    const stages = { ...s.run.stages }; for (const k of Object.keys(stages)) if (stages[k] === 'running') stages[k] = f.cancelled ? 'pending' : 'failed'; // a cancelled stage did not fail
    const good = state === 'done' && results !== null;
    return { stations: good ? null : s.stations, stationsNote: good ? (results!.kind === 'solve' ? 'Computing station results...' : null) : s.stationsNote, results: good ? results : s.results, resultView: good ? defaultResultView() : s.resultView, tableTab: good ? (results!.kind === 'modal' ? 'Tables' as TableTab : 'Displacements' as TableTab) : s.tableTab, run: { ...s.run, state, stages, seconds: f.seconds, resultPath: f.outputPath, summary, message: state === 'failed' ? (failedMsg ?? `The solver exited with code ${f.exit}`) : undefined } };
  }),
  stations: null, stationsNote: null,
  tableUi: {}, patchTableUi: (key, p) => set((s) => ({ tableUi: { ...s.tableUi, [key]: { ...(s.tableUi[key] ?? defaultTableUi()), ...p } } })),
  unitSystem: loadUnitSystem(), setUnitSystem: (unitSystem) => { saveUnitSystem(unitSystem); set({ unitSystem }); },
  results: null, resultView: defaultResultView(), setResultView: (p) => set((s) => ({ resultView: { ...s.resultView, ...p } })),
  view: null, fileName: '', loadError: null, selection: null, search: '', tableTab: 'Results', centerTab: '3d', setCenterTab: (centerTab) => set({ centerTab }),
  show: { nodes: true, elements: true, supports: true, links: true, solid: true },
  camera: { preset: 'iso', n: 0 }, openRequest: 0,
  loadText: (text, name) => {
    try { set({ view: parseModelView(text), tableUi: {}, results: null, stations: null, stationsNote: null, resultView: defaultResultView(), fileName: name, loadError: null, selection: null, search: '', camera: { preset: 'iso', n: get().camera.n + 1 } }); }
    catch (e) { set({ loadError: (e as Error).message }); } // keep the previously loaded model
  },
  loadSample: async () => {
    try {
      const r = await fetch(SAMPLE_URL); if (!r.ok) throw new Error(`HTTP ${r.status}`);
      get().loadText(await r.text(), 'sample: verification model 100 (inspect output)');
    } catch (e) { set({ loadError: `Could not load the sample model: ${(e as Error).message}` }); }
  },
  select: (selection) => set({ selection }), setSearch: (search) => set({ search }), setTableTab: (tableTab) => set({ tableTab }),
  toggle: (k) => set((s) => ({ show: { ...s.show, [k]: !s.show[k] } })),
  setCamera: (preset) => set((s) => ({ camera: { preset, n: s.camera.n + 1 } })),
  requestOpen: () => set((s) => ({ openRequest: s.openRequest + 1 })),
}));
