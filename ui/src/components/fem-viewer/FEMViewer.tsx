// Placeholder. The real viewer (vtk.js or three.js, decided by the Phase 3 evaluation) is not
// implemented; the panel says so instead of drawing a fake model.
export function FEMViewer() {
  return (
    <div className="viewer" data-testid="fem-viewer">
      <div className="note">
        <div style={{ fontSize: 15, color: 'var(--text)', marginBottom: 6 }}>3D FEM Viewer</div>
        Not implemented yet (Phase 3). It will draw nodes, beam elements, sections, supports, loads and results from a KOPRUQ model.
      </div>
    </div>
  );
}
