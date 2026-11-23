const list = await (await fetch('http://127.0.0.1:9340/json')).json();
const ws = new WebSocket(list.find((t) => t.type === 'page').webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pending = new Map(); ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); } };
const ev = (expression) => new Promise((res) => { const i = ++id; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: 'Runtime.evaluate', params: { expression, awaitPromise: true, returnByValue: true } })); }).then((d) => d.result?.result?.value ?? JSON.stringify(d.result?.exceptionDetails ?? d.result).slice(0, 600));
console.log(await ev(`(async()=>{ const st=(await import('/src/state/store.ts')).useUi; const s=st.getState(); const out={host:s.host.kind, solver:s.solver.state};
  try{ out.readLen=(await s.host.readText(${JSON.stringify(process.argv[2])})).length; }catch(e){ out.readErr=String(e);} return JSON.stringify(out); })()`));
ws.close();
