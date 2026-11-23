import { BarChart, LineChart } from 'echarts/charts';
import { GridComponent, MarkLineComponent, TooltipComponent } from 'echarts/components';
import * as echarts from 'echarts/core';
import { CanvasRenderer } from 'echarts/renderers';
import { useEffect, useMemo, useRef } from 'react';
import { cleanForPlot, diagramPoints, FORCE_LABELS, maxClosure, rowsScale } from '../../results/stations';
import { useUi } from '../../state/store';

echarts.use([LineChart, BarChart, GridComponent, TooltipComponent, MarkLineComponent, CanvasRenderer]);

function Chart({ title, points, length }: { title: string; points: [number, number][]; length: number }) {
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const el = ref.current; if (!el) return; const c = echarts.init(el, undefined, { renderer: 'canvas' });
    c.setOption({ animation: false, grid: { left: 52, right: 12, top: 22, bottom: 22 }, title: { text: title, left: 6, top: 0, textStyle: { color: '#cbd5e1', fontSize: 11, fontWeight: 'normal' } },
      tooltip: { trigger: 'axis', valueFormatter: (v: unknown) => (typeof v === 'number' ? v.toPrecision(6) : String(v)) },
      xAxis: { type: 'value', min: 0, max: length, axisLabel: { color: '#8aa0b8', fontSize: 10 }, splitLine: { show: false } },
      yAxis: { type: 'value', scale: true, ...(points.every((p) => p[1] === 0) ? { min: -1, max: 1 } : {}), axisLabel: { color: '#8aa0b8', fontSize: 10, formatter: (v: number) => Number(v.toPrecision(4)).toString() }, splitLine: { lineStyle: { color: '#1e2d45' } } },
      series: [{ type: 'line', data: points, showSymbol: points.length < 40, symbolSize: 4, lineStyle: { color: '#38bdf8', width: 1.6 }, itemStyle: { color: '#38bdf8' }, areaStyle: { color: 'rgba(56,189,248,.12)' }, markLine: { silent: true, symbol: 'none', lineStyle: { color: '#475569' }, data: [{ yAxis: 0 }], label: { show: false } } }] });
    const ro = new ResizeObserver(() => c.resize()); ro.observe(el);
    return () => { ro.disconnect(); c.dispose(); };
  }, [title, points, length]);
  return <div ref={ref} className="chart" aria-label={title} />;
}

/** N, Vy, Vz, T, My, Mz along the selected member (click a member in the viewer or the explorer) for the selected result case. */
export function ForceDiagram() {
  const { stations, stationsNote, results, resultView, selection, view } = useUi();
  const c = results?.cases[Math.min(resultView.caseIndex, (results?.cases.length ?? 1) - 1)];
  const sc = useMemo(() => (c && stations ? stations.cases.find((x) => x.name === c.name && x.kind === c.kind) : undefined), [c, stations]);
  if (!results) return <div className="empty" data-testid="diagrams-na">Diagrams: no results. Run a Linear Static analysis in the desktop app.</div>;
  if (results.kind !== 'solve') return <div className="empty" data-testid="diagrams-na">Force diagrams are produced for Linear Static analyses only (modal shapes carry no forces; spectrum values are peak magnitudes without equilibrium).</div>;
  if (!stations || !sc) return <div className="empty" data-testid="diagrams-na">{stationsNote ?? 'Station results are not available for this case.'}</div>;
  const id = selection?.kind === 'element' && sc.elements.has(selection.id) ? selection.id : (view?.model.beams[0]?.id ?? [...sc.elements.keys()][0]);
  const el = sc.elements.get(id);
  if (!el) return <div className="empty" data-testid="diagrams-na">No station data for element {id}.</div>;
  const cl = maxClosure(sc);
  return (
    <div className="resbox" data-testid="diagrams">
      <div className="casebar"><b>Element {el.id}</b><span className="muted">L = {el.length.toPrecision(6)} m · case {sc.name}{selection?.kind === 'element' ? '' : ' · select an element to change'}</span>
        <span className="muted" title={stations.convention}>Solver local axes; moments are plotted as reported (no sign inversion). Equilibrium closure ≤ {Math.max(cl.force, cl.moment).toExponential(1)}</span></div>
      <div className="charts">{FORCE_LABELS.map((l, k) => <Chart key={l} title={l} points={cleanForPlot(diagramPoints(el.rows, k), rowsScale(el.rows))} length={el.length} />)}</div>
    </div>
  );
}
