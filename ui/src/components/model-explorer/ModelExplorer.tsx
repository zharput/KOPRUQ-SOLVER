import { useState } from 'react';
import { useUi } from '../../state/store';

function Row({ label, count, depth = 0, open, onToggle, dim, sel, onClick, title }: {
  label: string; count?: number; depth?: number; open?: boolean; onToggle?: () => void; dim?: boolean; sel?: boolean; onClick?: () => void; title?: string;
}) {
  return (
    <div className={`trow${sel ? ' sel' : ''}${dim ? ' dim' : ''}`} style={{ paddingLeft: 4 + depth * 14 }} title={title} onClick={() => { onToggle?.(); onClick?.(); }}>
      <span style={{ width: 10 }}>{onToggle ? (open ? '▾' : '▸') : ''}</span><span>{label}</span>
      {count !== undefined && <span className="n">{count}</span>}
    </div>
  );
}

export function ModelExplorer() {
  const { view, selection, select, search, setSearch, loadSample, requestOpen } = useUi();
  const [open, setOpen] = useState<Record<string, boolean>>({ Model: true, Elements: true });
  if (!view) return (
    <div className="empty">No model loaded.<br /><br />
      <button className="tbtn" onClick={requestOpen}>Open model view JSON…</button>{' '}
      <button className="tbtn" onClick={() => void loadSample()}>Load sample (model 100)</button></div>);
  const m = view.model; const t = (k: string) => () => setOpen((o) => ({ ...o, [k]: !o[k] }));
  const q = search.trim().toLowerCase();
  const nodes = m.nodes.filter((n) => !q || String(n.id).includes(q));
  const elems = m.beams.filter((e) => !q || String(e.id).includes(q) || `section ${e.section}`.includes(q));
  return (
    <div>
      <input className="search" placeholder="Search id / 'section n'…" value={search} onChange={(e) => setSearch(e.target.value)} aria-label="Search model" />
      <Row label="Model" open={open.Model} onToggle={t('Model')} />
      {open.Model && (<>
        <Row label="Nodes" count={m.nodes.length} depth={1} open={open.Nodes} onToggle={t('Nodes')} />
        {open.Nodes && nodes.map((n) => <Row key={n.id} label={`N${n.id}`} depth={2} sel={selection?.kind === 'node' && selection.id === n.id} onClick={() => select({ kind: 'node', id: n.id })} />)}
        <Row label="Elements" count={m.beams.length + (m.trusses?.length ?? 0)} depth={1} open={open.Elements} onToggle={t('Elements')} />
        {open.Elements && (<>
          <Row label="Beam" count={m.beams.length} depth={2} open={open.Beam} onToggle={t('Beam')} />
          {open.Beam && elems.map((e) => <Row key={e.id} label={`E${e.id}  section ${e.section}`} depth={3} sel={selection?.kind === 'element' && selection.id === e.id} onClick={() => select({ kind: 'element', id: e.id })} />)}
          <Row label="Truss" count={m.trusses?.length ?? 0} depth={2} dim={!m.trusses?.length} />
          <Row label="Shell" count={0} depth={2} dim title="Not supported by the solver" />
          <Row label="Solid" count={0} depth={2} dim title="Not supported by the solver" />
        </>)}
        <Row label="Materials" count={m.materials.length} depth={1} />
        <Row label="Sections" count={m.sections.length} depth={1} />
        <Row label="Supports" count={m.supports.length} depth={1} />
        <Row label="Elastic Links" count={m.springs?.length ?? 0} depth={1} dim={!m.springs?.length} />
        <Row label="Rigid Links" count={m.rigid_links?.length ?? 0} depth={1} dim={!m.rigid_links?.length} />
        <Row label="Load Cases" count={m.load_cases?.length ?? 0} depth={1} dim={!m.load_cases?.length} />
        <Row label="Load Combinations" count={m.combinations?.length ?? 0} depth={1} dim={!m.combinations?.length} />
        <Row label="Analysis Cases" count={0} depth={1} dim title="Analysis is connected in Phase 4" />
      </>)}
    </div>
  );
}
