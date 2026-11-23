// Pure geometry for the viewer (no WebGL): member axes exactly as the solver's Beam3D::axes, section
// extrusion, end caps, support glyphs. Everything is testable without a browser.
import type { Contour, ModelView, Vec3 } from './types';

const sub = (a: Vec3, b: Vec3): Vec3 => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const add = (a: Vec3, b: Vec3): Vec3 => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
const dot = (a: Vec3, b: Vec3) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a: Vec3, b: Vec3): Vec3 => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = (a: Vec3) => Math.hypot(a[0], a[1], a[2]);
const scale = (a: Vec3, s: number): Vec3 => [a[0] * s, a[1] * s, a[2] * s];

export interface MemberAxes { x: Vec3; y: Vec3; z: Vec3; length: number }

/** Beam3D::axes: x along the member, y = reference_y made orthogonal to x, z = x cross y. null when degenerate. */
export function memberAxes(p1: Vec3, p2: Vec3, referenceY: Vec3): MemberAxes | null {
  const d = sub(p2, p1); const length = norm(d);
  if (!(length > 0)) return null;
  const x = scale(d, 1 / length);
  const y0 = sub(referenceY, scale(x, dot(referenceY, x)));
  if (!(norm(y0) > 64 * Number.EPSILON * norm(referenceY))) return null;
  const y = scale(y0, 1 / norm(y0));
  const z0 = cross(x, y); const z = scale(z0, 1 / norm(z0));
  return { x, y, z, length };
}

/** Signed polygon area (counter-clockwise positive). */
export const polygonArea = (c: Contour) => c.reduce((s, p, i) => { const q = c[(i + 1) % c.length]; return s + (p[0] * q[1] - q[0] * p[1]) / 2; }, 0);

/** Ear-clipping triangulation of a simple polygon without holes; returns index triples (counter-clockwise input). */
export function triangulate(c: Contour): [number, number, number][] {
  const n = c.length; const idx = Array.from({ length: n }, (_, i) => i);
  if (polygonArea(c) < 0) idx.reverse();
  const out: [number, number, number][] = [];
  const cr = (a: number, b: number, k: number) => (c[b][0] - c[a][0]) * (c[k][1] - c[a][1]) - (c[b][1] - c[a][1]) * (c[k][0] - c[a][0]);
  const inside = (a: number, b: number, k: number, p: number) => cr(a, b, p) >= 0 && cr(b, k, p) >= 0 && cr(k, a, p) >= 0;
  let guard = 0;
  while (idx.length > 3 && guard++ < n * n) {
    let clipped = false;
    for (let i = 0; i < idx.length; i++) {
      const a = idx[(i + idx.length - 1) % idx.length], b = idx[i], k = idx[(i + 1) % idx.length];
      if (cr(a, b, k) <= 1e-14) continue; // reflex or degenerate
      if (idx.some((p) => p !== a && p !== b && p !== k && inside(a, b, k, p))) continue;
      out.push([a, b, k]); idx.splice(i, 1); clipped = true; break;
    }
    if (!clipped) break; // degenerate input: stop rather than loop
  }
  if (idx.length === 3) out.push([idx[0], idx[1], idx[2]]);
  return out;
}

export interface SolidMesh {
  positions: Float32Array;  // xyz per vertex (faces do not share vertices -> flat shading, per-face colour)
  colors: Uint8Array;       // rgb per vertex
  triangles: Uint32Array;   // vertex indices, 3 per triangle
  triElement: Int32Array;   // element id of each triangle
  skipped: number[];        // element ids that could not be extruded (no outline for the section, degenerate axes)
}

export const SECTION_PALETTE: [number, number, number][] = [[96, 165, 250], [183, 189, 199], [135, 209, 148], [250, 189, 102], [204, 153, 242], [242, 140, 153]];

/** Extrudes every beam with an outline along its own axes between node + offset1 and node + offset2. */
export function buildSolids(view: ModelView): SolidMesh {
  const nodes = new Map(view.model.nodes.map((n) => [n.id, [n.x, n.y, n.z] as Vec3]));
  const outlines = new Map(view.section_outlines.map((o) => [o.section, o.contours]));
  const colorOf = new Map<number, [number, number, number]>();
  [...outlines.keys()].sort((a, b) => a - b).forEach((s, i) => colorOf.set(s, SECTION_PALETTE[i % SECTION_PALETTE.length]));
  const pos: number[] = [], col: number[] = [], tri: number[] = [], te: number[] = []; const skipped: number[] = [];
  let nv = 0;
  const vert = (p: Vec3, c: [number, number, number]) => { pos.push(p[0], p[1], p[2]); col.push(c[0], c[1], c[2]); return nv++; };
  for (const b of view.model.beams) {
    const contours = outlines.get(b.section);
    const p1 = nodes.get(b.nodes[0])!, p2 = nodes.get(b.nodes[1])!;
    const A = add(p1, b.offset1 ?? [0, 0, 0]), B = add(p2, b.offset2 ?? [0, 0, 0]);
    const ax = contours ? memberAxes(A, B, b.reference_y ?? [0, 1, 0]) : null;
    if (!contours || !ax) { skipped.push(b.id); continue; }
    const c = colorOf.get(b.section)!;
    const at = (o: Vec3, q: [number, number]): Vec3 => add(o, add(scale(ax.y, q[0]), scale(ax.z, q[1])));
    for (const ring of contours) {
      for (let i = 0; i < ring.length; i++) {
        const q0 = ring[i], q1 = ring[(i + 1) % ring.length];
        const a = vert(at(A, q0), c), bb = vert(at(A, q1), c), cc = vert(at(B, q1), c), d = vert(at(B, q0), c);
        tri.push(a, bb, cc, a, cc, d); te.push(b.id, b.id);
      }
    }
    if (contours.length === 1) { // solid section: close both ends (sections with a cell stay open, the cell is visible)
      const t = triangulate(contours[0]);
      for (const end of [A, B]) {
        const base = nv; contours[0].forEach((q) => vert(at(end, q), c));
        for (const [i, j, k] of t) { tri.push(base + i, base + j, base + k); te.push(b.id); }
      }
    }
  }
  return { positions: new Float32Array(pos), colors: new Uint8Array(col), triangles: new Uint32Array(tri), triElement: new Int32Array(te), skipped };
}

/** Octahedron-free support glyph: a cube for a fully fixed node, a pyramid for any other restraint pattern. */
export function supportGlyphs(view: ModelView, size: number): { positions: Float32Array; triangles: Uint32Array; supportNode: Int32Array } {
  const nodes = new Map(view.model.nodes.map((n) => [n.id, n]));
  const pos: number[] = [], tri: number[] = [], who: number[] = []; let nv = 0;
  const v = (x: number, y: number, z: number) => { pos.push(x, y, z); return nv++; };
  for (const s of view.model.supports) {
    const n = nodes.get(s.node); if (!n || !s.restraints.some(Boolean)) continue;
    const h = size / 2, fixed = s.restraints.every(Boolean);
    if (fixed) {
      const c = [[-h,-h,-h],[h,-h,-h],[h,h,-h],[-h,h,-h],[-h,-h,h],[h,-h,h],[h,h,h],[-h,h,h]].map(([a,b,d]) => v(n.x + a, n.y + b, n.z + d));
      for (const f of [[0,2,1],[0,3,2],[4,5,6],[4,6,7],[0,1,5],[0,5,4],[1,2,6],[1,6,5],[2,3,7],[2,7,6],[3,0,4],[3,4,7]]) { tri.push(c[f[0]], c[f[1]], c[f[2]]); who.push(s.node); }
    } else {
      const apex = v(n.x, n.y, n.z), b = [[-h,-h],[h,-h],[h,h],[-h,h]].map(([a,d]) => v(n.x + a, n.y + d, n.z - size));
      for (let i = 0; i < 4; i++) { tri.push(apex, b[i], b[(i + 1) % 4]); who.push(s.node); }
      tri.push(b[0], b[2], b[1], b[0], b[3], b[2]); who.push(s.node, s.node);
    }
  }
  return { positions: new Float32Array(pos), triangles: new Uint32Array(tri), supportNode: new Int32Array(who) };
}

export function modelBounds(view: ModelView): { min: Vec3; max: Vec3; size: number } {
  const min: Vec3 = [Infinity, Infinity, Infinity], max: Vec3 = [-Infinity, -Infinity, -Infinity];
  for (const n of view.model.nodes) { const p = [n.x, n.y, n.z]; for (let k = 0; k < 3; k++) { min[k] = Math.min(min[k], p[k]); max[k] = Math.max(max[k], p[k]); } }
  if (!view.model.nodes.length) return { min: [0, 0, 0], max: [1, 1, 1], size: 1 };
  return { min, max, size: Math.max(max[0] - min[0], max[1] - min[1], max[2] - min[2], 1) };
}

export function elementLength(view: ModelView, id: number): number | null {
  const b = view.model.beams.find((e) => e.id === id); if (!b) return null;
  const n = new Map(view.model.nodes.map((q) => [q.id, q]));
  const a = n.get(b.nodes[0])!, c = n.get(b.nodes[1])!;
  return Math.hypot(c.x - a.x, c.y - a.y, c.z - a.z);
}

export const add3 = add;
/** Centroidal axis end points of a beam: node + offset (the solver applies the rigid end offsets the same way). */
export function memberAxesOfBeam(e: { nodes: [number, number]; offset1?: Vec3; offset2?: Vec3 }, nodePos: Map<number, Vec3>): { a: Vec3; b: Vec3 } {
  return { a: add(nodePos.get(e.nodes[0])!, e.offset1 ?? [0, 0, 0]), b: add(nodePos.get(e.nodes[1])!, e.offset2 ?? [0, 0, 0]) };
}
