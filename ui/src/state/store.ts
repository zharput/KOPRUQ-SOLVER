import { create } from 'zustand';

export type Selection = { kind: 'node' | 'element'; id: number } | null;
export type BottomTab = 'Results' | 'Displacements' | 'Reactions' | 'Element Forces' | 'Diagrams' | 'Tables' | 'Verification';

interface UiState {
  selection: Selection;
  search: string;
  bottomTab: BottomTab;
  view: { nodes: boolean; elements: boolean; supports: boolean; loads: boolean; localAxes: boolean; solid: boolean };
  select: (s: Selection) => void;
  setSearch: (s: string) => void;
  setBottomTab: (t: BottomTab) => void;
  toggleView: (k: keyof UiState['view']) => void;
}

export const useUi = create<UiState>((set) => ({
  selection: null,
  search: '',
  bottomTab: 'Results',
  view: { nodes: true, elements: true, supports: true, loads: false, localAxes: false, solid: false },
  select: (selection) => set({ selection }),
  setSearch: (search) => set({ search }),
  setBottomTab: (bottomTab) => set({ bottomTab }),
  toggleView: (k) => set((s) => ({ view: { ...s.view, [k]: !s.view[k] } })),
}));
