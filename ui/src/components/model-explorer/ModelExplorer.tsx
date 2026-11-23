import { useState } from 'react';
import { mockElements, mockLoadCases, mockMaterials, mockNodes, mockSections } from '../../mock/demoModel';
import { useUi } from '../../state/store';

function Row({ label, count, depth = 0, open, onToggle, dim, sel, onClick, title }: {
  label: string; count?: number; depth?: number; open?: boolean; onToggle?: () => void; dim?: boolean; sel?: boolean; onClick?: () => void; title?: string;
}) {
  return (
    <div className={`trow${sel ? ' sel' : ''}${dim ? ' dim' : ''}`} style={{ paddingLeft: 4 + depth * 14 }} title={title}
         onClick={() => { onToggle?.(); onClick?.(); }}>
      <span style={{ width: 10 }}>{onToggle ? (open ? '▾' : '▸') : ''}</span><span>{label}</span>
      {count !== undefined && <span className="n">{count}</span>}
    </div>
  );
}

export function ModelExplorer() {
  const { selection, select, search, setSearch } = useUi();
  const [open, setOpen] = useState<Record<string, boolean>>({ Model: true, Elements: true });
  const t = (k: string) => () => setOpen((o) => ({ ...o, [k]: !o[k] }));
  const q = search.trim();
  const nodes = mockNodes.filter((n) => !q || String(n.id).includes(q));
  const elems = mockElements.filter((e) => !q || String(e.id).includes(q) || e.section.toLowerCase().includes(q.toLowerCase()));
  return (
    <div>
      <input className="search" placeholder="Search id / section…" value={search} onChange={(e) => setSearch(e.target.value)} aria-label="Search model" />
      <Row label="Model" open={open.Model} onToggle={t('Model')} />
      {open.Model && (<>
        <Row label="Nodes" count={mockNodes.length} depth={1} open={open.Nodes} onToggle={t('Nodes')} />
        {open.Nodes && nodes.map((n) => (
          <Row key={n.id} label={`N${n.id}`} depth={2} sel={selection?.kind === 'node' && selection.id === n.id} onClick={() => select({ kind: 'node', id: n.id })} />))}
        <Row label="Elements" count={mockElements.length} depth={1} open={open.Elements} onToggle={t('Elements')} />
        {open.Elements && (<>
          <Row label="Beam" count={mockElements.length} depth={2} open={open.Beam} onToggle={t('Beam')} />
          {open.Beam && elems.map((e) => (
            <Row key={e.id} label={`E${e.id}  ${e.section}`} depth={3} sel={selection?.kind === 'element' && selection.id === e.id} onClick={() => select({ kind: 'element', id: e.id })} />))}
          <Row label="Truss" count={0} depth={2} dim title="Supported by the solver; import into this tree arrives with Phase 3" />
          <Row label="Shell" count={0} depth={2} dim title="Not supported by the solver" />
          <Row label="Solid" count={0} depth={2} dim title="Not supported by the solver" />
        </>)}
        <Row label="Materials" count={mockMaterials.length} depth={1} />
        <Row label="Sections" count={mockSections.length} depth={1} />
        <Row label="Supports" count={0} depth={1} dim title="Read from the model in Phase 3" />
        <Row label="Elastic Links" count={0} depth={1} dim title="Read from the model in Phase 3" />
        <Row label="Load Cases" count={mockLoadCases.length} depth={1} />
        <Row label="Load Combinations" count={0} depth={1} dim />
        <Row label="Analysis Cases" count={0} depth={1} dim title="Moving-load analysis is not supported by the solver" />
      </>)}
    </div>
  );
}
