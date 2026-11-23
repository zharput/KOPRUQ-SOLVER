import { create } from 'zustand';
import { parseModelView } from '../model/load';
import type { ModelView } from '../model/types';

export type Selection = { kind: 'node' | 'element'; id: number } | null;
export type BottomTab = 'Results' | 'Displacements' | 'Reactions' | 'Element Forces' | 'Diagrams' | 'Tables' | 'Verification';
export type CameraPreset = 'iso' | 'front' | 'top' | 'side' | 'fit';
export interface ShowFlags { nodes: boolean; elements: boolean; supports: boolean; links: boolean; solid: boolean }

interface UiState {
  view: ModelView | null; fileName: string; loadError: string | null;
  selection: Selection; search: string; bottomTab: BottomTab;
  show: ShowFlags;
  camera: { preset: CameraPreset; n: number };   // n increments on every request so the same preset can be re-applied
  openRequest: number;                            // incremented to ask the shell to show the file dialog
  loadText: (text: string, name: string) => void;
  loadSample: () => Promise<void>;
  select: (s: Selection) => void; setSearch: (s: string) => void; setBottomTab: (t: BottomTab) => void;
  toggle: (k: keyof ShowFlags) => void; setCamera: (p: CameraPreset) => void; requestOpen: () => void;
}

export const SAMPLE_URL = 'samples/m100_view.json';

export const useUi = create<UiState>((set, get) => ({
  view: null, fileName: '', loadError: null, selection: null, search: '', bottomTab: 'Results',
  show: { nodes: true, elements: true, supports: true, links: true, solid: true },
  camera: { preset: 'iso', n: 0 }, openRequest: 0,
  loadText: (text, name) => {
    try { set({ view: parseModelView(text), fileName: name, loadError: null, selection: null, search: '', camera: { preset: 'iso', n: get().camera.n + 1 } }); }
    catch (e) { set({ loadError: (e as Error).message }); } // keep the previously loaded model
  },
  loadSample: async () => {
    try {
      const r = await fetch(SAMPLE_URL); if (!r.ok) throw new Error(`HTTP ${r.status}`);
      get().loadText(await r.text(), 'sample: verification model 100 (inspect output)');
    } catch (e) { set({ loadError: `Could not load the sample model: ${(e as Error).message}` }); }
  },
  select: (selection) => set({ selection }), setSearch: (search) => set({ search }), setBottomTab: (bottomTab) => set({ bottomTab }),
  toggle: (k) => set((s) => ({ show: { ...s.show, [k]: !s.show[k] } })),
  setCamera: (preset) => set((s) => ({ camera: { preset, n: s.camera.n + 1 } })),
  requestOpen: () => set((s) => ({ openRequest: s.openRequest + 1 })),
}));
