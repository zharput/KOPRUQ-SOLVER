import { AnalysisControls } from '../components/analysis-controls/AnalysisControls';
import { FEMViewer } from '../components/fem-viewer/FEMViewer';
import { ModelExplorer } from '../components/model-explorer/ModelExplorer';
import { PropertiesInspector } from '../components/properties-inspector/PropertiesInspector';
import { TablesWorkspace } from '../components/results-viewer/TablesWorkspace';
import { useEffect, useRef } from 'react';
import { pickAndOpen } from '../state/runner';
import { useUi } from '../state/store';
import { MenuBar } from './MenuBar';
import { Toolbar } from './Toolbar';
import { useSplit } from './useSplit';

function Panel({ title, children }: { title: string; children: React.ReactNode }) {
  return <section className="panel" aria-label={title}><div className="head"><b>{title}</b></div><div className="body">{children}</div></section>;
}

export function Shell() {
  const left = useSplit(270, 180, 500, 'x');
  const right = useSplit(280, 200, 480, 'x', true);
  const leftSplit = useSplit(300, 120, 700, 'y');
  const { centerTab, setCenterTab, view, fileName, loadError, loadText, openRequest, host, solver, run } = useUi();
  const fileInput = useRef<HTMLInputElement>(null);
  useEffect(() => { if (openRequest === 0) return; if (host.kind === 'tauri') void pickAndOpen(); else fileInput.current?.click(); }, [openRequest]); // eslint-disable-line
  const onFile = async (f?: File) => { if (f) loadText(await f.text(), f.name); if (fileInput.current) fileInput.current.value = ''; };
  return (
    <div className="shell">
      <MenuBar />
      <Toolbar />
      <input ref={fileInput} type="file" accept=".json,application/json" hidden aria-label="Open model view file" onChange={(e) => void onFile(e.target.files?.[0])} />
      {loadError ? <div className="errbar" role="alert"><span>{loadError}</span><button className="tbtn" onClick={() => useUi.setState({ loadError: null })}>Dismiss</button></div> : <div />}
      <div className="workspace" style={{ gridTemplateColumns: `${left.size}px 5px 1fr 5px ${right.size}px` }}>
        <div style={{ display: 'grid', gridTemplateRows: `${leftSplit.size}px 5px 1fr`, minHeight: 0 }}>
          <Panel title="Model Explorer"><ModelExplorer /></Panel>
          <div className="splitter-h" onPointerDown={leftSplit.onPointerDown} role="separator" />
          <Panel title="Properties"><PropertiesInspector /></Panel>
        </div>
        <div className="splitter-v" onPointerDown={left.onPointerDown} role="separator" aria-orientation="vertical" />
        <section className="panel" aria-label="Workspace" style={{ minWidth: 0, minHeight: 0 }}>
          <div className="head" role="tablist" aria-label="Workspace tabs">
            {([['3d', '3D Model'], ['tables', 'Tables']] as const).map(([k, l]) => <button key={k} role="tab" aria-selected={centerTab === k} className={`tab${centerTab === k ? ' active' : ''}`} onClick={() => setCenterTab(k)}>{l}</button>)}
          </div>
          {/* both views stay mounted: the WebGL scene and the table state survive tab switches */}
          <div className={`pane${centerTab === '3d' ? '' : ' hidden'}`} aria-label="3D Model"><FEMViewer /></div>
          <div className={`pane${centerTab === 'tables' ? '' : ' hidden'}`} aria-label="Tables"><TablesWorkspace /></div>
        </section>
        <div className="splitter-v" onPointerDown={right.onPointerDown} role="separator" aria-orientation="vertical" />
        <Panel title="Analysis"><AnalysisControls /></Panel>
      </div>
      <footer className="statusbar">
        <span><span className="dot" style={{ background: solver.state === 'connected' ? 'var(--ok)' : 'var(--warn)' }} />{solver.state === 'connected' ? `Solver: ${solver.version}` : 'No solver connected'}</span>
        <span>Model: {view ? fileName : 'none'}</span><span>Nodes: {view?.model.nodes.length ?? 0}</span><span>Elements: {view?.model.beams.length ?? 0}</span><span>Analysis: {run.state === 'idle' ? 'none' : run.state}</span>
      </footer>
    </div>
  );
}
