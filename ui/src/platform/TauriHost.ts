import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import { open } from '@tauri-apps/plugin-dialog';
import type { Finished, HostHandlers, RunRequest, SolverHost, SolverInfo } from './SolverHost';

export const isTauri = () => typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;

export const tauriHost: SolverHost = {
  kind: 'tauri',
  info: () => invoke<SolverInfo>('solver_info'),
  pickModel: async () => {
    const p = await open({ multiple: false, filters: [{ name: 'Model JSON', extensions: ['json'] }] });
    return typeof p === 'string' ? p : null;
  },
  readText: (path) => invoke<string>('read_text_file', { path }),
  run: (req: RunRequest) => invoke<number>('run_solver', { req: { command: req.command, model_path: req.modelPath, format: req.format, num_modes: req.numModes ?? null, mass_type: req.massType ?? null } }),
  cancel: () => invoke<boolean>('cancel_solver'),
  subscribe: async (h: HostHandlers) => {
    const un = await Promise.all([
      listen<{ run_id: number; line: string }>('solver-log', (e) => h.onLog(e.payload.run_id, e.payload.line)),
      listen<{ run_id: number; event: Record<string, unknown> }>('solver-progress', (e) => h.onProgress(e.payload.run_id, e.payload.event)),
      listen<{ run_id: number; exit: number; cancelled: boolean; output_path: string; seconds: number }>('solver-finished', (e) =>
        h.onFinished({ runId: e.payload.run_id, exit: e.payload.exit, cancelled: e.payload.cancelled, outputPath: e.payload.output_path, seconds: e.payload.seconds } satisfies Finished)),
    ]);
    return () => un.forEach((f) => f());
  },
};
