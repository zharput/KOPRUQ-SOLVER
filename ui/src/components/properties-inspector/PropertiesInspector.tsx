import { elementLength } from '../../model/geometry';
import { useUi } from '../../state/store';

const dash = '—';
const f = (v: number) => v.toFixed(3);
const flags = (a?: boolean[]) => (a && a.some(Boolean) ? a.map((x, i) => (x ? ['UX', 'UY', 'UZ', 'RX', 'RY', 'RZ'][i] : '')).filter(Boolean).join(' ') : 'none');
function KV({ rows }: { rows: [string, string][] }) {
  return <table className="kv"><tbody>{rows.map(([k, v]) => <tr key={k}><td>{k}</td><td>{v}</td></tr>)}</tbody></table>;
}

export function PropertiesInspector() {
  const { selection: sel, view } = useUi();
  if (!view) return <div className="empty">No model loaded.</div>;
  if (!sel) return <div className="empty">Nothing selected. Click a node or element in the viewer or the Model Explorer.</div>;
  if (sel.kind === 'node') {
    const n = view.model.nodes.find((x) => x.id === sel.id); if (!n) return null;
    const s = view.model.supports.find((x) => x.node === sel.id);
    return <KV rows={[['ID', `N${n.id}`], ['X (m)', f(n.x)], ['Y (m)', f(n.y)], ['Z (m)', f(n.z)], ['Restraints', s ? flags(s.restraints) : 'none']]} />;
  }
  const e = view.model.beams.find((x) => x.id === sel.id); if (!e) return null;
  const len = elementLength(view, e.id);
  const off = (o?: number[]) => (o && o.some((v) => v !== 0) ? o.map((v) => v.toFixed(3)).join(', ') : 'none');
  return <KV rows={[['ID', `E${e.id}`], ['Type', 'Beam'], ['Material', String(e.material)], ['Section', String(e.section)], ['Length (m)', len == null ? dash : f(len)],
    ['Connectivity', `N${e.nodes[0]} – N${e.nodes[1]}`], ['Reference y', e.reference_y ? e.reference_y.map((v) => v.toFixed(3)).join(', ') : dash],
    ['Releases (start)', flags(e.releases?.start)], ['Releases (end)', flags(e.releases?.end)], ['Offset 1 (m)', off(e.offset1)], ['Offset 2 (m)', off(e.offset2)]]} />;
}
