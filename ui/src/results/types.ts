// Result sets read from the CLI's own JSON documents (kopruq-results, kopruq-modal-results,
// kopruq-response-spectrum-results). Values are in the solver's units (m, kN, kNm, rad) and the
// solver's local-axis convention; nothing is converted here.
export interface NodeValues { id: number; disp: number[]; reac?: number[]; reacLocal?: number[]; restrained?: boolean[] }
export interface ElemValues { id: number; local: number[] }   // 12 local end forces: node1 N Vy Vz T My Mz, node2 N Vy Vz T My Mz
export type CaseKind = 'load case' | 'combination' | 'spectrum' | 'mode';
export interface ResultCase { name: string; kind: CaseKind; nodes: NodeValues[]; elements: ElemValues[]; springs?: ElemValues[]; note?: string; peak?: boolean;
  modeRows?: ModeRow[]; totalMass?: number; massRatio?: number }   // spectrum cases: the per-mode participation summary of that case
export interface ModeRow { index: number; frequency: number; omega: number; period: number; effectiveMass?: number; participation?: number }
export interface ResultSet { kind: 'solve' | 'modal' | 'spectrum'; cases: ResultCase[]; modes: ModeRow[]; totalMass?: number; solverVersion?: string; failed: { name: string; message: string }[] }
