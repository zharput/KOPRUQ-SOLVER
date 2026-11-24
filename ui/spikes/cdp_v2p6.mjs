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
const shot = async (name) => { await sleep(1800); const d = await send('Page.captureScreenshot', { format: 'png' }); writeFileSync(name, Buffer.from(d.result.data, 'base64')); };
const r = readFileSync('../../build/gui-release/m100r1/results.json', 'utf-8');
await ev(`(()=>{const u=window.__kopruq.useUi; u.getState().finishRun({runId:1,exit:0,cancelled:false,seconds:1,outputPath:'x'}, ${JSON.stringify(r)}, null); u.getState().setCenterTab('tables'); u.getState().setTableTab('Element Forces'); u.getState().select({kind:'element',id:85}); return 1})()`);
await shot('../docs/v2-phase6-sync.png');
console.log(await ev(`(()=>{const tr=document.querySelector('tr.active'); const box=document.querySelector('.rscroll'); return JSON.stringify({row: tr?.getAttribute('data-row-id'), scrollTop: box.scrollTop})})()`));
ws.close(); edge.kill();
