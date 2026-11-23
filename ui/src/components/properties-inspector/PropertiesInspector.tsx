import { mockElements, mockNodes } from '../../mock/demoModel';
import { useUi } from '../../state/store';

const dash = '—';
function KV({ rows }: { rows: [string, string][] }) {
  return <table className="kv"><tbody>{rows.map(([k, v]) => <tr key={k}><td>{k}</td><td>{v}</td></tr>)}</tbody></table>;
}

export function PropertiesInspector() {
  const sel = useUi((s) => s.selection);
  if (!sel) return <div className="empty">Nothing selected. Pick a node or element in the Model Explorer.</div>;
  if (sel.kind === 'node') {
    const n = mockNodes.find((x) => x.id === sel.id);
    if (!n) return null;
    return <KV rows={[['ID', `N${n.id}`], ['X (m)', n.x.toFixed(3)], ['Y (m)', n.y.toFixed(3)], ['Z (m)', n.z.toFixed(3)], ['Restraints', dash]]} />;
  }
  const e = mockElements.find((x) => x.id === sel.id);
  if (!e) return null;
  const a = mockNodes.find((x) => x.id === e.n1)!, b = mockNodes.find((x) => x.id === e.n2)!;
  const len = Math.hypot(b.x - a.x, b.y - a.y, b.z - a.z);
  return <KV rows={[['ID', `E${e.id}`], ['Type', e.type], ['Material', e.material], ['Section', e.section], ['Length (m)', len.toFixed(3)],
    ['Connectivity', `N${e.n1} – N${e.n2}`], ['Beta (deg)', e.beta.toFixed(1)], ['Local axes', dash], ['Releases', dash], ['Offsets', dash]]} />;
}
