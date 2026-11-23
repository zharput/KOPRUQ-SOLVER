import { spawn } from 'node:child_process'; import { writeFileSync } from 'node:fs';
const edge = spawn('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', ['--headless=new', '--disable-gpu', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist',
  '--remote-debugging-port=9336', '--user-data-dir=' + process.env.TEMP + '/edge_cdp3', '--window-size=1600,900', 'http://127.0.0.1:5173/?sample'], { stdio: 'ignore' });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let list; for (let i = 0; i < 40; i++) { try { list = await (await fetch('http://127.0.0.1:9336/json')).json(); if (list.find((t) => t.type === 'page')) break; } catch {} await sleep(500); }
const ws = new WebSocket(list.find((t) => t.type === 'page').webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pending = new Map(); ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); } };
const send = (method, params = {}) => new Promise((res) => { const i = ++id; pending.set(i, res); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = (e) => send('Runtime.evaluate', { expression: e, awaitPromise: true, returnByValue: true }).then((d) => d.result?.result?.value);
for (let i = 0; i < 60 && !(await ev('!!window.__femViewer')); i++) await sleep(500); await sleep(1500);
const clickText = (t, sel) => ev(`[...document.querySelectorAll('${sel}')].find(e=>e.textContent.replace(/^[▸▾ ]+/, '').trim().startsWith('${t}'))?.click()`);
import { readFileSync } from 'node:fs';
const shot = async (name) => { await sleep(2000); const d = await send('Page.captureScreenshot', { format: 'png' }); writeFileSync(name, Buffer.from(d.result.data, 'base64')); };
const r = readFileSync('../../build/gui-release/m100r1/results.json', 'utf-8'), s = readFileSync('../../build/gui-release/m100r1/stations.json', 'utf-8');
await ev(`(async()=>{const m=await import('/src/results/stations.ts'); const u=window.__kopruq.useUi; u.getState().finishRun({runId:1,exit:0,cancelled:false,seconds:1,outputPath:'x'}, ${JSON.stringify(r)}, null); u.setState({stations:m.parseStations(${JSON.stringify(s)}), stationsNote:null}); u.getState().setResultView({caseIndex:0,contour:'My',deformed:true,scale:300}); return 1})()`);
await shot('../docs/v2-phase3-my.png');
await ev(`window.__kopruq.useUi.getState().setUnitSystem('Ton.m')`); await shot('../docs/v2-phase3-tonm.png');
await ev(`window.__kopruq.useUi.getState().setUnitSystem('kN.m'); window.__kopruq.useUi.getState().setResultView({contour:'UZ',deformed:false})`); await shot('../docs/v2-phase3-uz.png');
ws.close(); edge.kill();
