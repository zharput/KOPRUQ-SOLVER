// End-to-end check of the desktop app (Tauri + real C++ CLI) through the WebView2 DevTools port.
import { writeFileSync } from 'node:fs';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const list = await (await fetch('http://127.0.0.1:9340/json')).json();
const ws = new WebSocket(list.find((t) => t.type === 'page').webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pending = new Map(); ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); } };
const send = (method, params = {}) => new Promise((res) => { const i = ++id; pending.set(i, res); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (expression) => { const d = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true }); return d.result?.result?.value ?? JSON.stringify(d.result?.exceptionDetails?.exception?.description ?? d.result).slice(0, 500); };
const text = (sel) => ev(`document.querySelector('${sel}')?.innerText.replace(/\s+/g,' ')`);
const model = process.argv[2];
console.log('is tauri      :', await ev(`'__TAURI_INTERNALS__' in window`));
await sleep(1500);
console.log('solver status :', await text('[data-testid=solver-connection]'));
console.log('open model    :', await ev(`(async()=>{const k=window.__kopruq; await k.runner.openModelPath(${JSON.stringify(model)}); const s=k.useUi.getState(); return JSON.stringify({err:s.loadError, nodes:s.view?.model.nodes.length, beams:s.view?.model.beams.length, outlines:s.view?.section_outlines.length, source:s.source})})()`));
await sleep(2500);
for (const kind of ['solve', 'modal', 'spectrum']) {
  console.log(`run ${kind}`.padEnd(14), ':', await ev(`(async()=>{const k=window.__kopruq, st=k.useUi; st.getState().set({analysis:'${kind}', modes:6}); await k.runner.startAnalysis(); const x=st.getState().run; return JSON.stringify({state:x.state, stages:x.stages, seconds:x.seconds, summary:x.summary, msg:x.message, logLines:x.log.length})})()`));
}
const d = await send('Page.captureScreenshot', { format: 'png' }); writeFileSync('../docs/desktop-run.png', Buffer.from(d.result.data, 'base64'));
ws.close();
