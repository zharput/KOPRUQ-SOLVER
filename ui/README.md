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
`status.success`; otherwise the failure reason is shown.
In a plain browser the analysis controls stay disabled ("Desktop app required").

Security: the window can only call four app commands (solver_info, read_text_file for `.json` files,
run_solver with a fixed command set, cancel_solver) and the open-file dialog; no fs/shell/http access.

## Phase 5a/5b status: results

After a successful run the result document is parsed (`src/results/parse.ts`: kopruq-results, kopruq-modal-results,
kopruq-response-spectrum-results; failed cases are listed, never shown as numbers). Bottom tabs: **Displacements**,
**Reactions** (restrained nodes), **Element Forces** (12 local end forces exactly as the solver reports them),
**Tables** (modal frequencies / spectrum mass participation), **Results** summary; sortable windowed TanStack tables
and a case selector. The viewer's **Deformed** button draws the case's displaced shape (cubic Hermite along every member,
section rotation about the axis, rigid offsets; first-order, scale factor editable) and a colour contour (|U|, UX, UY, UZ)
with legend. Modal shapes are mass-normalised (arbitrary amplitude); spectrum values are peak magnitudes.
**Diagrams** tab (Phase 5c): after a successful *Linear Static* run the app runs `kopruq_solver_cli stations` on the same model and plots N, Vy, Vz, T, My, Mz
along the selected member (ECharts) for the selected case; the values are the solver's local convention, unconverted (`docs/station-results-design.md`).
Not yet: load arrows, local axes, element ID labels.
Checked in headless Edge with the real model-100 result file (`spikes/cdp_results.mjs`, images `docs/results-*.png`);
the desktop-shell path was not re-run after Phase 5.

## GUI V2 Phase 1: layout (2026-11-23)
The bottom results panel is gone. The centre is one workspace with two tabs, **3D Model** and **Tables**; both stay mounted so
the 3D scene, selection, results and table category survive tab switches. Result categories (Element Forces, Displacements, Reactions,
Element Stresses [disabled: the result files carry no stresses], Diagrams, Modal / Spectrum, Summary) are sub-tabs of Tables; the solver
log is a collapsible section of the Analysis panel. Next phases: unit system, legend/units in the viewer, full tables, export.

## GUI V2 Phase 6: export and synchronisation (2026-11-23)
Every result table has **CSV** and **Excel** buttons: the selected rows (or all shown rows, after search/filters), visible columns, values in the active unit system, with a metadata
block (model name, analysis case, load case/combination, result type, unit system). CSV is UTF-8 with BOM; XLSX is written directly (OOXML zipped with `fflate`, the only added dependency;
checked by re-reading a generated file with openpyxl). Files are produced by a browser download; the desktop shell's save behaviour (WebView2 download) has not been verified.
Synchronisation: clicking a table row selects the node/element for the 3D viewer, Model Explorer and Properties; a selection made in any of those marks and scrolls to its row (a note says
when the object is not in the table or hidden by search/filters). The multi-row checkbox selection is table-local.

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
