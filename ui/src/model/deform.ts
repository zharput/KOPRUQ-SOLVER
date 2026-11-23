// Deformed shape of a straight member from its end displacements. Conventions are the solver's
// (kopruq/loads/Loads.hpp): theta_z = dv/dx, theta_y = -dw/dx (v along local y, w along local z); transverse
// displacement is the cubic Hermite interpolation of the end values, axial and twist are linear.
// First-order (small displacement) -- it is a picture of the linear solution scaled by the user, not a
// geometrically nonlinear result.
import type { MemberAxes } from './geometry';
import type { Vec3 } from './types';

const dot = (a: Vec3, b: Vec3) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a: Vec3, b: Vec3): Vec3 => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];

export interface EndState { u: Vec3; theta: Vec3 }
/** End state of the member axis (centroid) from the node's displacement and the rigid offset: u_c = u + theta x offset. */
export const endState = (d: number[], offset: Vec3 = [0, 0, 0]): EndState => {
  const th: Vec3 = [d[3], d[4], d[5]], c = cross(th, offset);
  return { u: [d[0] + c[0], d[1] + c[1], d[2] + c[2]], theta: th };
};

/** Translation and rotation (global components) of the member axis at xi in [0,1]. */
export function memberDeformation(ax: MemberAxes, e1: EndState, e2: EndState, xi: number): { u: Vec3; theta: Vec3 } {
  const L = ax.length, s = xi;
  const l = (e: EndState) => ({ ux: dot(e.u, ax.x), v: dot(e.u, ax.y), w: dot(e.u, ax.z), tx: dot(e.theta, ax.x), ty: dot(e.theta, ax.y), tz: dot(e.theta, ax.z) });
  const a = l(e1), b = l(e2);
  const H1 = 1 - 3 * s * s + 2 * s ** 3, H2 = L * (s - 2 * s * s + s ** 3), H3 = 3 * s * s - 2 * s ** 3, H4 = L * (-s * s + s ** 3);
  const D1 = (-6 * s + 6 * s * s) / L, D2 = 1 - 4 * s + 3 * s * s, D3 = (6 * s - 6 * s * s) / L, D4 = -2 * s + 3 * s * s;
  const ux = a.ux * (1 - s) + b.ux * s, tx = a.tx * (1 - s) + b.tx * s;
  const v = H1 * a.v + H2 * a.tz + H3 * b.v + H4 * b.tz;
  const w = H1 * a.w - H2 * a.ty + H3 * b.w - H4 * b.ty;
  const tz = D1 * a.v + D2 * a.tz + D3 * b.v + D4 * b.tz;     // dv/dx
  const ty = -(D1 * a.w - D2 * a.ty + D3 * b.w - D4 * b.ty);  // -dw/dx
  const g = (p: number, q: number, r: number): Vec3 => [ax.x[0] * p + ax.y[0] * q + ax.z[0] * r, ax.x[1] * p + ax.y[1] * q + ax.z[1] * r, ax.x[2] * p + ax.y[2] * q + ax.z[2] * r];
  return { u: g(ux, v, w), theta: g(tx, ty, tz) };
}
