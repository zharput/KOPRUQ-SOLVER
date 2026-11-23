import { useState } from 'react';

// Menu structure from the requirements. Every entry is disabled: no command is implemented in
// Phase 2, and a disabled entry carries the phase that will provide it instead of pretending to work.
const MENUS: Record<string, [string, string][]> = {
  File: [['New Model', 'Phase 4'], ['Open JSON…', 'Phase 4'], ['Save', 'Phase 4'], ['Export Results…', 'Phase 5']],
  Model: [['Nodes', 'Phase 3'], ['Elements', 'Phase 3'], ['Check Model', 'Phase 4']],
  Properties: [['Materials', 'Phase 3'], ['Sections', 'Phase 3']],
  Boundary: [['Supports', 'Phase 3'], ['Elastic Links', 'Phase 3'], ['Rigid Links', 'Phase 3']],
  Loads: [['Load Cases', 'Phase 3'], ['Load Combinations', 'Phase 3']],
  Analysis: [['Linear Static', 'Phase 4'], ['Modal', 'Phase 4'], ['Response Spectrum', 'Phase 4']],
  Results: [['Displacements', 'Phase 5'], ['Reactions', 'Phase 5'], ['Element Forces', 'Phase 5'], ['Diagrams', 'Phase 5']],
  Verification: [['Select Test Model…', 'Phase 6'], ['Select Reference Results…', 'Phase 6'], ['Run & Compare', 'Phase 6'], ['Batch Verification', 'Phase 6']],
  Tools: [['Options', 'later']],
  Help: [['About', 'later']],
};

export function MenuBar() {
  const [open, setOpen] = useState<string | null>(null);
  return (
    <nav className="menubar" aria-label="Main menu" onMouseLeave={() => setOpen(null)}>
      <span className="brand">KOPRUQ-SOLVER</span>
      {Object.entries(MENUS).map(([name, items]) => (
        <div key={name} className={`menu${open === name ? ' open' : ''}`}>
          <button onClick={() => setOpen(open === name ? null : name)} onMouseEnter={() => open && setOpen(name)}>{name}</button>
          {open === name && (
            <div className="dropdown" role="menu">
              {items.map(([label, phase]) => (
                <button key={label} role="menuitem" disabled title={`Not implemented yet (${phase})`}>
                  <span>{label}</span><span className="tag">{phase}</span>
                </button>
              ))}
            </div>
          )}
        </div>
      ))}
    </nav>
  );
}
