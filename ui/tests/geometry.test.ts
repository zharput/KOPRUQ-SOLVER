import { readFileSync } from 'node:fs';
import { buildSolids, memberAxes, modelBounds, polygonArea, supportGlyphs, triangulate } from '../src/model/geometry';
import { parseModelView } from '../src/model/load';
import type { Contour, ModelView } from '../src/model/types';

const real = parseModelView(readFileSync('tests/data/m100_view.json', 'utf-8')); // real `inspect` output of verification model 100
const dot = (a: number[], b: number[]) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const solidVolume = (pos: Float32Array, tri: Uint32Array, from = 0, to = tri.length / 3) => {
  let v = 0; // sum of signed tetrahedra against the origin = enclosed volume of a closed, outward-oriented surface
  for (let t = from; t < to; t++) { const [a, b, c] = [tri[t * 3], tri[t * 3 + 1], tri[t * 3 + 2]].map((i) => [pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]]);
    v += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6; }
  return v;
};
const synthetic = (contours: Contour[]): ModelView => ({
  schema: 'kopruq-model-view', schema_version: 1, metadata: { solver_version: 't', source_format: 'native', model_sha256: '' },
  model: { nodes: [{ id: 1, x: 0, y: 0, z: 0 }, { id: 2, x: 4, y: 0, z: 0 }], beams: [{ id: 7, nodes: [1, 2], material: 1, section: 1 }], supports: [], sections: [{ id: 1, area: 1 }], materials: [{ id: 1 }] },
  section_outlines: [{ section: 1, contours }], import: { assumptions: [], rejected: [] },
});

describe('member axes (= solver Beam3D::axes)', () => {
  it('x along the member, y = reference made orthogonal, z = x cross y', () => {
    const a = memberAxes([0, 0, 0], [4, 0, 0], [0, 1, 0])!;
    expect(a.x).toEqual([1, 0, 0]); expect(a.y).toEqual([0, 1, 0]); expect(a.z).toEqual([0, 0, 1]); expect(a.length).toBe(4);
    const v = memberAxes([0, 0, 0], [0, 0, 5], [0, 1, 0])!;            // vertical pier, reference y = global Y
    expect(v.y).toEqual([0, 1, 0]); expect(v.z).toEqual([-1, 0, 0]); // z = x cross y = (0,0,1) x (0,1,0) = (-1,0,0), computed by hand
  });
  it('is orthonormal and right-handed for every real beam of model 100; degenerate cases give null', () => {
    const pos = new Map(real.model.nodes.map((n) => [n.id, [n.x, n.y, n.z] as [number, number, number]]));
    for (const b of real.model.beams) {
      const ax = memberAxes(pos.get(b.nodes[0])!, pos.get(b.nodes[1])!, b.reference_y ?? [0, 1, 0])!;
      expect(ax).not.toBeNull();
      expect(dot(ax.x, ax.y)).toBeCloseTo(0, 12); expect(dot(ax.x, ax.z)).toBeCloseTo(0, 12); expect(dot(ax.y, ax.z)).toBeCloseTo(0, 12);
      const c = [ax.x[1] * ax.y[2] - ax.x[2] * ax.y[1], ax.x[2] * ax.y[0] - ax.x[0] * ax.y[2], ax.x[0] * ax.y[1] - ax.x[1] * ax.y[0]];
      expect(dot(c, ax.z)).toBeCloseTo(1, 12);
    }
    expect(memberAxes([0, 0, 0], [0, 0, 0], [0, 1, 0])).toBeNull();
    expect(memberAxes([0, 0, 0], [1, 0, 0], [2, 0, 0])).toBeNull(); // reference parallel to the member
  });
});

describe('section polygons', () => {
  it('ear clipping covers a non-convex I outline exactly (triangle areas sum to the polygon area)', () => {
    const I: Contour = [[-0.25, -1], [0.25, -1], [0.25, -0.9], [0.05, -0.9], [0.05, -0.1], [0.25, -0.1], [0.25, 0], [-0.25, 0], [-0.25, -0.1], [-0.05, -0.1], [-0.05, -0.9], [-0.25, -0.9]];
    const tris = triangulate(I);
    expect(tris.length).toBe(I.length - 2);
    const sum = tris.reduce((s, [a, b, c]) => s + Math.abs(polygonArea([I[a], I[b], I[c]])), 0);
    expect(sum).toBeCloseTo(Math.abs(polygonArea(I)), 12);
    expect(Math.abs(polygonArea(I))).toBeCloseTo(0.5 * 0.1 * 2 + 0.1 * 0.8, 12); // 2 flanges 0.5x0.1 + web 0.1x0.8
  });
  it('the real outlines of model 100 have the area of the solver sections (SB, B exactly; SR within the 48-gon error)', () => {
    for (const o of real.section_outlines) {
      const area = polygonArea(o.contours[0]) + o.contours.slice(1).reduce((s, h) => s + polygonArea(h), 0); // holes are clockwise (negative)
      const sec = real.model.sections.find((x) => x.id === o.section)!;
      expect(Math.abs(area - sec.area) / sec.area).toBeLessThan(3e-3);
    }
  });
});

describe('section solids', () => {
  it('a closed extrusion has the volume area x length (rectangle 0.5 x 1.0, length 4) and outward faces', () => {
    const view = synthetic([[[-0.25, -0.5], [0.25, -0.5], [0.25, 0.5], [-0.25, 0.5]]]);
    const m = buildSolids(view);
    expect(m.skipped).toEqual([]);
    expect(solidVolume(m.positions, m.triangles)).toBeCloseTo(0.5 * 1.0 * 4, 12);
    expect(new Set(m.triElement)).toEqual(new Set([7]));
  });
  it('a non-convex I section has the volume of its area', () => {
    const I: Contour = [[-0.25, -1], [0.25, -1], [0.25, -0.9], [0.05, -0.9], [0.05, -0.1], [0.25, -0.1], [0.25, 0], [-0.25, 0], [-0.25, -0.1], [-0.05, -0.1], [-0.05, -0.9], [-0.25, -0.9]];
    const m = buildSolids(synthetic([I]));
    expect(Math.abs(solidVolume(m.positions, m.triangles) / (Math.abs(polygonArea(I)) * 4) - 1)).toBeLessThan(1e-6); // vertex data is Float32
  });
  it('the member offset moves the solid (CT offset puts the node at the top fiber)', () => {
    const v = synthetic([[[-0.25, -0.5], [0.25, -0.5], [0.25, 0.5], [-0.25, 0.5]]]);
    v.model.beams[0].offset1 = [0, 0, -0.5]; v.model.beams[0].offset2 = [0, 0, -0.5];
    const z = Array.from({ length: v.model.beams.length ? buildSolids(v).positions.length / 3 : 0 }, (_, i) => buildSolids(v).positions[i * 3 + 2]);
    expect(Math.max(...z)).toBeCloseTo(0, 6); expect(Math.min(...z)).toBeCloseTo(-1, 6);
  });
  it('builds a solid for every beam of the real model 100 and skips none', () => {
    const m = buildSolids(real);
    expect(m.skipped).toEqual([]);
    expect(new Set(m.triElement).size).toBe(real.model.beams.length);
    expect(m.positions.length / 3).toBe(m.colors.length / 3);
  });
});

describe('support glyphs and bounds', () => {
  it('one glyph per restrained node of the real model; bounds equal the inspect bounds', () => {
    const g = supportGlyphs(real, 0.5);
    expect(new Set(g.supportNode).size).toBe(real.model.supports.filter((s) => s.restraints.some(Boolean)).length);
    const b = modelBounds(real);
    expect(b.min).toEqual(real.bounds!.min); expect(b.max).toEqual(real.bounds!.max);
  });
});
