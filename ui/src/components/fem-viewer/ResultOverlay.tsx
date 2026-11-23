import { rampColor } from '../../model/displaced';
import { type Contour, useUi } from '../../state/store';

export interface Legend { label: string; min: number; max: number }
const stops = Array.from({ length: 9 }, (_, i) => { const c = rampColor(i / 8); return `rgb(${c[0]},${c[1]},${c[2]}) ${(i / 8) * 100}%`; }).join(',');
const f = (v: number) => (v === 0 ? '0' : Math.abs(v) < 1e-3 || Math.abs(v) >= 1e4 ? v.toExponential(3) : v.toPrecision(4));

/** Deformed-shape scale, contour choice and colour legend; shown only while results exist. */
export function ResultOverlay({ legend }: { legend: Legend | null }) {
  const { results, resultView, setResultView } = useUi();
  if (!results || !results.cases.length) return null;
  const c = results.cases[Math.min(resultView.caseIndex, results.cases.length - 1)];
  return (
    <div className="resov" data-testid="result-overlay">
      <div><b>{c.name}</b>{c.peak ? ' — peak magnitudes' : ''}</div>
      <label>Contour <select aria-label="Contour" value={resultView.contour} onChange={(e) => setResultView({ contour: e.target.value as Contour })}>
        <option value="none">None</option><option value="U">|U| total</option><option value="UX">UX</option><option value="UY">UY</option><option value="UZ">UZ</option></select></label>
      {resultView.deformed && <label>Scale <input aria-label="Deformation scale" type="number" min={0} step={1} value={resultView.scale} onChange={(e) => setResultView({ scale: Math.max(0, +e.target.value || 0) })} />×</label>}
      {resultView.deformed && <div className="muted">Displacements drawn ×{resultView.scale} (first-order shape)</div>}
      {legend && <div className="legend"><div className="bar" style={{ background: `linear-gradient(to right,${stops})` }} /><div className="lab"><span>{f(legend.min)}</span><span>{legend.label}</span><span>{f(legend.max)}</span></div></div>}
    </div>
  );
}
