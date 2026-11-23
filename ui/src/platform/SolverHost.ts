// The only seam between the reusable components and the outside world (solver, files). Phase 2 has
// no implementation: nothing here is wired to the C++ solver yet. TauriHost arrives with Phase 4.
export interface SolverHost {
  readonly kind: 'none' | 'tauri';
  readonly canRunAnalysis: boolean;
}
export const noHost: SolverHost = { kind: 'none', canRunAnalysis: false };
