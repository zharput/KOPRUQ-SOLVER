# KOPRUQ-SOLVER UI (React) — Phase 2: GUI foundation

React + TypeScript + Vite front end for KOPRUQ-SOLVER. **This phase is a skeleton only**: layout,
menus, toolbar and panels. Nothing is connected to the C++ solver yet, and every unimplemented
command is disabled (with a tooltip naming the phase that will provide it). The model shown in the
Model Explorer is **MOCK data** (`src/mock/demoModel.ts`), marked as such in the toolbar and status bar.
The existing C++ core, JSON schemas, CLI and the Qt6 GUI are untouched.

![viewer](docs/viewer-iso.png)

## Phase 3 status: FEM viewer (vtk.js)

Reads a **`kopruq-model-view`** JSON written by the solver CLI (no analysis involved):

```
kopruq_solver_cli inspect model.json -o view.json                       # native kopruq-model
kopruq_solver_cli inspect midas.json --format midas-nx-json -o view.json # MIDAS NX export
```

Then **Open** in the toolbar / File menu (or **Sample**, a real inspect output of verification model 100;
`http://127.0.0.1:5173/?sample` opens it at start-up).

Working: members drawn as section solids (section outlines extruded along the solver's own member axes,
offsets applied) or as lines, nodes, support glyphs, elastic and rigid links, camera presets (Iso, Front,
Top, Side, Fit; drag = rotate, Shift+drag = pan, wheel = zoom), click selection of elements, nodes and
supports (GPU picking) synchronised with the Model Explorer and Properties, show/hide toggles.
Not implemented (disabled, with the phase named): loads, local axes, deformed shape and result colouring
(Phase 5), analysis (Phase 4), verification (Phase 6), editing.

Tests: `npm test` -- geometry (member axes, ear clipping, extrusion volumes, support glyphs, all on the
real model-100 data) and shell behaviour. Click picking in a real browser was checked with a DevTools
script (`spikes/cdp_pick.mjs`, 16/16 on model 100); viewer timings are in `docs/viewer-evaluation.md`.

## Phase 4 status: desktop app (Tauri 2) connected to the C++ solver

```
cd ui
npm install
npm run desktop            # tauri dev: Vite + native window (needs Rust, WebView2)
```

The app starts the existing `kopruq_solver_cli` as a child process (no shell; argument vector built
from validated fields), streams its `--progress json` events and stderr into the Analysis panel and the
**Log** tab, and can cancel it. Where the CLI is looked up: `KOPRUQ_SOLVER_CLI`, else next to the app
executable (packaged), else `build/release|gui-release|debug` of this repository (development) --
build the CLI first (`scripts/build.ps1`).

Flow: **Open** picks a MIDAS NX JSON / kopruq-model / kopruq-model-view file; MIDAS and native models
go through `kopruq_solver_cli inspect` and appear in the viewer; **Run Analysis** runs Linear Static
(`solve`), Modal or Response Spectrum on the opened model. Status shows what the CLI reports (read /
solve / write). "Completed" appears only when the CLI exited 0 *and* its result file says
`status.success`; otherwise the failure reason is shown. Result values are not displayed yet (Phase 5).
In a plain browser the analysis controls stay disabled ("Desktop app required").

Security: the window can only call four app commands (solver_info, read_text_file for `.json` files,
run_solver with a fixed command set, cancel_solver) and the open-file dialog; no fs/shell/http access.

## Layout

| Area | Component |
|---|---|
| Top menu, toolbar, status bar | `src/app/MenuBar.tsx`, `Toolbar.tsx`, `Shell.tsx` |
| Left: tree + search | `components/model-explorer` |
| Left bottom: properties | `components/properties-inspector` |
| Centre: 3D viewer | `components/fem-viewer` (vtk.js), pure geometry in `src/model/geometry.ts` |
| Right: analysis controls | `components/analysis-controls` |
| Bottom: result tabs + Verification | `components/results-viewer`, `components/verification-panel` |

Reusable components import nothing from Tauri; the outside world is reached only through
`src/platform/SolverHost.ts` (Tauri implementation: Phase 4).

## Phases

2 foundation (done) · 3 FEM viewer (this) · 4 solver connection through Tauri + new CLI commands ·
5 results · 6 verification workspace (MIDAS NX comparison stays in C++) · 7 packaging · 8 Qt6 retirement plan.
