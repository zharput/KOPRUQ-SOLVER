// Spike data: the REAL model 100 (native kopruq-model JSON) and its REAL results.json (SDL-1 case),
// optionally tiled `copies` times along y to scale the element count.
export async function loadData(copies = 1, caseName = 'SDL-1') {
  const [model, results] = await Promise.all([fetch('/data/m100_model.json').then(r => r.json()), fetch('/data/m100_results.json').then(r => r.json())]);
  const lc = results.load_cases.find(c => c.name === caseName);
  const dispById = new Map(lc.nodes.map(n => [n.id, n.displacement]));
  const nN = model.nodes.length, nE = model.beams.length;
  const idx = new Map(model.nodes.map((n, i) => [n.id, i]));
  const pos = new Float32Array(nN * copies * 3), disp = new Float32Array(nN * copies * 3), mag = new Float32Array(nN * copies);
  const conn = new Uint32Array(nE * copies * 2), elemId = new Int32Array(nE * copies);
  for (let c = 0; c < copies; c++) {
    model.nodes.forEach((n, i) => {
      const k = (c * nN + i) * 3, d = dispById.get(n.id);
      pos[k] = n.x; pos[k + 1] = n.y + c * 30; pos[k + 2] = n.z;
      disp[k] = d[0]; disp[k + 1] = d[1]; disp[k + 2] = d[2];
      mag[c * nN + i] = Math.hypot(d[0], d[1], d[2]);
    });
    model.beams.forEach((b, j) => {
      const e = c * nE + j;
      conn[e * 2] = c * nN + idx.get(b.nodes[0]); conn[e * 2 + 1] = c * nN + idx.get(b.nodes[1]);
      elemId[e] = c * 100000 + b.id;
    });
  }
  return { nNodes: nN * copies, nElems: nE * copies, pos, disp, mag, conn, elemId };
}
export const post = (name, obj) => fetch('http://127.0.0.1:5181/' + name, { method: 'POST', body: JSON.stringify(obj) });
export const median = a => { const s = [...a].sort((x, y) => x - y); return s[Math.floor(s.length / 2)]; };
export function colorRamp(t) { // blue -> cyan -> green -> yellow -> red, t in [0,1]
  const s = Math.min(1, Math.max(0, t)) * 4, i = Math.floor(s), f = s - i;
  const k = [[0,0,1],[0,1,1],[0,1,0],[1,1,0],[1,0,0]]; const a = k[Math.min(i,4)], b = k[Math.min(i+1,4)];
  return [a[0]+(b[0]-a[0])*f, a[1]+(b[1]-a[1])*f, a[2]+(b[2]-a[2])*f];
}
export function segDist(px, py, ax, ay, bx, by) { // distance (px) from point to 2D segment
  const dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy; let t = l2 ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0; t = Math.max(0, Math.min(1, t));
  return Math.hypot(px - (ax + t * dx), py - (ay + t * dy));
}
