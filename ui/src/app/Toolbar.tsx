import { MOCK_LABEL } from '../mock/demoModel';

// All tools are disabled in Phase 2 (no viewer, no model I/O yet); the tooltip says which phase
// provides each. Grouping follows the requirements: file, edit, interaction, camera, display, result.
const GROUPS: [string, string][][] = [
  [['New', 'Phase 4'], ['Open', 'Phase 4'], ['Save', 'Phase 4']],
  [['Undo', 'later'], ['Redo', 'later']],
  [['Select', 'Phase 3'], ['Pan', 'Phase 3'], ['Rotate', 'Phase 3'], ['Zoom', 'Phase 3'], ['Fit', 'Phase 3']],
  [['Iso', 'Phase 3'], ['Front', 'Phase 3'], ['Top', 'Phase 3'], ['Side', 'Phase 3']],
  [['Wire/Solid', 'Phase 3'], ['Nodes', 'Phase 3'], ['Elements', 'Phase 3'], ['Supports', 'Phase 3'], ['Loads', 'Phase 3'], ['Axes', 'Phase 3']],
  [['Deformed', 'Phase 5']],
];

export function Toolbar() {
  return (
    <div className="toolbar" role="toolbar" aria-label="Main toolbar">
      {GROUPS.map((g, i) => (
        <span key={i} style={{ display: 'contents' }}>
          {i > 0 && <span className="sep" />}
          {g.map(([label, phase]) => (
            <button key={label} className="tbtn" disabled title={`Not implemented yet (${phase})`}>{label}</button>
          ))}
        </span>
      ))}
      <span className="mockbanner" title="The model shown is placeholder data, not a real KOPRUQ-SOLVER model">{MOCK_LABEL} DATA — not a real model</span>
    </div>
  );
}
