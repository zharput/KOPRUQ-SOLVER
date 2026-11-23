import type { BottomTab } from '../../state/store';

export function ResultsViewer({ tab }: { tab: Exclude<BottomTab, 'Verification'> }) {
  return <div className="empty" data-testid="results-empty">{tab}: no results. Run an analysis (Phase 4) to fill this view (Phase 5).</div>;
}
