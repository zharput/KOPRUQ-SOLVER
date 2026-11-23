import { useUi, type ShowFlags } from '../state/store';

// Working tools are enabled; everything else is disabled with the phase that will provide it.
function B({ label, onClick, on, disabled, title }: { label: string; onClick?: () => void; on?: boolean; disabled?: boolean; title?: string }) {
  return <button className={`tbtn${on ? ' on' : ''}`} onClick={onClick} disabled={disabled} aria-pressed={on} title={disabled ? `Not implemented yet${title ? ` (${title})` : ''}` : title}>{label}</button>;
}

export function Toolbar() {
  const { view, show, toggle, setCamera, requestOpen, loadSample } = useUi();
  const t = (k: keyof ShowFlags, label: string) => <B label={label} on={show[k]} onClick={() => toggle(k)} disabled={!view} title={`Show/hide ${label.toLowerCase()}`} />;
  const sep = <span className="sep" />;
  return (
    <div className="toolbar" role="toolbar" aria-label="Main toolbar">
      <B label="New" disabled title="later" /><B label="Open" onClick={requestOpen} title="Open a kopruq-model-view JSON (kopruq_solver_cli inspect)" />
      <B label="Sample" onClick={() => void loadSample()} title="Load the sample model (verification model 100)" /><B label="Save" disabled title="later" />
      {sep}<B label="Undo" disabled title="later" /><B label="Redo" disabled title="later" />
      {sep}<B label="Select" on disabled={false} title="Click an element or node to select it" />
      <B label="Pan" disabled title="use Shift+drag" /><B label="Rotate" disabled title="use drag" /><B label="Zoom" disabled title="use the wheel" />
      <B label="Fit" onClick={() => setCamera('fit')} disabled={!view} title="Fit the model in the view" />
      {sep}<B label="Iso" onClick={() => setCamera('iso')} disabled={!view} /><B label="Front" onClick={() => setCamera('front')} disabled={!view} />
      <B label="Top" onClick={() => setCamera('top')} disabled={!view} /><B label="Side" onClick={() => setCamera('side')} disabled={!view} />
      {sep}{t('solid', 'Solid')}{t('nodes', 'Nodes')}{t('elements', 'Elements')}{t('supports', 'Supports')}{t('links', 'Links')}
      <B label="Loads" disabled title="Phase 5" /><B label="Axes" disabled title="later" />
      {sep}<B label="Deformed" disabled title="Phase 5" />
    </div>
  );
}
