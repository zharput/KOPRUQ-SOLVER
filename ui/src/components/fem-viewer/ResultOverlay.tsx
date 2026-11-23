import { useState } from 'react';
import { availability, CONTOUR_LABEL, type ContourId, FORCE_CONTOURS, legendTicks } from '../../model/contourField';
import { rampColor } from '../../model/displaced';
import { useUi } from '../../state/store';
import { UNIT_SYSTEMS } from '../../units/units';

export interface LegendInfo { name: string; unit: string; dataMin: number; dataMax: number; lo: number; hi: number }
const stops = Array.from({ length: 11 }, (_, i) => { const c = rampColor(1 - i / 10); return `rgb(${c[0]},${c[1]},${c[2]}) ${(i / 10) * 100}%`; }).join(','); // top = max
export const fmtTick = (v: number) => (v === 0 ? '0' : Math.abs(v) < 1e-3 || Math.abs(v) >= 1e6 ? v.toExponential(2) : Number(v.toPrecision(4)).toString());

/** Top control bar: load case / combination, result type, display mode, deformation scale. */
export function ViewerControls() {
  const { results, stations, resultView, setResultView } = useUi();
  if (!results || !results.cases.length) return null;
  const c = results.cases[Math.min(resultView.caseIndex, results.cases.length - 1)];
  const sc = stations?.cases.find((x) => x.name === c.name && x.kind === c.kind);
  const opt = (id: ContourId) => { const a = availability(id, results, sc); return <option key={id} value={id} disabled={!a.ok} title={a.why}>{CONTOUR_LABEL[id]}{a.ok ? '' : ' (n/a)'}</option>; };
  return (
    <div className="vctl" data-testid="viewer-controls">
      <label>{results.cases.some((x) => x.kind === 'combination') ? 'Load Case / Combination' : results.kind === 'modal' ? 'Mode' : 'Load Case'}
        <select aria-label="Result case" value={resultView.caseIndex} onChange={(e) => setResultView({ caseIndex: +e.target.value })}>
          {results.cases.map((k, i) => <option key={i} value={i}>{k.name}{k.kind === 'combination' ? ' (comb.)' : ''}</option>)}</select></label>
      <label>Result <select aria-label="Result type" value={resultView.contour} onChange={(e) => setResultView({ contour: e.target.value as ContourId })}>
        {opt('none')}<optgroup label="Displacement">{(['U', 'UX', 'UY', 'UZ'] as ContourId[]).map(opt)}</optgroup><optgroup label="Member forces">{FORCE_CONTOURS.map(opt)}</optgroup></select></label>
      <label>Display <select aria-label="Display mode" value={resultView.deformed ? 'def' : 'und'} onChange={(e) => setResultView({ deformed: e.target.value === 'def' })}>
        <option value="und">Undeformed</option><option value="def">Deformed shape</option></select></label>
      {resultView.deformed && <label>Scale <input aria-label="Deformation scale" type="number" min={0} step={1} value={resultView.scale} onChange={(e) => setResultView({ scale: Math.max(0, +e.target.value || 0) })} />×</label>}
      {c.note && <span className="muted vnote" title={c.note}>ⓘ {c.peak ? 'peak magnitudes' : 'see note'}</span>}
    </div>
  );
}

/** Vertical colour legend at the right edge: name, unit, max at the top, ticks, automatic/manual range. */
function LegendPanel({ legend }: { legend: LegendInfo }) {
  const { resultView, setResultView } = useUi();
  const span = legend.hi - legend.lo, ticks = legendTicks(legend.lo, legend.hi);
  return (
    <div className="legendv" data-testid="legend" aria-label="Result legend">
      <div className="lname"><b>{legend.name}</b> <span className="muted">{legend.unit}</span></div>
      <div className="lbody">
        <div className="lbar" style={{ background: `linear-gradient(to bottom,${stops})` }} />
        <div className="lticks">{ticks.map((t, i) => <span key={i} style={{ bottom: `${span > 0 ? ((t - legend.lo) / span) * 100 : 0}%` }}>{fmtTick(t)}</span>)}</div>
      </div>
      <label className="lrange"><input type="checkbox" aria-label="Automatic range" checked={resultView.rangeAuto} onChange={(e) => setResultView(e.target.checked ? { rangeAuto: true } : { rangeAuto: false, rangeMin: legend.lo, rangeMax: legend.hi })} /> Auto range</label>
      {!resultView.rangeAuto && <div className="lman">
        <label>Max <input aria-label="Range maximum" type="number" step="any" value={resultView.rangeMax} onChange={(e) => setResultView({ rangeMax: +e.target.value })} /></label>
        <label>Min <input aria-label="Range minimum" type="number" step="any" value={resultView.rangeMin} onChange={(e) => setResultView({ rangeMin: +e.target.value })} /></label></div>}
    </div>
  );
}

/** Clickable active unit system, bottom right of the viewer. */
function UnitPicker() {
  const { unitSystem, setUnitSystem } = useUi(); const [open, setOpen] = useState(false);
  return (
    <div className="unitpick">
      {open && <div className="umenu" role="menu">{UNIT_SYSTEMS.map((u) => <button key={u} role="menuitemradio" aria-checked={u === unitSystem} className={u === unitSystem ? 'on' : ''} onClick={() => { setUnitSystem(u); setOpen(false); }}>{u === unitSystem ? '✓ ' : ''}{u}</button>)}</div>}
      <button className="ubtn" aria-label="Unit system" aria-haspopup="menu" aria-expanded={open} onClick={() => setOpen(!open)}>Units: {unitSystem} ✎</button>
    </div>
  );
}

export function ViewerOverlays({ legend }: { legend: LegendInfo | null }) {
  const results = useUi((s) => s.results);
  return <>{results && <ViewerControls />}{legend && <LegendPanel legend={legend} />}<UnitPicker /></>;
}
