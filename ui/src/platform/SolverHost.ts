// The only seam between the reusable components and the outside world (solver process, files).
// `noHost` is used in a plain browser; `tauriHost` (TauriHost.ts) in the desktop app.
export type Command = 'inspect' | 'solve' | 'modal' | 'spectrum';
export type SourceFormat = 'native' | 'midas-nx-json';
export interface RunRequest { command: Command; modelPath: string; format: SourceFormat; numModes?: number; massType?: 'consistent' | 'lumped' }
export interface SolverInfo { path: string; version: string }
export interface Finished { runId: number; exit: number; cancelled: boolean; outputPath: string; seconds: number }
export interface HostHandlers {
  onLog(runId: number, line: string): void;
  onProgress(runId: number, event: Record<string, unknown>): void;
  onFinished(f: Finished): void;
}
export interface SolverHost {
  readonly kind: 'none' | 'tauri';
  info(): Promise<SolverInfo>;
  pickModel(): Promise<string | null>;
  readText(path: string): Promise<string>;
  run(req: RunRequest): Promise<number>;
  cancel(): Promise<boolean>;
  subscribe(h: HostHandlers): Promise<() => void>;
}
const unavailable = () => Promise.reject(new Error('Available in the desktop app only (this is a plain browser)'));
export const noHost: SolverHost = {
  kind: 'none', info: unavailable, pickModel: () => Promise.resolve(null), readText: unavailable, run: unavailable, cancel: () => Promise.resolve(false),
  subscribe: () => Promise.resolve(() => {}),
};
