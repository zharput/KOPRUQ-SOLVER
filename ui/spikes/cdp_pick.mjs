// Drives the real app in headless Edge through the DevTools protocol and checks click picking.
import { spawn } from 'node:child_process';
const edge = spawn('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', ['--headless=new', '--disable-gpu', '--use-angle=swiftshader', '--enable-unsafe-swiftshader',
  '--ignore-gpu-blocklist', '--remote-debugging-port=9334', '--user-data-dir=' + process.env.TEMP + '/edge_cdp', '--window-size=1600,900', 'http://127.0.0.1:5173/?sample'], { stdio: 'ignore' });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let list; for (let i = 0; i < 40; i++) { try { list = await (await fetch('http://127.0.0.1:9334/json')).json(); if (list.find((t) => t.type === 'page')) break; } catch {} await sleep(500); }
const ws = new WebSocket(list.find((t) => t.type === 'page').webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pending = new Map(); ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); } };
const ev = (expression) => new Promise((res) => { const i = ++id; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: 'Runtime.evaluate', params: { expression, awaitPromise: true, returnByValue: true } })); }).then((d) => d.result?.result?.value ?? JSON.stringify(d.result?.exceptionDetails ?? d.result).slice(0, 400));
for (let i = 0; i < 60 && !(await ev('!!window.__femViewer')); i++) await sleep(500);
await sleep(1500);
const click = (kind, idv) => ev(`(async()=>{
  const s=window.__femViewer, v=s.view, el=document.querySelector('[data-testid=fem-viewer]'); const rect=el.getBoundingClientRect();
  const ogl=s.grw.getApiSpecificRenderWindow(), sz=ogl.getSize(); const pos=new Map(v.model.nodes.map(n=>[n.id,[n.x,n.y,n.z]]));
  let w; if('${kind}'==='node') w=pos.get(${idv}); else { const b=v.model.beams.find(e=>e.id===${idv}); const a=pos.get(b.nodes[0]),c=pos.get(b.nodes[1]); const o1=b.offset1||[0,0,0],o2=b.offset2||[0,0,0]; w=[0,1,2].map(k=>((a[k]+o1[k])+(c[k]+o2[k]))/2); }
  const cam=s.grw.getRenderer().getActiveCamera(); const fp=cam.getFocalPoint(), pp=cam.getPosition(); const dist=Math.hypot(pp[0]-fp[0],pp[1]-fp[1],pp[2]-fp[2]);
  const u=[0,1,2].map(k=>(pp[k]-fp[k])/dist); const near=Math.min(dist, 14); // zoom to ~14 m from the target so that neighbours are separable on screen
  cam.setFocalPoint(...w); cam.setPosition(w[0]+u[0]*near,w[1]+u[1]*near,w[2]+u[2]*near); s.grw.getRenderer().resetCameraClippingRange(); s.grw.getRenderWindow().render(); await new Promise(r=>setTimeout(r,300));
  const d=ogl.worldToDisplay(w[0],w[1],w[2],s.grw.getRenderer());
  const cx=rect.left+d[0]*rect.width/sz[0], cy=rect.bottom-d[1]*rect.height/sz[1];
  for(const t of ['pointerdown','pointerup']) el.dispatchEvent(new PointerEvent(t,{clientX:cx,clientY:cy,bubbles:true,pointerId:1}));
  await new Promise(r=>setTimeout(r,900));
  const txt=document.querySelector('[aria-label=Properties] .body').innerText.replace(/\s+/g,' ');
  return txt.slice(0,60);})()`);
const idOf = (t) => (typeof t === 'string' ? (t.match(/ID	([NE]\d+)/) || [])[1] ?? 'none' : 'error');
const run = async (cases) => { const out = []; for (const [k, v] of cases) { const got = idOf(await click(k, v)); out.push({ asked: (k === 'node' ? 'N' : 'E') + v, got, ok: got === (k === 'node' ? 'N' : 'E') + v }); } return out; };
const solid = await run([['element', 5], ['element', 20], ['element', 41], ['element', 60], ['element', 85], ['element', 3], ['element', 70], ['element', 90]]);
await ev(`[...document.querySelectorAll('button')].find(b=>b.textContent==='Solid').click()`); await sleep(800);
const lines = await run([['node', 1], ['node', 33], ['node', 60], ['node', 90], ['element', 20], ['element', 41], ['element', 60], ['element', 85]]);
console.log('SOLID MODE   ', solid.filter((r) => r.ok).length + '/' + solid.length, JSON.stringify(solid.filter((r) => !r.ok)));
console.log('LINE MODE    ', lines.filter((r) => r.ok).length + '/' + lines.length, JSON.stringify(lines.filter((r) => !r.ok)));
ws.close(); edge.kill();
