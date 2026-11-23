import * as THREE from 'three';
import { loadData, post, median, colorRamp, segDist } from './common.js';
const canvas = document.getElementById('c');
async function bench(copies) {
  const d = await loadData(copies); const R = { lib: 'three', copies, elements: d.nElems, nodes: d.nNodes };
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, preserveDrawingBuffer: true });
  const scene = new THREE.Scene(); scene.background = new THREE.Color(0x0b1525);
  const t0 = performance.now();
  const geo = new THREE.BufferGeometry();
  const posArr = new Float32Array(d.nElems * 6), colArr = new Float32Array(d.nElems * 6);
  const fill = (scale) => { const maxM = Math.max(...d.mag.slice(0, Math.min(d.mag.length, 5000)));
    for (let e = 0; e < d.nElems; e++) for (let s = 0; s < 2; s++) { const n = d.conn[e * 2 + s];
      for (let k = 0; k < 3; k++) posArr[e * 6 + s * 3 + k] = d.pos[n * 3 + k] + scale * d.disp[n * 3 + k];
      const c = colorRamp(d.mag[n] / maxM); colArr.set(c, e * 6 + s * 3); } };
  fill(0);
  geo.setAttribute('position', new THREE.BufferAttribute(posArr, 3)); geo.setAttribute('color', new THREE.BufferAttribute(colArr, 3));
  const lines = new THREE.LineSegments(geo, new THREE.LineBasicMaterial({ vertexColors: true })); scene.add(lines);
  geo.computeBoundingSphere(); const bs = geo.boundingSphere;
  const cam = new THREE.PerspectiveCamera(45, 1000 / 600, 0.1, 5000);
  cam.up.set(0, 0, 1); cam.position.set(bs.center.x - bs.radius * 1.2, bs.center.y - bs.radius * 1.2, bs.center.z + bs.radius * 0.9); cam.lookAt(bs.center); cam.updateMatrixWorld();
  renderer.setSize(1000, 600, false);
  R.build_ms = performance.now() - t0;
  const gl = renderer.getContext(); const px4 = new Uint8Array(4); const frame = () => { renderer.render(scene, cam); gl.readPixels(0, 0, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, px4); };
  frame(); const fr = []; for (let i = 0; i < 20; i++) { const t = performance.now(); frame(); fr.push(performance.now() - t); } R.render_ms_median = median(fr);
  // deformation: rewrite positions + upload + render
  const df = []; for (let i = 0; i < 10; i++) { const t = performance.now(); fill(50 + i); geo.attributes.position.needsUpdate = true; geo.attributes.color.needsUpdate = true; frame(); df.push(performance.now() - t); } R.deform_recolor_ms_median = median(df);
  fill(0); geo.attributes.position.needsUpdate = true; frame();
  // picking: project element midpoints to the screen, raycast there, compare the element id
  const ray = new THREE.Raycaster(); ray.params.Line.threshold = bs.radius * 0.004; let ok = 0, valid = 0, tot = 0; const pt = []; const scr = (n) => { const v = new THREE.Vector3(d.pos[n*3], d.pos[n*3+1], d.pos[n*3+2]).project(cam); return [(v.x + 1) * 500, (1 - v.y) * 300]; };
  const test = []; for (let i = 0; i < 40; i++) test.push(Math.floor((i + 0.5) * d.nElems / 40));
  for (const e of test) {
    const a = d.conn[e * 2], b = d.conn[e * 2 + 1];
    const m = new THREE.Vector3((d.pos[a*3]+d.pos[b*3])/2, (d.pos[a*3+1]+d.pos[b*3+1])/2, (d.pos[a*3+2]+d.pos[b*3+2])/2); m.project(cam);
    const t = performance.now(); ray.setFromCamera(new THREE.Vector2(m.x, m.y), cam); const hit = ray.intersectObject(lines)[0]; pt.push(performance.now() - t);
    tot++;
    if (hit) { const he = Math.floor(hit.index / 2); if (d.elemId[he] === d.elemId[e]) ok++;
      const p = scr(d.conn[he * 2]), q = scr(d.conn[he * 2 + 1]), mx = (m.x + 1) * 500, my = (1 - m.y) * 300; if (segDist(mx, my, p[0], p[1], q[0], q[1]) <= 3) valid++; }
  }
  R.pick_exact = `${ok}/${tot}`; R.pick_valid = `${valid}/${tot}`; R.pick_ms_median = median(pt);
  if (copies === 1) { frame(); await post('three_img', { png: canvas.toDataURL('image/png') }); } R.gpu = gl.getParameter(gl.RENDERER); renderer.dispose(); return R;
}
const out = []; for (const c of [1, 100]) out.push(await bench(c));
await post('three', out); document.title = 'done';
