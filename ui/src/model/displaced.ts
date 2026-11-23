// Displaced positions and result scalars for the viewer meshes (pure functions, no WebGL).
// Every mesh vertex knows its beam, its position xi along the member and its offset r from the member axis;
// the displacement of the vertex is  u(xi) + theta(xi) x r  (first-order rotation of the section about the axis).
import { memberAxes, type MemberAxes } from './geometry';
import { endState, memberDeformation, type EndState } from './deform';
import type { ModelView, Vec3 } from './types';

export interface VertexMesh { positions: Float32Array; vBeam: Int32Array; vXi: Float32Array; vR?: Float32Array }
export interface Displaced { positions: Float32Array; ux: Float32Array; uy: Float32Array; uz: Float32Array; u: Float32Array }

interface BeamState { ax: MemberAxes | null; e1: EndState; e2: EndState }
const add = (a: Vec3, b: Vec3): Vec3 => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];

/** Per-beam axes and end states; nodes missing from the case (e.g. not reported) have zero displacement. */
export function beamStates(view: ModelView, disp: Map<number, number[]>): BeamState[] {
  const pos = new Map(view.model.nodes.map((n) => [n.id, [n.x, n.y, n.z] as Vec3]));
  const zero = [0, 0, 0, 0, 0, 0];
  return view.model.beams.map((b) => {
    const o1 = b.offset1 ?? [0, 0, 0], o2 = b.offset2 ?? [0, 0, 0];
    const A = add(pos.get(b.nodes[0])!, o1), B = add(pos.get(b.nodes[1])!, o2);
    return { ax: memberAxes(A, B, b.reference_y ?? [0, 1, 0]), e1: endState(disp.get(b.nodes[0]) ?? zero, o1), e2: endState(disp.get(b.nodes[1]) ?? zero, o2) };
  });
}

export function displace(mesh: VertexMesh, states: BeamState[], scale: number): Displaced {
  const n = mesh.vBeam.length, pos = new Float32Array(n * 3), ux = new Float32Array(n), uy = new Float32Array(n), uz = new Float32Array(n), um = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const st = states[mesh.vBeam[i]]; let u: Vec3 = [0, 0, 0];
    if (st.ax) {
      const d = memberDeformation(st.ax, st.e1, st.e2, mesh.vXi[i]);
      const r: Vec3 = mesh.vR ? [mesh.vR[i * 3], mesh.vR[i * 3 + 1], mesh.vR[i * 3 + 2]] : [0, 0, 0];
      const t = d.theta; u = [d.u[0] + t[1] * r[2] - t[2] * r[1], d.u[1] + t[2] * r[0] - t[0] * r[2], d.u[2] + t[0] * r[1] - t[1] * r[0]];
    }
    for (let k = 0; k < 3; k++) pos[i * 3 + k] = mesh.positions[i * 3 + k] + scale * u[k];
    ux[i] = u[0]; uy[i] = u[1]; uz[i] = u[2]; um[i] = Math.hypot(u[0], u[1], u[2]);
  }
  return { positions: pos, ux, uy, uz, u: um };
}

/** Blue -> cyan -> green -> yellow -> red for t in [0,1]. */
export function rampColor(t: number): [number, number, number] {
  const x = Math.min(1, Math.max(0, Number.isFinite(t) ? t : 0)) * 4, seg = Math.min(3, Math.floor(x)), f = x - seg;
  const stops: [number, number, number][] = [[37, 99, 235], [34, 211, 238], [74, 222, 128], [250, 204, 21], [239, 68, 68]];
  const a = stops[seg], b = stops[seg + 1];
  return [Math.round(a[0] + (b[0] - a[0]) * f), Math.round(a[1] + (b[1] - a[1]) * f), Math.round(a[2] + (b[2] - a[2]) * f)];
}

/** Largest displacement magnitude over the nodes of a case (for the suggested scale factor). */
export const maxTranslation = (nodes: { disp: number[] }[]) => nodes.reduce((m, n) => Math.max(m, Math.hypot(n.disp[0], n.disp[1], n.disp[2]) || 0), 0);
/** Scale factor that makes the largest displacement ~ 8% of the model size. */
export const suggestScale = (modelSize: number, maxU: number) => (maxU > 0 ? Math.max(1, Math.round((0.08 * modelSize) / maxU)) : 1);
