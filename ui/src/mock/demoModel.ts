// MOCK DATA -- UI development only. This is NOT a real KOPRUQ-SOLVER model or result, it is never
// sent to the solver and every place that shows it carries the "MOCK" marker. Replaced in Phase 3/4
// by models read from the existing KOPRUQ JSON schemas.
export interface MockNode { id: number; x: number; y: number; z: number }
export interface MockElement { id: number; type: 'Beam' | 'Truss'; n1: number; n2: number; section: string; material: string; beta: number }
export const MOCK_LABEL = 'MOCK';
export const mockNodes: MockNode[] = Array.from({ length: 11 }, (_, i) => ({ id: i + 1, x: i * 10, y: 0, z: 0 }));
export const mockElements: MockElement[] = Array.from({ length: 10 }, (_, i) => ({
  id: i + 1, type: 'Beam', n1: i + 1, n2: i + 2, section: i < 5 ? 'GIRDER-1' : 'GIRDER-2', material: 'C35/45', beta: 0,
}));
export const mockMaterials = ['C35/45'];
export const mockSections = ['GIRDER-1', 'GIRDER-2'];
export const mockLoadCases = ['SW', 'SDL'];
