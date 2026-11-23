import '@kitware/vtk.js/Rendering/Profiles/Geometry';
import vtkGenericRenderWindow from '@kitware/vtk.js/Rendering/Misc/GenericRenderWindow';
import vtkPolyData from '@kitware/vtk.js/Common/DataModel/PolyData';
import vtkActor from '@kitware/vtk.js/Rendering/Core/Actor';
import vtkMapper from '@kitware/vtk.js/Rendering/Core/Mapper';
import vtkDataArray from '@kitware/vtk.js/Common/Core/DataArray';
import vtkColorTransferFunction from '@kitware/vtk.js/Rendering/Core/ColorTransferFunction';
import { loadData, post, median, segDist } from './common.js';

async function bench(copies) {
  const d = await loadData(copies); const R = { lib: 'vtk.js', copies, elements: d.nElems, nodes: d.nNodes };
  const container = document.getElementById('c'); container.innerHTML = '';
  const grw = vtkGenericRenderWindow.newInstance({ background: [0.043, 0.082, 0.145] }); grw.setContainer(container); grw.resize();
  const renderer = grw.getRenderer(), rw = grw.getRenderWindow(), ogl = grw.getApiSpecificRenderWindow();
  const t0 = performance.now();
  const pts = new Float32Array(d.pos);                       // working copy: deformation rewrites it
  const cells = new Uint32Array(d.nElems * 3);
  for (let e = 0; e < d.nElems; e++) { cells[e * 3] = 2; cells[e * 3 + 1] = d.conn[e * 2]; cells[e * 3 + 2] = d.conn[e * 2 + 1]; }
  const pd = vtkPolyData.newInstance();
  pd.getPoints().setData(pts, 3); pd.getLines().setData(cells);
  const scal = vtkDataArray.newInstance({ name: 'umag', numberOfComponents: 1, values: new Float32Array(d.mag) });
  pd.getPointData().setScalars(scal);
  const lut = vtkColorTransferFunction.newInstance(); const maxM = Math.max(...d.mag.slice(0, 5000));
  [[0,0,0,1],[0.25,0,1,1],[0.5,0,1,0],[0.75,1,1,0],[1,1,0,0]].forEach(([t, r, g, b]) => lut.addRGBPoint(t * maxM, r, g, b));
  const mapper = vtkMapper.newInstance({ interpolateScalarsBeforeMapping: true }); mapper.setInputData(pd); mapper.setLookupTable(lut); mapper.setScalarRange(0, maxM);
  const actor = vtkActor.newInstance(); actor.setMapper(mapper); actor.getProperty().setLineWidth(1); renderer.addActor(actor);
  const b = pd.getBounds(), c = [(b[0]+b[1])/2, (b[2]+b[3])/2, (b[4]+b[5])/2], rad = Math.hypot(b[1]-b[0], b[3]-b[2], b[5]-b[4]) / 2;
  const cam = renderer.getActiveCamera(); cam.setViewUp(0, 0, 1); cam.setFocalPoint(...c); cam.setPosition(c[0] - rad * 1.2, c[1] - rad * 1.2, c[2] + rad * 0.9); cam.setClippingRange(0.1, 5000);
  R.build_ms = performance.now() - t0;
  const px4 = new Uint8Array(4); const frame = () => { rw.render(); const g = ogl.getContext(); g.readPixels(0, 0, 1, 1, g.RGBA, g.UNSIGNED_BYTE, px4); };
  frame(); const fr = []; for (let i = 0; i < 20; i++) { const t = performance.now(); frame(); fr.push(performance.now() - t); } R.render_ms_median = median(fr);
  // deformation + recolor: rewrite point coordinates and scalars, modified(), render
  const df = []; for (let i = 0; i < 10; i++) {
    const t = performance.now(); const sc = 50 + i;
    for (let k = 0; k < pts.length; k++) pts[k] = d.pos[k] + sc * d.disp[k];
    pd.getPoints().modified(); scal.modified(); pd.modified(); frame(); df.push(performance.now() - t); }
  R.deform_recolor_ms_median = median(df);
  pts.set(d.pos); pd.getPoints().modified(); pd.modified(); frame();
  // picking via the GPU hardware selector (CellPicker does not intersect line cells)
  let ok = 0, valid = 0, tot = 0; const pt = []; const scr = (n) => { const q = ogl.worldToDisplay(d.pos[n*3], d.pos[n*3+1], d.pos[n*3+2], renderer); return [q[0], q[1]]; }; const sel = ogl.createSelector(); sel.setFieldAssociation(1);
  const test = []; for (let i = 0; i < 40; i++) test.push(Math.floor((i + 0.5) * d.nElems / 40));
  const size = ogl.getSize(); R.pick_error = null;
  try {
    for (const e of test) {
      const a = d.conn[e * 2], bb = d.conn[e * 2 + 1];
      const wp = [0,1,2].map(k => (d.pos[a*3+k] + d.pos[bb*3+k]) / 2);
      const ds = ogl.worldToDisplay(wp[0], wp[1], wp[2], renderer); // display coords (origin bottom-left)
      const x = Math.round(ds[0]), y = Math.round(ds[1]);
      const t = performance.now(); const data = await sel.getSourceDataAsync(renderer, x - 2, y - 2, x + 2, y + 2);
      const res = data ? data.generateSelection(x - 2, y - 2, x + 2, y + 2) : []; pt.push(performance.now() - t);
      tot++; const id = res && res[0] ? res[0].getProperties().attributeID : -1; if (id >= 0 && d.elemId[id] === d.elemId[e]) ok++; if (id >= 0) { const p = scr(d.conn[id * 2]), q = scr(d.conn[id * 2 + 1]); if (segDist(ds[0], ds[1], p[0], p[1], q[0], q[1]) <= 3) valid++; }
    }
  } catch (err) { R.pick_error = String(err); }
  R.pick_exact = `${ok}/${tot}`; R.pick_valid = `${valid}/${tot}`; R.pick_ms_median = pt.length ? median(pt) : null;
  if (copies === 1) { frame(); const cv = container.querySelector('canvas'); await post('vtk_img', { png: cv.toDataURL('image/png') }); } R.gpu = ogl.getContext().getParameter(ogl.getContext().RENDERER); grw.delete(); return R;
}
const out = []; for (const c of [1, 100]) out.push(await bench(c));
await post('vtk', out); document.title = 'done';
