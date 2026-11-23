import { AnalysisControls } from '../components/analysis-controls/AnalysisControls';
import { FEMViewer } from '../components/fem-viewer/FEMViewer';
import { ModelExplorer } from '../components/model-explorer/ModelExplorer';
import { PropertiesInspector } from '../components/properties-inspector/PropertiesInspector';
import { ResultsViewer } from '../components/results-viewer/ResultsViewer';
import { VerificationPanel } from '../components/verification-panel/VerificationPanel';
import { useEffect, useRef } from 'react';
import { type BottomTab, useUi } from '../state/store';
import { MenuBar } from './MenuBar';
import { Toolbar } from './Toolbar';
import { useSplit } from './useSplit';

const TABS: BottomTab[] = ['Results', 'Displacements', 'Reactions', 'Element Forces', 'Diagrams', 'Tables', 'Verification'];

function Panel({ title, children }: { title: string; children: React.ReactNode }) {
  return <section className="panel" aria-label={title}><div className="head"><b>{title}</b></div><div className="body">{children}</div></section>;
}

export function Shell() {
  const left = useSplit(270, 180, 500, 'x');
  const right = useSplit(280, 200, 480, 'x', true);
  const bottom = useSplit(230, 90, 520, 'y', true);
  const leftSplit = useSplit(300, 120, 700, 'y');
  const { bottomTab, setBottomTab, view, fileName, loadError, loadText, openRequest } = useUi();
  const fileInput = useRef<HTMLInputElement>(null);
  useEffect(() => { if (openRequest > 0) fileInput.current?.click(); }, [openRequest]);
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
        <div style={{ display: 'grid', gridTemplateRows: `1fr 5px ${bottom.size}px`, minHeight: 0, minWidth: 0 }}>
          <section className="panel" aria-label="3D Model"><div className="head"><b>3D Model</b></div><FEMViewer /></section>
          <div className="splitter-h" onPointerDown={bottom.onPointerDown} role="separator" />
          <section className="panel" aria-label="Results panel">
            <div className="head" role="tablist">
              {TABS.map((t) => <button key={t} role="tab" aria-selected={bottomTab === t} className={`tab${bottomTab === t ? ' active' : ''}`} onClick={() => setBottomTab(t)}>{t}</button>)}
            </div>
            <div className="body">{bottomTab === 'Verification' ? <VerificationPanel /> : <ResultsViewer tab={bottomTab} />}</div>
          </section>
        </div>
        <div className="splitter-v" onPointerDown={right.onPointerDown} role="separator" aria-orientation="vertical" />
        <Panel title="Analysis"><AnalysisControls /></Panel>
      </div>
      <footer className="statusbar">
        <span><span className="dot" style={{ background: 'var(--warn)' }} />No solver connected</span>
        <span>Model: {view ? fileName : 'none'}</span><span>Nodes: {view?.model.nodes.length ?? 0}</span><span>Elements: {view?.model.beams.length ?? 0}</span><span>Analysis: none</span>
      </footer>
    </div>
  );
}
