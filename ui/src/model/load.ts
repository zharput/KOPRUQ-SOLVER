import type { ModelView } from './types';

/** Parses and sanity-checks a kopruq-model-view v1 document; throws an Error with a readable message. */
export function parseModelView(text: string): ModelView {
  let doc: unknown;
  try { doc = JSON.parse(text); } catch (e) { throw new Error(`Not valid JSON: ${(e as Error).message}`); }
  const d = doc as Partial<ModelView> | null;
  if (!d || typeof d !== 'object') throw new Error('The file is not a JSON object');
  if (d.schema !== 'kopruq-model-view')
    throw new Error(`Not a kopruq-model-view document (schema = ${JSON.stringify((d as { schema?: unknown }).schema)}). Create one with: kopruq_solver_cli inspect <model> -o view.json`);
  if (d.schema_version !== 1) throw new Error(`Unsupported kopruq-model-view version ${String(d.schema_version)} (this viewer reads version 1)`);
  const m = d.model;
  if (!m || !Array.isArray(m.nodes) || !Array.isArray(m.beams) || !Array.isArray(m.supports)) throw new Error('The document has no valid "model" (nodes/beams/supports)');
  if (!Array.isArray(d.section_outlines)) throw new Error('The document has no "section_outlines" array');
  const ids = new Set(m.nodes.map((n) => n.id));
  for (const b of m.beams) if (!ids.has(b.nodes[0]) || !ids.has(b.nodes[1])) throw new Error(`Element ${b.id} references an unknown node`);
  return d as ModelView;
}
