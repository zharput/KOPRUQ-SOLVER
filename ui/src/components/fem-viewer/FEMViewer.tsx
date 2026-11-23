import '@kitware/vtk.js/Rendering/Profiles/Geometry';
import vtkActor from '@kitware/vtk.js/Rendering/Core/Actor';
import vtkDataArray from '@kitware/vtk.js/Common/Core/DataArray';
import vtkGenericRenderWindow from '@kitware/vtk.js/Rendering/Misc/GenericRenderWindow';
import vtkMapper from '@kitware/vtk.js/Rendering/Core/Mapper';
import vtkPolyData from '@kitware/vtk.js/Common/DataModel/PolyData';
import { useEffect, useRef, useState } from 'react';
import { buildLines, buildSolids, modelBounds, supportGlyphs, type LineMesh } from '../../model/geometry';
import { beamStates, displace, rampColor } from '../../model/displaced';
import type { ModelView } from '../../model/types';
import type { ResultCase } from '../../results/types';
import { type CameraPreset, type ResultView, useUi } from '../../state/store';
import { CONTOUR_QUANTITY, CONTOUR_SHORT, forceField, FORCE_INDEX } from '../../model/contourField';
import type { StationCase } from '../../results/stations';
import { factor, label as unitLabel, type UnitSystemId } from '../../units/units';
import { ViewerOverlays, type LegendInfo } from './ResultOverlay';

type Actor = ReturnType<typeof vtkActor.newInstance>;
const rgb = (hex: string): [number, number, number] => /^#[0-9a-f]{6}$/i.test(hex) ? [parseInt(hex.slice(1, 3), 16) / 255, parseInt(hex.slice(3, 5), 16) / 255, parseInt(hex.slice(5, 7), 16) / 255] : (() => { throw new Error(`colour must be #rrggbb: ${hex}`); })();
const poly = (points: Float32Array, cells?: { lines?: Uint32Array; polys?: Uint32Array; verts?: Uint32Array }) => {
  const pd = vtkPolyData.newInstance(); pd.getPoints().setData(points, 3);
  if (cells?.lines) pd.getLines().setData(cells.lines); if (cells?.polys) pd.getPolys().setData(cells.polys); if (cells?.verts) pd.getVerts().setData(cells.verts);
  return pd;
};
const actorOf = (pd: ReturnType<typeof poly>, color: string, setup?: (a: Actor) => void): Actor => {
  const m = vtkMapper.newInstance(); m.setInputData(pd); const a = vtkActor.newInstance(); a.setMapper(m); a.getProperty().setColor(...rgb(color)); setup?.(a); return a;
};
const withCounts = (idx: ArrayLike<number>, n: number) => { const o = new Uint32Array(idx.length / n * (n + 1)); for (let i = 0, k = 0; i < idx.length; i += n) { o[k++] = n; for (let j = 0; j < n; j++) o[k++] = idx[i + j]; } return o; };

interface Scene { grw: ReturnType<typeof vtkGenericRenderWindow.newInstance>; actors: Record<string, Actor>; view: ModelView; nodeIds: number[]; beamIds: number[]; solidMesh: ReturnType<typeof buildSolids>; triElement: Int32Array; supportNode: Int32Array; size: number; hi: Actor; hiPd: ReturnType<typeof poly>;
  lineMesh: LineMesh; seg: number; shownLines: Float32Array; nodeXYZ: Map<number, [number, number, number]>; baseSolid: ReturnType<typeof buildSolids> | null }

function buildScene(container: HTMLElement, view: ModelView): Scene {
  const grw = vtkGenericRenderWindow.newInstance({ background: [0.043, 0.082, 0.145] }); grw.setContainer(container); grw.resize();
  const renderer = grw.getRenderer();
  const b = modelBounds(view); const size = b.size;
  const nodePos = new Map(view.model.nodes.map((n) => [n.id, [n.x, n.y, n.z] as [number, number, number]]));
  // element axes (centroidal line, offsets applied); `seg` pieces per member so that a deformed member is a curve
  const lineMesh = buildLines(view, 1); const beamIds = lineMesh.beamIds;
  const lines = actorOf(poly(lineMesh.positions, { lines: lineMesh.segments }), '#38bdf8');
  // nodes as verts (cell id = node index)
  const nodeIds = view.model.nodes.map((n) => n.id); const np = new Float32Array(nodeIds.length * 3), nc: number[] = [];
  view.model.nodes.forEach((n, i) => { np.set([n.x, n.y, n.z], i * 3); nc.push(1, i); });
  const nodes = actorOf(poly(np, { verts: new Uint32Array(nc) }), '#e2e8f0', (a) => a.getProperty().setPointSize(4));
  // section solids
  const solid = buildSolids(view); const sp = poly(solid.positions, { polys: withCounts(solid.triangles, 3) });
  const rgba = new Uint8Array((solid.colors.length / 3) * 4); // RGBA: the 3-component direct-colour path drops a channel in vtk.js
  for (let i = 0; i < solid.colors.length / 3; i++) rgba.set([solid.colors[i * 3], solid.colors[i * 3 + 1], solid.colors[i * 3 + 2], 255], i * 4);
  sp.getPointData().setScalars(vtkDataArray.newInstance({ name: 'rgba', numberOfComponents: 4, values: rgba, dataType: 'Uint8Array' }));
  const solids = actorOf(sp, '#ffffff', (a) => { const m = a.getMapper()!; m.setColorModeToDirectScalars(); m.setScalarVisibility(true); a.getProperty().setAmbient(0.35); a.getProperty().setDiffuse(0.7); });
  // supports
  const sg = supportGlyphs(view, size * 0.012);
  const supports = actorOf(poly(sg.positions, { polys: withCounts(sg.triangles, 3) }), '#22c55e');
  // elastic / rigid links
  const seg = (pairs: [number, number][], color: string) => {
    const p = new Float32Array(pairs.length * 6), c: number[] = [];
    pairs.forEach(([x, y], i) => { p.set([...(nodePos.get(x) ?? [0, 0, 0]), ...(nodePos.get(y) ?? [0, 0, 0])], i * 6); c.push(2, i * 2, i * 2 + 1); });
    return actorOf(poly(p, { lines: new Uint32Array(c) }), color);
  };
  const elastic = seg((view.model.springs ?? []).map((s) => s.nodes), '#a78bfa');
  const rigid = seg((view.model.rigid_links ?? []).map((l) => [l.master, l.slave]), '#94a3b8');
  // selection highlight (geometry replaced on every selection)
  const hiPd = poly(new Float32Array(6), { lines: new Uint32Array([2, 0, 1]) });
  const hi = actorOf(hiPd, '#f59e0b', (a) => { a.getProperty().setLineWidth(4); a.getProperty().setPointSize(10); a.getProperty().setLighting(false); a.setVisibility(false); });
  hi.getMapper()!.setResolveCoincidentTopologyToPolygonOffset?.();
  const actors = { lines, nodes, solids, supports, elastic, rigid };
  Object.values(actors).forEach((a) => renderer.addActor(a)); renderer.addActor(hi);
  return { grw, actors, view, nodeIds, beamIds, solidMesh: solid, triElement: solid.triElement, supportNode: sg.supportNode, size, hi, hiPd, lineMesh, seg: 1, shownLines: lineMesh.positions, nodeXYZ: new Map(nodePos), baseSolid: null };
}

const SEG = 8; // pieces per member while a deformed shape or a contour is shown (a cubic member is drawn as a curve)
type PD = ReturnType<typeof poly>;
const inputOf = (a: Actor) => a.getMapper()!.getInputData() as PD;

/** Shows a result case on the scene: displaced geometry (scale from the user), colour contour, legend range. Returns the legend or null. */
function applyResult(s: Scene, c: ResultCase | null, rv: ResultView, ctx: { unit: UnitSystemId; stationCase?: StationCase }): LegendInfo | null {
  const { view } = s; const active = !!c && (rv.deformed || rv.contour !== 'none'); const seg = active ? SEG : 1;
  if (seg !== s.seg) {
    s.lineMesh = buildLines(view, seg); s.solidMesh = buildSolids(view, seg); s.triElement = s.solidMesh.triElement; s.seg = seg;
    const lp = inputOf(s.actors.lines); lp.getPoints().setData(s.lineMesh.positions, 3); lp.getLines().setData(s.lineMesh.segments);
    const sp = inputOf(s.actors.solids); sp.getPoints().setData(s.solidMesh.positions, 3); sp.getPolys().setData(withCounts(s.solidMesh.triangles, 3));
  }
  const disp = new Map((c?.nodes ?? []).map((n) => [n.id, n.disp])); const st = beamStates(view, disp); const scale = c && rv.deformed ? rv.scale : 0;
  const dl = displace(s.lineMesh, st, scale), ds = displace(s.solidMesh, st, scale); s.shownLines = dl.positions;
  const q = CONTOUR_QUANTITY[rv.contour], fac = factor(q, ctx.unit); const isForce = FORCE_INDEX[rv.contour] !== undefined;
  const scaled = (a: Float32Array) => { const o = new Float32Array(a.length); for (let i = 0; i < a.length; i++) o[i] = a[i] * fac; return o; };
  const fieldOf = (d: typeof dl, mesh: typeof s.lineMesh | typeof s.solidMesh) => {
    if (isForce) return ctx.stationCase ? scaled(forceField(mesh, s.beamIds, ctx.stationCase.elements, rv.contour)) : new Float32Array(mesh.vBeam.length).fill(NaN);
    return scaled(rv.contour === 'UX' ? d.ux : rv.contour === 'UY' ? d.uy : rv.contour === 'UZ' ? d.uz : d.u);
  };
  const fl = fieldOf(dl, s.lineMesh), fs = fieldOf(ds, s.solidMesh);
  let legend: LegendInfo | null = null; let lo = 0, hi = 1;
  if (c && rv.contour !== 'none') {
    let mn = Infinity, mx = -Infinity; for (const f of [fl, fs]) for (const v of f) { if (!Number.isFinite(v)) continue; if (v < mn) mn = v; if (v > mx) mx = v; }
    if (!Number.isFinite(mn)) { mn = 0; mx = 0; }
    lo = rv.rangeAuto ? mn : rv.rangeMin; hi = rv.rangeAuto ? mx : rv.rangeMax;
    legend = { name: CONTOUR_SHORT[rv.contour], unit: unitLabel(q, ctx.unit), dataMin: mn, dataMax: mx, lo, hi };
  }
  const span = hi - lo || 1; const colorize = (f: Float32Array, base: Uint8Array | null) => {
    const n = f.length, o = new Uint8Array(n * 4);
    for (let i = 0; i < n; i++) { const k = rv.contour === 'none' && base ? [base[i * 3], base[i * 3 + 1], base[i * 3 + 2]] : Number.isFinite(f[i]) ? rampColor((f[i] - lo) / span) : [110, 118, 130]; o.set([k[0], k[1], k[2], 255], i * 4); }
    return vtkDataArray.newInstance({ name: 'rgba', numberOfComponents: 4, values: o, dataType: 'Uint8Array' });
  };
  const sp = inputOf(s.actors.solids); sp.getPoints().setData(ds.positions, 3); sp.getPointData().setScalars(colorize(fs, s.solidMesh.colors)); sp.modified();
  const lp = inputOf(s.actors.lines); lp.getPoints().setData(dl.positions, 3); lp.getPointData().setScalars(colorize(fl, null)); lp.modified();
  const lm = s.actors.lines.getMapper()!; lm.setColorModeToDirectScalars(); lm.setScalarVisibility(rv.contour !== 'none');
  // nodes and links follow the nodes' own displacement
  const at = (id: number): [number, number, number] => { const n = s.view.model.nodes.find((q) => q.id === id)!; const d = disp.get(id); return [n.x + scale * (d?.[0] ?? 0), n.y + scale * (d?.[1] ?? 0), n.z + scale * (d?.[2] ?? 0)]; };
  const np = new Float32Array(s.nodeIds.length * 3); s.nodeIds.forEach((id, i) => { const p = at(id); np.set(p, i * 3); s.nodeXYZ.set(id, p); });
  const npd = inputOf(s.actors.nodes); npd.getPoints().setData(np, 3); npd.modified();
  const moveLinks = (a: Actor, pairs: [number, number][]) => { const p = new Float32Array(pairs.length * 6); pairs.forEach(([x, y], i) => p.set([...at(x), ...at(y)], i * 6)); const pd = inputOf(a); pd.getPoints().setData(p, 3); pd.modified(); };
  moveLinks(s.actors.elastic, (view.model.springs ?? []).map((q) => q.nodes)); moveLinks(s.actors.rigid, (view.model.rigid_links ?? []).map((l) => [l.master, l.slave]));
  s.grw.getRenderer().resetCameraClippingRange(); s.grw.getRenderWindow().render();
  return legend;
}

function applyCamera(s: Scene, preset: CameraPreset) {
  const r = s.grw.getRenderer(), cam = r.getActiveCamera(); const b = modelBounds(s.view);
  const c = [0, 1, 2].map((k) => (b.min[k] + b.max[k]) / 2); const d = s.size * 1.6;
  const pos: Record<Exclude<CameraPreset, 'fit'>, { p: number[]; up: number[] }> = {
    iso: { p: [c[0] - d * 0.7, c[1] - d * 0.7, c[2] + d * 0.5], up: [0, 0, 1] },
    front: { p: [c[0], c[1] - d, c[2]], up: [0, 0, 1] },   // looking along +Y, X to the right
    top: { p: [c[0], c[1], c[2] + d], up: [0, 1, 0] },     // looking down, Y up
    side: { p: [c[0] + d, c[1], c[2]], up: [0, 0, 1] },    // looking along -X, Y to the right
  };
  if (preset !== 'fit') { const q = pos[preset]; cam.setFocalPoint(c[0], c[1], c[2]); cam.setPosition(q.p[0], q.p[1], q.p[2]); cam.setViewUp(q.up[0], q.up[1], q.up[2]); }
  r.resetCamera([b.min[0], b.max[0], b.min[1], b.max[1], b.min[2], b.max[2]]);
  // fit: project the 8 bounding-box corners and zoom so that the model fills ~88% of the canvas (independent of the canvas shape)
  r.resetCameraClippingRange();
  try {
    const ogl = s.grw.getApiSpecificRenderWindow() as unknown as { getSize(): number[]; worldToDisplay(a: number, b: number, c: number, r: unknown): number[] }; const [W, H] = ogl.getSize();
    let x0 = Infinity, x1 = -Infinity, y0 = Infinity, y1 = -Infinity;
    for (const x of [b.min[0], b.max[0]]) for (const y of [b.min[1], b.max[1]]) for (const z of [b.min[2], b.max[2]]) { const d = ogl.worldToDisplay(x, y, z, r); x0 = Math.min(x0, d[0]); x1 = Math.max(x1, d[0]); y0 = Math.min(y0, d[1]); y1 = Math.max(y1, d[1]); }
    const f = Math.min(W / (x1 - x0), H / (y1 - y0)) * 0.88; if (Number.isFinite(f) && f > 0) cam.zoom(Math.min(Math.max(f, 0.2), 8)); else cam.zoom(1.9);
  } catch { cam.zoom(1.9); }
  r.resetCameraClippingRange(); s.grw.getRenderWindow().render();
}

function updateHighlight(s: Scene, sel: ReturnType<typeof useUi.getState>['selection'], solid: boolean) {
  const { view } = s; const nodePos = s.nodeXYZ;
  s.hi.setVisibility(!!sel);
  if (!sel) return;
  const pd = s.hiPd;
  if (sel.kind === 'node') {
    const p = nodePos.get(sel.id)!; pd.getPoints().setData(new Float32Array(p), 3); pd.getLines().setData(new Uint32Array([])); pd.getPolys().setData(new Uint32Array([])); pd.getVerts().setData(new Uint32Array([1, 0]));
  } else {
    const e = view.model.beams.find((x) => x.id === sel.id); if (!e) return;
    if (solid && s.solidMesh.triElement.includes(sel.id)) { // tint the element's own solid faces (a line inside the solid would be hidden)
      const { positions, triangles, triElement } = s.solidMesh; const pts: number[] = [], cells: number[] = [];
      const eps = s.size * 0.0007; // lift each face along its own normal so it does not fight with the solid for depth
      triElement.forEach((el, t) => {
        if (el !== sel.id) return; const base = pts.length / 3;
        const P = [0, 1, 2].map((k) => { const v = triangles[t * 3 + k]; return [positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2]]; });
        const u = [P[1][0] - P[0][0], P[1][1] - P[0][1], P[1][2] - P[0][2]], w = [P[2][0] - P[0][0], P[2][1] - P[0][1], P[2][2] - P[0][2]];
        const n = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]; const l = Math.hypot(n[0], n[1], n[2]) || 1;
        for (const q of P) pts.push(q[0] + (n[0] / l) * eps, q[1] + (n[1] / l) * eps, q[2] + (n[2] / l) * eps);
        cells.push(3, base, base + 1, base + 2);
      });
      pd.getPoints().setData(new Float32Array(pts), 3); pd.getVerts().setData(new Uint32Array([])); pd.getLines().setData(new Uint32Array([])); pd.getPolys().setData(new Uint32Array(cells));
    } else {
      const bi = view.model.beams.indexOf(e), pc = s.seg; const q = s.shownLines.slice(bi * (pc + 1) * 3, (bi + 1) * (pc + 1) * 3); const cells: number[] = [];
      for (let k = 0; k < pc; k++) cells.push(2, k, k + 1);
      pd.getVerts().setData(new Uint32Array([])); pd.getPolys().setData(new Uint32Array([])); pd.getPoints().setData(q, 3); pd.getLines().setData(new Uint32Array(cells));
    }
  }
  pd.modified();
}

export function FEMViewer() {
  const ref = useRef<HTMLDivElement>(null); const sceneRef = useRef<Scene | null>(null);
  const { view, show, selection, camera, select, results, resultView } = useUi(); const unitSystem = useUi((st) => st.unitSystem); const stations = useUi((st) => st.stations); const [legend, setLegend] = useState<LegendInfo | null>(null);

  useEffect(() => {  // (re)build the scene when a model is loaded
    const el = ref.current; if (!el || !view) return;
    const s = buildScene(el, view); sceneRef.current = s; applyCamera(s, 'iso');
    const raf = requestAnimationFrame(() => { s.grw.resize(); applyCamera(s, 'iso'); }); // once more after the layout settled
    const ro = new ResizeObserver(() => { s.grw.resize(); s.grw.getRenderWindow().render(); }); ro.observe(el);
    // click = pick (drag = rotate, ignored)
    const nodePosById = s.nodeXYZ;
    let down: { x: number; y: number } | null = null;
    const onDown = (e: PointerEvent) => { down = { x: e.clientX, y: e.clientY }; };
    const onUp = async (e: PointerEvent) => {
      if (!down || Math.hypot(e.clientX - down.x, e.clientY - down.y) > 4) { down = null; return; } down = null;
      // createSelector exists at runtime (HardwareSelector) but is missing from the type declarations
      const ogl = s.grw.getApiSpecificRenderWindow() as unknown as { getSize(): number[]; createSelector(): any }; const rect = el.getBoundingClientRect(); const sz = ogl.getSize();
      const x = Math.round((e.clientX - rect.left) * sz[0] / rect.width), y = Math.round((rect.bottom - e.clientY) * sz[1] / rect.height);
      const sel = ogl.createSelector(); sel.setFieldAssociation(1);
      const R = 4; // capture once (+-4 px); nodes are chosen by screen distance in that area, elements from the clicked pixel only
      const data = await sel.getSourceDataAsync(s.grw.getRenderer(), x - R, y - R, x + R, y + R);
      let node = -1, elem = -1;
      if (data) {
        const w2d = (id: number) => { const n = nodePosById.get(id)!; const d = (ogl as unknown as { worldToDisplay(a: number, b: number, c: number, r: unknown): number[] }).worldToDisplay(n[0], n[1], n[2], s.grw.getRenderer()); return Math.hypot(d[0] - x, d[1] - y); };
        let best = Infinity;
        for (const r of data.generateSelection(x - R, y - R, x + R, y + R) ?? []) {
          const p = r.getProperties(); const a = p.attributeID as number;
          if (p.prop === s.actors.nodes && a >= 0) { const d = w2d(s.nodeIds[a]); if (d < best && d <= R) { best = d; node = s.nodeIds[a]; } }
        }
        if (node < 0) for (const r of data.generateSelection(x - 1, y - 1, x + 1, y + 1) ?? []) {
          const p = r.getProperties(); const a = p.attributeID as number;
          if (a < 0) continue;
          if (p.prop === s.actors.supports) { node = s.supportNode[a]; break; } // a support glyph selects its node
          if (p.prop === s.actors.solids) { elem = s.solidMesh.triElement[a]; break; }
          if (p.prop === s.actors.lines) { elem = s.beamIds[s.lineMesh.segBeam[a]]; break; }
        }
      }
      select(node >= 0 ? { kind: 'node', id: node } : elem >= 0 ? { kind: 'element', id: elem } : null);
    };
    el.addEventListener('pointerdown', onDown); el.addEventListener('pointerup', onUp);
    (window as unknown as { __femViewer?: Scene }).__femViewer = s;
    return () => { cancelAnimationFrame(raf); ro.disconnect(); el.removeEventListener('pointerdown', onDown); el.removeEventListener('pointerup', onUp); s.grw.delete(); sceneRef.current = null; el.innerHTML = ''; };
  }, [view, select]);

  useEffect(() => {  // visibility toggles
    const s = sceneRef.current; if (!s) return; const a = s.actors;
    a.solids.setVisibility(show.solid); a.lines.setVisibility(show.elements && !show.solid);
    a.nodes.setVisibility(show.nodes); a.supports.setVisibility(show.supports); a.elastic.setVisibility(show.links); a.rigid.setVisibility(show.links);
    s.grw.getRenderWindow().render();
  }, [show, view]);
  useEffect(() => { const s = sceneRef.current; if (s) { updateHighlight(s, selection, show.solid); s.grw.getRenderWindow().render(); } }, [selection, show.solid, view]);
  useEffect(() => {  // result display: deformed shape / contour (undone when there are no results)
    const s = sceneRef.current; if (!s) return;
    const c = results ? (results.cases[Math.min(resultView.caseIndex, results.cases.length - 1)] ?? null) : null;
    const sc = c && stations ? stations.cases.find((x) => x.name === c.name && x.kind === c.kind) : undefined;
    setLegend(applyResult(s, c, resultView, { unit: unitSystem, stationCase: sc })); updateHighlight(s, useUi.getState().selection, show.solid); s.grw.getRenderWindow().render();
  }, [results, resultView, view, show.solid, unitSystem, stations]);
  useEffect(() => { const s = sceneRef.current; if (s && camera.n > 0) applyCamera(s, camera.preset); }, [camera]);

  if (!view) return (<div className="viewer" data-testid="fem-viewer"><div className="note"><div style={{ fontSize: 15, color: 'var(--text)', marginBottom: 6 }}>3D FEM Viewer</div>
    No model loaded. Use <b>Open</b> to read a model-view JSON (<code>kopruq_solver_cli inspect model.json -o view.json</code>) or <b>Sample</b>.</div></div>);
  return <div className="viewer" data-testid="fem-viewer" style={{ display: 'block' }}><div ref={ref} className="vtkhost" /><ViewerOverlays legend={legend} /></div>;
}
