// Subset of the `kopruq-model-view` v1 document (schemas/model_view.schema.json) that the viewer reads.
// The embedded `model` is a `kopruq-model` v1 object (schemas/model.schema.json).
export type Vec3 = [number, number, number];
export interface ModelNode { id: number; x: number; y: number; z: number }
export interface ModelBeam {
  id: number; nodes: [number, number]; material: number; section: number;
  reference_y?: Vec3; offset1?: Vec3; offset2?: Vec3;
  releases?: { start: boolean[]; end: boolean[] };
}
export interface ModelTruss { id: number; nodes: [number, number]; material: number; section: number }
export interface ModelSpring { id: number; nodes: [number, number] }
export interface ModelRigidLink { id: number; master: number; slave: number; dofs?: boolean[] }
export interface ModelSupport { node: number; restraints: boolean[] }
export interface ModelSection { id: number; area: number }
export interface ModelMaterial { id: number }
export interface LoadCaseInfo { name: string }
export interface KopruqModel {
  nodes: ModelNode[]; beams: ModelBeam[]; trusses?: ModelTruss[]; springs?: ModelSpring[]; rigid_links?: ModelRigidLink[];
  supports: ModelSupport[]; sections: ModelSection[]; materials: ModelMaterial[]; load_cases?: LoadCaseInfo[]; combinations?: { name: string }[];
}
export type Contour = [number, number][]; // [y, z] points in metres, section coordinates, centroid at origin
export interface SectionOutline { section: number; contours: Contour[] } // contours[0] = outer, others = holes
export interface ModelView {
  schema: 'kopruq-model-view'; schema_version: 1;
  metadata: { solver_version: string; source_format: 'native' | 'midas-nx-json'; model_sha256: string };
  model: KopruqModel; section_outlines: SectionOutline[];
  bounds?: { min: Vec3; max: Vec3 };
  import: { assumptions: string[]; rejected: string[]; counts?: Record<string, number> };
}
