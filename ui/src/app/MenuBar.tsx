import { useState } from 'react';
import { useUi } from '../state/store';

// Entries without an action are disabled and name the phase that will provide them; the few that work
// today (opening a model-view file) are enabled. Nothing pretends to run an analysis.
type Item = [label: string, phase: string, action?: 'open' | 'sample'];
const MENUS: Record<string, Item[]> = {
  File: [['Open Model View JSON…', '', 'open'], ['Open Sample Model (model 100)', '', 'sample'], ['New Model', 'later'], ['Save', 'later'], ['Export Results…', 'Phase 5']],
  Model: [['Nodes', 'later'], ['Elements', 'later'], ['Check Model', 'Phase 4']],
  Properties: [['Materials', 'later'], ['Sections', 'later']],
  Boundary: [['Supports', 'later'], ['Elastic Links', 'later'], ['Rigid Links', 'later']],
  Loads: [['Load Cases', 'Phase 5'], ['Load Combinations', 'Phase 5']],
  Analysis: [['Linear Static', 'Phase 4'], ['Modal', 'Phase 4'], ['Response Spectrum', 'Phase 4']],
  Results: [['Displacements', 'Phase 5'], ['Reactions', 'Phase 5'], ['Element Forces', 'Phase 5'], ['Diagrams', 'Phase 5']],
  Verification: [['Select Test Model…', 'Phase 6'], ['Select Reference Results…', 'Phase 6'], ['Run & Compare', 'Phase 6'], ['Batch Verification', 'Phase 6']],
  Tools: [['Options', 'later']],
  Help: [['About', 'later']],
};

export function MenuBar() {
  const [open, setOpen] = useState<string | null>(null);
  const { requestOpen, loadSample } = useUi();
  const run = (a?: 'open' | 'sample') => { setOpen(null); if (a === 'open') requestOpen(); if (a === 'sample') void loadSample(); };
  return (
    <nav className="menubar" aria-label="Main menu" onMouseLeave={() => setOpen(null)}>
      <span className="brand">KOPRUQ-SOLVER</span>
      {Object.entries(MENUS).map(([name, items]) => (
        <div key={name} className={`menu${open === name ? ' open' : ''}`}>
          <button onClick={() => setOpen(open === name ? null : name)} onMouseEnter={() => open && setOpen(name)}>{name}</button>
          {open === name && (
            <div className="dropdown" role="menu">
              {items.map(([label, phase, action]) => (
                <button key={label} role="menuitem" disabled={!action} onClick={() => run(action)} title={action ? undefined : `Not implemented yet (${phase})`}>
                  <span>{label}</span><span className="tag">{action ? '' : phase}</span>
                </button>
              ))}
            </div>
          )}
        </div>
      ))}
    </nav>
  );
}
