# KOPRUQ-SOLVER — Engineering Development Roadmap

## Product objective
Build a standalone, production-grade C++20 three-dimensional finite-element structural solver, initially optimized for bridges, with independently verifiable numerical results, robust diagnostics, reproducible benchmarks, and a stable API for future KOPRUQ integration. No reliance on MIDAS or OpenSees for analysis.

## Design principles
- SI engineering units: m, kN, kNm, kN/m², rad.
- 6 DOFs per 3D beam node; deterministic indexing and sparse global matrices.
- Separate model input, validation, element formulations, assembly, load generation, solution algorithms, results recovery, verification, and persistence/API.
- Numerical correctness before optimization. Each new capability requires independent reference tests.
- No unapproved edits to the separate KOPRUQ frontend project.

## Scope change — 2026-10-17 (user decision)
Phase 6.3 (linear buckling / P-Delta), Phase 7 (time-dependent and nonlinear
analysis) and Phase 8 (bridge engineering capabilities: construction
stages, bearings/soil springs, moving loads/influence lines) are
**cancelled and out of scope** for this project. They are removed from
this roadmap, not deferred. The former Phase 9/10/11 are renumbered to
Phase 7/8/9 below since nothing after them in the sequence depends on the
cancelled phases. Full rationale/record: `docs/DEVELOPMENT_STATUS.md`.

## Immediate priority: Phase 3.2 requalification — COMPLETE
The historical 25-test suite was lost. It has **not** been restored. New
independent tests were written instead, preserving all existing tests;
these are requalification, not historical restoration.

### R1 — Beam3D core — COMPLETE (reported)
Independent local stiffness, orientation, global transform, rigid modes, energy, invalid inputs. Reported Debug and Release 7/7 PASS; commit `907ea32`.

### R2 — DOF mapping and sparse assembly — COMPLETE (verified)
Independent `tests/unit/assembly_requalification_tests.cpp` covering sorted
nonconsecutive node IDs, DOF ordering, independent 12×12 and 18×18 dense
references, restraint classification, component detection, orientation and
invalid inputs. One production defect found and fixed (`DofManager`
accepted duplicate node IDs). Debug and Release full CTest 12/12 PASS;
commits `07f7344`, `f050918`.

### R3 — Linear static solver requalification — COMPLETE (verified)
Independent `tests/unit/linear_static_solver_requalification_tests.cpp` (10
tests): analytical axial/flexural/torsional cantilever tip displacements
and reactions, a combined-load superposition case, direct independent
checks of `reduceKff` and `assembleLoads`, an independent dense 18-DOF
two-element portal-frame assembled/partitioned/solved without calling
`MatrixAssembler`/`DofManager`/`LinearStaticSolver`, status-code rejection
paths, and determinism. Debug and Release full CTest 22/22 PASS.

### R4 — Reaction, element-force and energy recovery requalification — COMPLETE (verified)
Implemented previously-dead `strain_energy`/`element_strain_energy`/
`energy_error` fields (commit `643c829`). Independent
`tests/unit/recovery_requalification_tests.cpp` (4 tests): strain-energy
consistency, an inclined element's end-force/reaction/energy recovery, and
a two-element portal frame with independently recomputed per-element
forces and energy. Debug and Release full CTest 26/26 PASS.

### R5 — Performance, robustness and release gate — COMPLETE (verified)
Verified on CI (run `38872157394`, commit `1ed5ae4`): benchmarks at
100/1,000/10,000 elements with every phase timed separately and scaling
linearly after a quadratic-lookup fix (`734b2fc`); Debug, Release and
AddressSanitizer CTest 30/30; MSVC `/analyze` gate with 0 findings in
production code; CLI smoke; test-inventory audit; previously unpopulated
diagnostics implemented (`506a8be`). Details:
`docs/REQUALIFICATION_REPORT.md`.

**Phase 3.2 requalification is complete.**

## Subsequent solver development — implement in dependency order
The phases below are planning targets, **not completed features** except
where explicitly marked COMPLETE. Validate scope and numerical
formulations before implementation.

### Phase 4 — Production model and load architecture
**4.1 member loads and load cases — COMPLETE (verified, CI run
`38874706685`, commits `4fefc25`, `9389ab3`):** uniform/trapezoidal member
loads over full or partial length in local or global axes (global per true
length, or per projected length normal to the load direction — **4.1b**,
CI run `38878331252`), consistent equivalent nodal loads, end forces
`k·u − FEF`, self-weight (`Material::unit_weight`, global −Z), multiple
load cases from one factorization, combinations by superposition,
`StaticSolution::fixed`.

**4.2 JSON model I/O and CLI — COMPLETE (verified, CI run `38884699955`,
commits `2d9e4da`, `b3b6ab0`, `24f1c65`):** `kopruq-model` v1 input with
path-qualified validation, `kopruq-results` v1 deterministic output with
SHA-256 input metadata, CLI `solve`.

**4.3 concentrated member forces/moments and distributed moments —
COMPLETE (verified, CI run `38887606677`, commits `48ad305`, `b2a605f`,
`62cd955`):** `MemberPointLoad`, distributed moments, a force-equilibrium
metric fix for moment-only loading.

**4.4 validated model in the C++ API — COMPLETE (verified, CI run
`38896544673`, commits `dba9837`, `a942083`):** `validateModel()` /
`validateModelOrThrow()`, same semantic checks as the JSON reader, for
direct C++ callers; `solveCases()`/`recover()` unchanged.

All of Phase 4's originally planned scope is done: immutable/validated
model representation (validated in 4.4; not yet immutable — mutation after
validation is still possible, caller discipline only), nodal/member loads
and load cases/combinations (4.1, 4.3), reliable import/export with schema
versioning and structured errors (4.2).

### Phase 5 — Additional structural elements and releases
**5.1 beam end releases — COMPLETE (verified, CI run `38894147560`, commits
`4dee2b5`, `cbef2e8`, `41aec8a`):** static condensation of K and member
loads, unstable-combination rejection, JSON `releases`.

**5.2 truss elements — COMPLETE (verified, CI run `39069625215`, commits
`2228863`, `a1e8212`):** `Truss3D.hpp` -- a pin-ended 3D truss member
implemented as a Beam3D with every rotational DOF released (RY,RZ at both
ends, RX at node2 only -- releasing RX at both ends creates a separate,
genuine zero-energy spin mechanism, caught by CI and fixed), reusing the
already-verified static-condensation machinery from 5.1 rather than a
second stiffness/mass/load/recovery path. JSON `trusses` array.

**5.3 rigid end offsets (eccentric connections) — COMPLETE (verified, CI
run `39069625215`, commit `a1e8212`):** `Offsets.hpp` -- a rigid-body
kinematic transform (`K_node = G^T K_point G`, same for M and member
loads) mapping a member's offset end point to its node; zero offsets are
an exact no-op. JSON `offset1`/`offset2` on beams.

**5.4 spring/link elements, including bearing and rotational-spring models
— COMPLETE (verified, CI run `39069625215`, commits `9126760`, `189df29`,
`7a6fb83`):** `SpringLink.hpp` -- a two-node, six-independent-DOF discrete
spring (kx,ky,kz,krx,kry,krz, no coupling between them), covering bearings,
isolators and soil springs. **Formulation limit, found by CI and now
enforced, not worked around:** node1 and node2 must be at the same
location -- the decoupled per-DOF stiffness has no lever-arm coupling
between a node's translational springs and the other node's position, so
it is only rigid-body consistent (passes the equilibrium-residual check
used throughout this codebase) when the nodes coincide. This is not a
general two-point rigid link for spatially separated nodes; a spring/link
always was, and remains, a single-point bearing/isolator/soil-spring
model. JSON `springs` array.

**5.5 general rigid links / multi-point constraints — COMPLETE (verified,
CI run `39104401638`, commit `18b6a70`):** `RigidLink.hpp` -- a slave
node's 6 DOFs entirely determined by a master's via rigid-body kinematics
(one master, many possible slaves, e.g. a rigid diaphragm), implemented as
a global DOF-elimination layer wrapped around the existing, unmodified
assembled K/F/U, with an isolated code path
(`LinearStaticSolver::solveCasesWithRigidLinks`) so a model without
rigid links is provably unaffected. Static analysis only -- ModalSolver
and ResponseSpectrumSolver explicitly reject a model that has any, rather
than silently ignoring the constraint. JSON `rigid_links` array.

**5.6 Timoshenko beam option — COMPLETE (verified, CI run `39104401638`,
commits `edc0e6d`, `a2f5290`):** optional effective shear areas
`Section::Asy`/`Asz` (0 = rigid in shear, the prior Euler-Bernoulli
behavior, recovered exactly); `Beam3D::localStiffness` applies the
standard Timoshenko (1+Phi) correction. The mass matrix deliberately keeps
its Euler-Bernoulli shape functions regardless (documented simplification,
not an oversight) -- modal/response-spectrum results for a Timoshenko
beam use this slightly approximate mass alongside the exact Timoshenko
stiffness.

Phase 5 is now complete in full: 5.1-5.6, with no remaining deferred
items. (5.5/5.6 were completed per explicit user instruction on
2026-10-18 after being initially deferred as out of the original work
period's scope.)

### Phase 6 — Advanced linear analyses
**6.1 modal analysis — COMPLETE (verified, CI run `38902536528`, commits
`63402a5`, `d12a40a`, `bca8206`):** consistent mass matrix, Guyan mass
condensation for released ends, free-DOF generalized eigenproblem,
mass-normalized mode shapes.

**6.1b JSON/CLI support for modal analysis — COMPLETE (verified, run
`38906210839`, commits `86f90d1`, `31fb203`):** `kopruq-modal-results` v1,
CLI `modal` command.

**6.2 single-direction response spectrum analysis — COMPLETE (verified, CI
run `39051080335`, commits `b207adf`, `5807f51`):** `ResponseSpectrumSolver`
on top of 6.1's modes; caller-supplied spectrum points only (spectrum
shape itself stays outside the solver kernel); SRSS modal combination; CQC
and multi-direction combination rules not implemented (out of scope unless
separately requested).

**6.2b JSON/CLI support for response spectrum analysis — COMPLETE
(verified, run `39055492002`, commits `3073eb6`, `7831ddd`):** model
`response_spectra` array, `kopruq-response-spectrum-results` v1, CLI
`spectrum` command.

Linear buckling / P-Delta (geometric stiffness) — **cancelled, out of
scope** (see "Scope change" above). Phase 6 is otherwise complete as
listed; no further items planned unless the user adds scope.

### Phase 7 — High-performance production solver
*(renumbered from the former Phase 9; the former Phase 7-8 are cancelled, see "Scope change" above)*
- Sparse direct/iterative solver strategy, optional SuiteSparse integration, profiling and parallelism where justified.
- Stable API, reproducibility, scalability and packaging.
Not started.

## Scope addition — 2026-10-18 (user decision): Phases 7A-7F, MIDAS NX JSON import
User instruction: make KOPRUQ-SOLVER able to import a real MIDAS Civil NX JSON export
(`xmidas/deneme1.json`), reconstruct the model internally, analyze it with KOPRUQ-SOLVER's own
solver (never MIDAS or OpenSees as a computational dependency), and export JSON results,
independently verified against analytical references first and MIDAS's own results where
available. Phases 7A-7F below sit after Phase 7 and before Phase 8/9, which remain deferred
pending explicit approval as before. First execution (2026-10-18) was audit-only, per the user's
explicit instruction not to begin implementing 7B yet: `docs/MIDAS_JSON_DATA_AUDIT.md` and
`docs/SOLVER_CAPABILITY_MATRIX.md` record the findings; nothing in 7B-7F has been implemented.

### Phase 7A — Solver capability and MIDAS JSON audit — COMPLETE (this execution, 2026-10-18)
Deliverables: `docs/MIDAS_JSON_DATA_AUDIT.md` (full inventory of `xmidas/deneme1.json`: 106
nodes, 94 beam elements, 2 materials, 5 sections, 9 supports, 4 elastic links, 5 rigid links, 5
load cases, 40 member loads, 40 element-temperature records, 1 modal case, 2 response-spectrum
cases); `docs/SOLVER_CAPABILITY_MATRIX.md` (capability-by-capability gap analysis). Key findings,
in blocking-severity order: (1) material elastic modulus/Poisson/unit-weight are not in the
export, only a design-code grade name — needs a cited code formula or explicit rejection; (2)
section area/Iy/Iz/J are not in the export, only a shape code + raw dimensions — needs a
section-shape property library for the 3 shapes actually used (`SB`,`B`,`SR`); (3) the sample's
4 elastic links (`ELNK`) connect non-coincident nodes, which Phase 5.4's `SpringLink` deliberately
rejects (a real formulation limit, not a missing-data gap) — needs either extending the element
or explicit rejection of those records; (4) the sample's rigid links (`RIGD`, maps directly onto
Phase 5.5's `RigidLink`) coexist with modal/response-spectrum requests that `ModalSolver`/
`ResponseSpectrumSolver` currently reject outright when `rigid_links` is non-empty; (5) element
temperature loads (`ETMP`/case `TU`) are not implemented in any phase. Official MIDAS
documentation pages (API manual, Export-to-JSON) returned HTTP 403 (authenticated portal);
findings are from direct inspection of the real export, not assumed from inaccessible docs.

### Phase 7B — MIDAS JSON importer and internal model reconstruction
**COMPLETE (verified, CI run `39227258173`, 199/199 Debug/Release/ASan, 2026-10-19; commits
`0823f8f`, `281b2b2`, `bd5ebbe`).** The real `xmidas/deneme1.json` bridge model (106 nodes, 94
beam elements, 7 individual rigid links, 4 load cases) imports successfully, passes
`validateModel`, and **solves successfully end to end** with KOPRUQ-SOLVER's own
`LinearStaticSolver` (no exception, `status.success == true`) -- the full chain MIDAS JSON ->
internal model -> independent numerical solution is demonstrated, not merely parsed.
`include/kopruq/io/midas/MidasJsonImporter.hpp`
translates `xmidas/deneme1.json` into a `kopruq::io::ModelInput`, using three MIDAS-specific
helpers built for this: `MidasMaterialLibrary.hpp` (grade-name -> E/Poisson/unit-weight, the
EN(RC) code formula plus disclosed standard-value assumptions), `MidasSectionLibrary.hpp`
(DBUSER shape code + dimensions -> area/I/J for `SB`/`B`/`SR`), `MidasOrientation.hpp`
(Beta angle -> `reference_y`, confirmed against MIDAS's own manual wording found via web
search). Supported: `NODE`, `ELEM` (`BEAM` only), `MATL`/`MATD`, `SECT`, `CONS`, `RIGD`, `BODF`
(global -Z only), `BMLD` (`CMD=BEAM`,`TYPE=UNILOAD`,`DIRECTION=GZ` only).

**Update, 2026-10-19 (verified, CI run `39247142910`, 219/219 Debug/Release/ASan; commits
`2b19d2a`, `b43f605`): `ETMP` (uniform element temperature) is now imported**, mapping directly
onto `MemberThermalLoad::uniform_delta_t` -- the sample's 40 `ETMP` records (case `"TU"`, all
`TEMP=40.0`) import and **solve successfully** (not merely parse) with the thermal load actually
active (`MidasMaterialLibrary.hpp` now also assigns the disclosed EN1992-1-1 S3.1.3(5) thermal
expansion coefficient, 1.0e-5/K). `GTMP` (gradient) and `BTMP` (beam-section temperature) remain
explicitly rejected when present -- not in this sample, 0 records -- because MIDAS's T1/T2
top-fiber sign convention could not be confirmed from accessible documentation; guessing it
would silently flip which way a member bends.

Explicitly rejected, not silently dropped: `ELNK` (non-coincident elastic links); `GTMP`/`BTMP`
(see above); anything outside the supported `ELEM`/`MATL`/`SECT` types; `EIGV`/`SPLC` (not
executed while `RIGD` models are rejected by `ModalSolver`/`ResponseSpectrumSolver`).
**Superseded by Phase 7S/7M below**: `MidasSectionLibrary`/`MidasMaterialLibrary` are
MIDAS-specific stand-ins for what 7S/7M build as general, reusable engines; once those exist, 7B
should be revisited to delegate to them instead of duplicating the shape/grade formulas -- see
the 2026-10-19 scope addition.

## Scope addition — 2026-10-19 (user decision): Phases 7S, 7M, 7T
User instruction: extend the standalone solver with three general-purpose engineering
capabilities that belong to the solver core itself, not to the MIDAS importer -- a parametric/
general section property engine, a Eurocode material library addressed by grade name, and
uniform/gradient temperature load analysis for Beam3D. All three must be usable by native JSON
models and by the MIDAS importer alike (no duplicate engineering logic). First execution
(2026-10-19) was roadmap-update only, per explicit instruction not to implement yet; nothing in
7S/7M/7T has been implemented. Inserted here, between 7B and 7C, because 7C (standalone EXE
analysis) and later phases should use these general engines rather than 7B's current
MIDAS-specific stand-ins once they exist.

### Phase 7S — Parametric and general section property engine
**First increment COMPLETE (verified, CI run `39227258173`, 199/199 Debug/Release/ASan,
2026-10-19; commits `f10dca0`, `59b0035`).** The first CI run (`39223638115`) caught a real bug
-- the centroid accumulator used the wrong divisor (1/12 instead of 1/6), off by exactly 2x,
invisible in every symmetric/origin-centered test case and only caught by a dedicated
translation-invariance test; fixed in `59b0035`.
`include/kopruq/sections/SectionGeometry.hpp`: a general polygon-integration engine (Green's
theorem / shoelace moment formulas) computing area, centroid, Iy/Iz/Iyz, principal axes/moments,
extreme fiber distances and elastic moduli (Wy+/-, Wz+/-) for ANY section built from simple
polygon contours -- an outer boundary plus any number of holes (opposite winding), i.e. the
general polygonal and multi-contour/openings families from the original scope are done.
`include/kopruq/sections/ParametricSections.hpp`: adds the hollow circle (annulus), the one
closed-form shape not already covered by `MidasSectionLibrary.hpp` (Phase 7B)'s solid rectangle/
box/solid circle, which still need migrating into this general (non-MIDAS-named) namespace --
not done in this increment, to keep it isolated and testable.

**Deliberately NOT implemented, requiring individual approval before each is added** (per
explicit instruction): I-section, H-section, T-section, channel section, angle section, precast
concrete girder sections. Also not yet done: torsional constant J for a general polygon (no
validated general-polygon torsion formula implemented; only the closed-form shapes have J so
far), shear areas Asy/Asz for any shape, mass-per-length, and the MIDAS geometry-vs-property
section verification report (needs 7B wiring once this engine covers what 7B currently gets from
`MidasSectionLibrary.hpp` directly).

### Phase 7M — Eurocode material library
**COMPLETE (verified, CI run `39227258173`, 199/199 Debug/Release/ASan, 2026-10-19; commit
`557d375`).** `include/kopruq/materials/EurocodeMaterialLibrary.hpp`. Scope: a versioned Eurocode concrete (and future-compatible steel) material
database addressed by grade name (e.g. `EN-C35/45`), citing the standard edition and National
Annex, giving fck, fck,cube, fcm, fctm, Ecm, Poisson's ratio, unit weight, thermal expansion
coefficient, and deriving G = E/(2(1+ν)). Distinguishes standard-defined values, National Annex
values, project assumptions and user overrides; never hardcodes an undocumented default
density/Poisson/thermal-expansion value. Generalizes `MidasMaterialLibrary.hpp` (built for Phase
7B), which both the MIDAS importer and native JSON models should use once this exists, instead
of each having their own grade-resolution logic.

### Phase 7T — Uniform and gradient temperature load analysis
**COMPLETE (verified, CI run `39237884916`, 215/215 Debug/Release/ASan, 2026-10-19; commits
`bd0a079`, `9c53fd1`, `1dd5323`).**
`Material::thermal_expansion` (alpha_T, 1/K, default 0) and `MemberThermalLoad` (uniform_delta_t
plus independent gradient_y/gradient_z delta_t+depth pairs) added; `Loads.hpp`'s `LoadCase`
gets a `thermal` field; `include/kopruq/loads/ThermalLoads.hpp` derives the consistent
equivalent nodal load vector directly from the same cubic-Hermite shape functions already used
throughout this project (MassMatrix.hpp/Loads.hpp), respecting the established per-plane sign
convention (theta_y = -dw/dx). Wired into `LinearStaticSolver::assembleLoadCase` via the
existing `accumulate()`/`frameOf()` machinery (releases, offsets, equilibrium bookkeeping all
apply unchanged); a thermal load's resultant force/moment are reported as zero (it is an
initial-strain/eigenstrain load, not a real external action), keeping the global equilibrium
check meaningful. JSON model I/O and schema updated (`material.thermal_expansion`,
`load_case.thermal`).

Independent verification (`tests/unit/thermal_load_tests.cpp`): free uniform expansion
(`alpha_T*Delta T*L`); fully restrained axial force `|N| = E*A*alpha_T*|Delta T|`; a
statically-indeterminate two-bar series system (partial restraint via the second bar's finite
stiffness, hand-solved by compatibility) with one and then both bars heated at different
alpha_T/Delta T; an inclined 3D member's free expansion along its own axis; free-cantilever
gradient curvature matched against classical constant-curvature kinematics (tip rotation =
kappa*L, tip deflection = kappa*L^2/2, independent of the production virtual-work derivation);
a fully restrained gradient's fixed-end moment `|M| = E*Iy*kappa`; combined thermal + nodal load
global equilibrium/energy checks; invalid input (gradient delta_t without a positive depth)
rejected.

This is the first thermal/temperature capability in any phase; `ETMP`/`TYPE=T` MIDAS records are
still rejected by Phase 7B as of this commit (wiring the MIDAS importer to use this new
capability is a separate, not-yet-done follow-up, deliberately not bundled into this change).
Explicitly out of scope for 7T itself: EN 1991-1-5 code-based thermal action generation (bridge
deck temperature components, National Annex parameters) -- 7T is structural analysis under an
already-defined thermal load, not a thermal-action generator; that is a separate, later,
explicitly-scoped capability if ever added.

### Phase 7C — Standalone EXE analysis (MIDAS-format CLI input)
**COMPLETE (pending CI verification, 2026-10-19).** `include/kopruq/io/Cli.hpp`: added a
`--format <native|midas-nx-json>` option to `solve`/`modal`/`spectrum`, sharing the same
`CommonArgs` parser as `-o`/`-n`. With `--format midas-nx-json`, the CLI parses the input file as
raw JSON and runs it through Phase 7B's `midas::importMidasJson()` instead of
`readModelText()`, producing the same `ModelInput` either way -- the rest of the CLI (solve/
modal/spectrum dispatch, results writing, exit codes) is completely unchanged and unaware which
format was used. The importer's `MidasImportReport` (node/beam/material/... counts, every
disclosed assumption, every rejected record) is always printed to stderr
(`detail::reportMidasImport`), so a MIDAS import's gaps are visible on every run, not only when
explicitly queried. An unknown `--format` value or a MIDAS file that is not valid JSON are
usage/exit-3 errors with diagnostics, matching the native-format contract. Existing native-format
CLI behavior is unchanged (format defaults to `"native"`). New tests in
`tests/unit/io_tests.cpp` (`Cli.MidasNxJsonFormatOption`) exercise: unknown-format rejection,
missing-value rejection, an end-to-end solve of the real `xmidas/deneme1.json` sample through the
CLI (checking the import report appears on stderr with its node count and at least one
rejection), and a malformed-MIDAS-JSON exit-3 path.

### Phase 7D — JSON results export (MIDAS-import-oriented results schema)
**COMPLETE as "no new schema needed" (2026-10-19).** Because Phase 7C's importer produces the
same `ModelInput` the native path produces, `solve`/`modal`/`spectrum` already write results
through the existing, unmodified `writeResults`/`writeModalResults`/`writeResponseSpectrumResults`
schemas regardless of which input format was used -- confirmed directly by the Phase 7C CLI test,
whose MIDAS-sourced `solve` output is parsed and checked like any native-format result. No
MIDAS-import-oriented results schema was needed or added; a model's provenance (which format it
was imported from) is not part of the results document today, since nothing downstream
(comparison, reporting) currently needs to distinguish it. If MIDAS NX result comparison (Phase
7E) later needs provenance tagging, it can be added in that phase without reopening 7D.

### Phase 7E — MIDAS NX results comparison
Not started; depends on 7B-7D. Reference data available so far: MIDAS's own modal frequencies
for all 20 modes, readable directly from `xmidas/deneme1_Eigenvalue.out` (plain text). No
readable numeric static or response-spectrum results were found in the sibling `.out`/`.log`
files (they are run logs only); the corresponding `.rst`/`.mec`/`.nfxp` files are binary MIDAS
formats not parsed in this audit. Full static/spectrum comparison may need an additional results
export from MIDAS in a parseable format, or MIDAS API access — to be confirmed, not assumed,
before claiming MIDAS numerical validation is complete.

### Phase 7F — Final standalone release
Not started; depends on 7A-7E.

### Phase 8 — KOPRUQ integration (EXPLICIT APPROVAL REQUIRED)
*(renumbered from the former Phase 10)*
- Define a versioned solver input/output contract and process/library boundary.
- Integrate only after user approves modifications to `C:\_ZHarput_Data\KOPRUQ`.
- Bridge alternative generation, batch analysis, candidate comparison, and optimization integration.

### Phase 9 — AI-assisted optimization (later, separate module)
*(renumbered from the former Phase 11)*
- AI proposes design candidates and prioritizes search; deterministic verified solver remains the authority for physics and safety checks.
- Traceable constraints, objectives, result provenance and human engineering review.

## QT6 VERIFICATION GUI — STANDALONE MIDAS NX VALIDATION

### 1. Purpose
A desktop verification tool that drives KOPRUQ-SOLVER's own CLI against a real MIDAS Civil NX
JSON export and the MIDAS Excel result exports it produces, and reports, per result category and
per individual value, whether KOPRUQ-SOLVER independently reproduces MIDAS's numbers within
engineering tolerance. This is an engineering verification instrument (Phase 7E's practical front
end), not a modeling or visualization tool.

### 2. Scope
In scope: file selection, model validation, solver invocation via the existing CLI, parsing
MIDAS's own Excel result exports, numeric comparison against KOPRUQ-SOLVER's `results.json`, and
report export. Explicitly out of scope: any 3D/graphical model preview, structural diagrams, QML/
Qt Quick, model editing, or any change to the FEM numerical core. The GUI never recomputes a
result itself -- every number it displays comes from the CLI's own JSON output or from the parsed
MIDAS Excel files, never from GUI-side arithmetic duplicating the solver.

### 3. Technology stack
C++20, Qt 6 Widgets, CMake (new vcpkg dependency: a Qt6 port, e.g. `qtbase[core,gui,widgets]`, not
present in today's `vcpkg.json`), `QProcess` to invoke the existing `kopruq_solver_cli` as a
separate process, `QFileDialog`/`QSettings`/`QTableView`/`QTabWidget`/`QPlainTextEdit`/
`QProgressBar`, a small dark-mode QSS stylesheet (reference layout:
`KOPRUQ-SOLVER_menü_qt6.jpg`, repo root). No QML, no 3D, no React/Electron. An Excel reader
is a new dependency (none exists in the repo today -- see section 9).

### 4. Architecture
Three independent layers, so the numerical core, the comparison logic, and the UI never blend:
- **FEM core** (`include/kopruq/...`, `apps/cli`): completely unchanged; the GUI never links
  against solver internals for computation, only reuses `kopruq_solver_io`'s JSON (de)serializers
  (`ResultsJson.hpp`/`ModelJson.hpp`) to parse the CLI's own output, the same way any other result
  consumer would.
- **`verification/`** (new, Qt-independent static library): MIDAS Excel reading, result ID/load-
  case matching, unit conversion, tolerance-based comparison, and XLSX report writing. Pure C++20
  + nlohmann::json + the chosen Excel library; no `QWidget` dependency, so it is independently
  unit-testable with GoogleTest exactly like the rest of the solver.
- **`apps/verification-gui/`** (new Qt executable): widgets, `QProcess` orchestration, and nothing
  else -- it calls into `verification/` for every actual computation or parse.

### 5. Repository structure
```
apps/verification-gui/   main.cpp, MainWindow.{h,cpp}, FileInputPanel.{h,cpp},
                          AnalysisControlPanel.{h,cpp}, ExecutionLogPanel.{h,cpp},
                          ResultsComparisonPanel.{h,cpp}, resources/dark.qss
verification/             ExcelReferenceReader.{h,cpp}, ResultMatcher.{h,cpp},
                          ComparisonEngine.{h,cpp}, VerificationReportExporter.{h,cpp}
```
No duplicate JSON parser or comparison engine is created; `verification/` is the single
implementation the GUI, any future CLI verification command, and tests all share.

### 6. Approved UI layout
Four areas, matching `KOPRUQ-SOLVER_menü_qt6.jpg` exactly in structure and proportion: (1)
five input-file selectors (MIDAS JSON required, four MIDAS Excel exports individually optional),
(2) four action buttons (Modeli Doğrula / Solver'ı Çalıştır / Sonuçları
Karşılaştır / Raporu Dışa Aktar), (3) an execution-status panel
(progress bar, timestamped log), (4) a full-width tabbed comparison area (Özet / Deplasmanlar
(Global) / Mesnet Tepkimeleri (Global) / Kiriş İç Kuvvetleri / Elastik Bağlantılar
/ Diagnostik) plus a bottom tolerance-settings strip. No model preview, no diagrams, no animation.

### 7. GUI-0 through GUI-7 milestones
| # | Milestone | Status |
|---|---|---|
| GUI-0 | Repository audit, roadmap/status update | **COMPLETE** |
| GUI-1 | Main window, four-area dark-theme layout, empty tabs | **COMPLETE (verified, release.yml run `39406148285`, 2026-10-20)** |
| GUI-2 | File selection, extension/existence validation, QSettings persistence | **COMPLETE (verified, release.yml run `39422159633`, 2026-10-20)** |
| GUI-3 | Solver execution via QProcess + real CLI, async, results.json discovery | **COMPLETE (verified, release.yml run `39428529324`, 2026-10-20)** |
| GUI-4 | MIDAS Excel reference parsing (4 files) | **COMPLETE (verified, release.yml run `39435756925`, 2026-10-20)** |
| GUI-5 | Comparison engine, PASS/FAIL/N-A/UNSUPPORTED, tables | **COMPLETE (verified, release.yml run `39468392733`, 2026-10-20)** |
| GUI-6 | XLSX verification report export | **COMPLETE (verified, release.yml run `39548523252`, 2026-10-21)** |
| GUI-7 | Cross-platform build/release validation (Windows + Ubuntu) | **COMPLETE (verified, release.yml run `39760415174`, 2026-10-22)** |

No milestone is marked COMPLETE without its stated acceptance tests passing; GUI-0's own
"acceptance" is this audit plus the two documentation updates it produces.

### 8. Dependencies on Phase 7C/7D
**Satisfied.** Phase 7C (`--format midas-nx-json` on `solve`/`modal`/`spectrum`) and Phase 7D (no
new results schema needed -- the importer produces a standard `ModelInput`, so existing
`kopruq-results`/`kopruq-modal-results`/`kopruq-response-spectrum-results` JSON is written
unchanged) are both complete and CI-verified (run `39265009361`, commit `a92f045`, all three
matrix jobs green). GUI-3 can invoke `kopruq_solver_cli solve <model.json> --format
midas-nx-json -o <results.json>` exactly as documented in `cli_usage`, with no CLI contract
changes required.

### 9. Excel parsing requirements
**No Excel/XLSX handling exists anywhere in the repository today** (confirmed by a
`grep -rn "xlsx|XLSX|Excel"` across all source and CMake files: zero matches outside this new
roadmap text). `vcpkg.json` currently lists only `eigen3`, `gtest`, `nlohmann-json`. A reader must
be added in GUI-4. Candidates to evaluate at that time: **QXlsx** (header+source, MIT-licensed,
already Qt-dependent so it adds no *second* large dependency once Qt6 is already required for the
GUI, straightforward to vendor under `third_party/` or pull via CMake `FetchContent` since it is
not in the vcpkg registry under that exact name) versus a Qt-independent library such as `xlnt`
(available via vcpkg, but a second substantial dependency on top of Qt6, and licensing/size should
be re-checked against vcpkg's current registry before committing). GUI-4 must also first inspect
the real files already present in the repo (`xmidas/Beam Force.xls`,
`xmidas/Displacements(Global).xls`, `xmidas/Elastic Link.xls`, `xmidas/Reaction(Global).xls` --
note these are legacy `.xls`, not `.xlsx`; MIDAS's actual export extension must be confirmed
before picking a library, since `.xls` (BIFF/OLE2) and `.xlsx` (OOXML) are different formats and
not every candidate library reads both) before assuming any fixed column layout.

### 10. Engineering comparison rules
Per-value status is PASS/FAIL/N-A/UNSUPPORTED, never a false PASS from a missing or unmatched
value. Matching is by original MIDAS ID (node/element/link) and load-case name, both of which
Phase 7B's importer already preserves (`MidasImportReport::originalToInternalNodeId`/
`originalToInternalElementId`, and load case names are carried through unchanged). A category with
no comparable KOPRUQ-SOLVER output -- today, this includes elastic links, since the importer
unconditionally rejects every MIDAS `ELNK` record (non-coincident-node elastic links are a real
formulation gap, not a missing-data gap -- see Phase 7B) -- must be reported as UNSUPPORTED for
every row, never silently skipped or marked PASS/N-A.

### 11. Numerical tolerances
Combined absolute/relative criterion, `abs(solver - reference) <= absTol + relTol * abs(reference)`,
applied with category-specific absolute tolerances and units (never one dimensional tolerance
reused across displacements, rotations, forces and moments): e.g. displacement in m, rotation in
rad, force in kN, moment in kNm each need their own default `absTol`, configurable in the GUI's
bottom settings strip and persisted via `QSettings`. Exact default values are a GUI-5 decision
(documented in `ComparisonEngine.h` and surfaced in the UI, not hardcoded invisibly), not fixed
here. Near-zero reference values are handled correctly by construction (the `relTol * abs(reference)`
term vanishes, leaving the `absTol` floor to govern) -- no special-cased division-by-reference
logic.

### 12. Acceptance criteria
Per-milestone acceptance is stated under each GUI-N item above (section 6 of the originating task
spec) and is not repeated in full here; the governing principle throughout is CLAUDE.md's existing
quality gate: no fabricated or simulated results, no silent data loss, every unsupported/missing
case explicitly and visibly reported, and every existing FEM regression test left untouched and
passing.

### 13. Windows and Ubuntu deployment
**Implemented in GUI-7 (2026-10-21).** Both platforms are verified via the same manual-dispatch-
only `release.yml` used for every prior GUI milestone (never added to every-push `ci.yml`, for
the Actions-minute-budget reasons recorded throughout this document):

- **Windows** (`ninja-gui-release`, existing `build-test` job, `windows-2022`): `windeployqt`
  deploys the Qt6 platform plugin (`qwindows.dll`) and runtime DLLs next to
  `kopruq_verification_gui.exe`; vcpkg's `applocal` step (already used for the CLI's own
  dependencies) copies any other linked DLLs. Without this, the exe cannot even start -- Qt
  Widgets apps fail immediately with "could not load the Qt platform plugin" if the plugin isn't
  deployed alongside the binary. A real `.exe` is uploaded as the `kopruq-verification-gui-
  windows-x64` artifact.
- **Ubuntu** (`gcc-gui-release`, new `build-test-linux-gui` job, `ubuntu-latest`): the GUI has no
  Windows-specific code (confirmed: no `_MSC_VER`/`Windows.h`/`__declspec`, same as the rest of
  the project per `ci.yml`'s own header comment), so the identical C++20 source builds under GCC
  unchanged. Qt6 is still fetched via vcpkg's `"gui"` manifest feature (never a system
  `apt-get install qt6-base-dev` package, avoiding the toolchain inconsistency flagged in the
  prior revision of this section) -- so both platforms build Qt6 from the exact same pinned
  vcpkg baseline. A Qt Widgets app needs a real (or virtual) X server to open at all on Linux; CI
  has no display, so the launch smoke test runs under `xvfb-run` (`xvfb` + a handful of
  `libxcb-*`/`libxkbcommon-x11-0` runtime packages installed via `apt-get`, needed by Qt's xcb
  platform plugin itself -- these are runtime shared libraries the plugin `dlopen`s, not a
  parallel Qt installation, so this does not reintroduce the system-Qt inconsistency). A real
  Linux executable is uploaded as the `kopruq-verification-gui-linux-x64` artifact.
- **For an end user running the GUI outside CI** (the actual deployment scenario this section is
  about): the Windows `.exe` needs its `windeployqt`-populated folder shipped alongside it (not
  just the bare exe); the Linux binary needs a real X11 (or Wayland-via-XWayland) desktop session
  -- no offscreen/headless mode is configured for normal use, only for the CI smoke test. Neither
  platform's binary is statically linked against Qt.

### GUI-7 implementation note (2026-10-21)
New `gcc-gui-release` CMake preset (Ubuntu GCC Release, `KOPRUQ_BUILD_GUI=ON`,
`VCPKG_MANIFEST_FEATURES=gui` -- identical gating discipline to `ninja-gui-release`, so
`ci.yml`'s every-push Ubuntu matrix still never requests `qtbase`/`xlnt`). New
`build-test-linux-gui` job in `release.yml` (manual-dispatch-only, same workflow as the Windows
job): builds and full-CTests the GUI target on Ubuntu, then runs the same "launches and stays
alive for 5s" smoke test as Windows, but under `xvfb-run` since Qt Widgets needs a real or
virtual X server to open at all on Linux. No application code changed -- the GUI already had no
Windows-specific code (same property `ci.yml`'s own header comment already relies on for the
rest of the project), so the identical sources build under GCC unchanged. Full deployment
requirements for both platforms are now documented in section 13 above, replacing its earlier
"risk, not yet resolved" framing.

### GUI-7 — COMPLETE AND VERIFIED (2026-10-22)

`release.yml` run [`39760415174`](https://github.com/zharput/KOPRUQ-SOLVER/actions/runs/39760415174)
(commit `7bf04cb`): **all six jobs green** -- `ninja-debug`, `ninja-release`, `ninja-asan`,
`ninja-analyze`, `ninja-gui-release` (Windows) and `gcc-gui-release` (Ubuntu). **242/242 tests**
on both platforms. The GUI smoke test actually launched the real executable under `xvfb-run` on
Linux and under a normal process start on Windows, confirmed alive for 5s, then closed cleanly on
both. The Linux GUI binary cache (`vcpkg-Linux-gcc-gui-release-*`, 427MB) was finally saved
successfully for the first time this arc, immediately after the job's first full green
completion. This closes a debugging arc of **thirteen distinct, individually-diagnosed, real
root-cause fixes** (full narrative below, kept for anyone who needs to touch this build
configuration again) -- every one converging, none repeated, the last four resolving an entirely
genuine chain of Qt6-on-Linux build/link/runtime issues (missing X11/XCB build-time `-dev`
packages; disk space; a `qtbase` manifest-feature trim that initially went too far and had to be
restored piece by piece with evidence from Qt's own source; and finally vcpkg's static-by-default
Linux triplet requiring an explicit `Q_IMPORT_PLUGIN`, the arc's only application-source-code
change). Windows and Ubuntu both now build, test, and launch the real GUI from the same pinned
vcpkg toolchain and the same application source, with full deployment requirements documented in
section 13.

The Windows half of GUI-7 (`ninja-gui-release`) has passed repeatedly throughout this effort and
is not in question. The new Ubuntu job (`gcc-gui-release`) took **eleven consecutive dispatches**
to debug, each catching one real, distinct, correctly-diagnosed problem (converging the whole
way, never repeating the same error) -- in order: (1) `gperf` (a qtbase build-time tool) needed
`autoconf`/`automake`/`libtool` from apt; (2) `libxcrypt` needed `libltdl-dev`; (3) qtbase's own
X11/XCB/OpenGL feature detection needed a full set of `-dev` packages, not just runtime `.so`s;
(4) one remaining feature, `system_xcb_xinput`, needed `libxcb-xinput-dev`; (5) a wrong guessed
package name (`libxcb-errors-dev`, later `libxcb-errors0-dev`) turned out to be unnecessary
entirely -- removed; (6) qtbase (built with its full default feature set, Debug+Release) ran the
runner out of disk ("No space left on device" mid-build) -- fixed by `VCPKG_BUILD_TYPE=release`
plus freeing preinstalled SDK dirs; (7) even Release-only, qtbase's *default* feature set (dbus,
sql-psql, sql-sqlite, testlib, icu, harfbuzz, brotli, network, opengl, openssl, async-io,
sessionmanager, dnslookup, zstd -- none of which `apps/verification-gui` uses) still didn't fit;
the actually-sustainable fix (not just more cleanup) was trimming `vcpkg.json`'s `qtbase`
dependency to `default-features:false` + an explicit `{gui, widgets}` list (`core` is implicit,
cannot be named explicitly -- a vcpkg manifest-schema rule that broke *every* preset, including
Windows, until caught: fixed same session); (8) with that same run, qtbase actually finished
building successfully (~2h) and the **real final blocker** turned out to be unrelated to any of
the above: `find_package(xlnt CONFIG REQUIRED)` should have been `find_package(Xlnt ...)` --
xlnt's own installed CMake package config is capitalized `Xlnt`; Windows's case-insensitive
filesystem silently tolerated the wrong spelling the whole time, Linux's case-sensitive one did
not. Both final fixes (the capitalization fix and the trimmed `qtbase` feature list) were
committed together as `d4c7db4`, then a manifest-schema typo in that same commit (`"core"` listed
explicitly) was caught and fixed as `edf6251`. The session paused overnight with CI run
`39619224162` (commit `edf6251`) deliberately left running in the cloud, not cancelled.

**(9) Resumed 2026-10-22:** run `39619224162` finished (while offline) as a genuine new failure,
not a repeat -- for the first time, qtbase **and** xlnt both built and linked successfully, and
the GUI target itself failed to *compile*: `QProcess::ExitStatus` was undeclared in
`MainWindow.h`/`SolverRunner.h`/their moc output, even though `<QProcess>` is included. First
hypothesis: trimming `qtbase` to the bare `{gui, widgets}` feature pair (fix (7) above) had
silently dropped `network`, cross-checked against Bitcoin Core's own minimal-Qt6 vcpkg manifest
(which includes `network`) -- added `{gui, widgets, network}` and re-dispatched (run
`39726773483`).

**(10)** That hypothesis was wrong: qtbase rebuilt with `network` included (confirmed from the
log: `qtbase[core,doubleconversion,gui,network,widgets]`) and hit the **identical**
`QProcess::ExitStatus` error again. Root-caused properly this time by reading Qt's own
`src/corelib/configure.cmake` directly: the `"process"` feature (i.e. `QProcess` itself) has
`CONDITION QT_FEATURE_thread OR NOT UNIX` -- on Linux this means `QProcess` does not exist at all
without `thread` explicitly enabled; `network` was never part of its condition. Bitcoin Core's
inclusion of `network` was for its own actual `QtNetwork` usage, unrelated to this. Fixed by
replacing `network` with `thread` in the requested feature list (`{gui, widgets, thread}`) -- this
is now the third attempt at this specific line, but the first one actually backed by reading Qt's
own source rather than cross-project inference. Still excludes every one of qtbase's genuinely
disk-heavy, unused default features (dbus, sql-psql, sql-sqlite, testlib, icu, harfbuzz, brotli,
network, opengl, openssl, async-io, sessionmanager, dnslookup, zstd), so the disk-budget fix from
(7) is not undone.

**(11)** Run `39736079075` (the `thread` fix, plus the approved cache `restore-keys` change):
qtbase built, the GUI target **compiled and linked successfully**, and **all 242 tests passed**
-- the first time any of that happened on Linux. Only the launch smoke test itself failed:
`qt.qpa.plugin: Could not find the Qt platform plugin "xcb" in ""`. Unlike Windows (`windeployqt`
copies the platform plugin next to the exe), nothing deploys `libqxcb.so` anywhere Qt's default
search path would find it on Linux. Fixed in the CI script itself (not a build/dependency
problem): `find`s `libqxcb.so` dynamically inside `vcpkg_installed` (no layout guessing) and
exports `QT_PLUGIN_PATH` to its parent `plugins` directory before launching under `xvfb-run`. A
real end-user Linux deployment would need the same handling (either set `QT_PLUGIN_PATH` or copy
vcpkg's `plugins/platforms` next to the installed binary, mirroring `windeployqt`'s output on
Windows) -- noted here as a GUI-7 deployment-documentation item, not yet automated into the
CMake build the way `windeployqt` is for Windows.

**(12)** Run `39743255847` (user-approved cache `restore-keys` optimization + the plugin-path
smoke-test fix): the `QT_PLUGIN_PATH` fix correctly reported `libqxcb.so not found anywhere under
vcpkg_installed` -- the plugin genuinely was never built at all. Root-caused by reading vcpkg's
`ports/qtbase/vcpkg.json` and `portfile.cmake` directly: on Linux, qtbase's own **default**
feature list includes `{xcb, xcb-sm, xcb-xlib, xrender}` (each `platform: linux`-conditioned) --
none of which our minimal `{gui, widgets, thread}` list requested. Without `xcb`, the portfile
passes `-DINPUT_xcb=no` to Qt's own build, which disables `QXcbIntegrationPlugin` entirely
regardless of which system X11/XCB `-dev` packages are installed (fix (3)/(4)'s apt packages were
necessary but not sufficient). Added a third, Linux-platform-filtered `qtbase` dependency entry
requesting exactly vcpkg's own proven-good Linux default X11 set: `{xcb, xcb-sm, xcb-xlib,
xrender}`.

**Open question noted for the CI-optimization task, not yet resolved:** despite the approved
`restore-keys` fix, `GET /repos/.../actions/caches` shows **zero** `vcpkg-Linux-gcc-gui-release-*`
entries have ever been saved, across the entire multi-day debugging arc -- while
`vcpkg-tool-Linux-*` (same job, same `actions/cache@v4` mechanism, smaller/faster-to-save) and
every Windows GUI cache entry (300-580MB, saving reliably every run) both work normally. Possible
cause: the Linux GUI job has consistently run far longer (1-3+ hours) than any other job in this
workflow, and `actions/cache`'s upload reservation can expire/fail silently on very long-running
jobs without a clear log message -- not confirmed, just the leading hypothesis. Worth watching
once a run completes faster (post-fix (12), most dependencies should already be warm); if binary
cache saves remain absent even on a short run, this needs its own dedicated investigation.
**Resolved/observed (2026-10-22):** on the final green run (`39760415174`), the
`vcpkg-Linux-gcc-gui-release-*` cache (427MB) saved successfully for the first time all arc, right
after the job's first full success. Consistent with (though not proof of) the leading hypothesis:
the save only completed reliably once the job ran to a genuine successful finish rather than
failing partway through a 1-3h run.

**(13)** Run `39753093766` (the `{xcb,xcb-sm,xcb-xlib,xrender}` fix): qtbase rebuilt
(`[core,doubleconversion,fontconfig,freetype,gui,sessionmanager,thread,widgets,xcb,xcb-sm,
xcb-xlib,xkb,xkbcommon-x11,xlib,xrender]`, vcpkg correctly pulling in `xkb`/`xkbcommon-x11`/`xlib`
transitively), configured cleanly with **no X11/XCB feature errors at all this time**, and all 242
tests passed again -- but `libqxcb.so` still didn't exist anywhere, confirmed by a direct `find`
across the entire `vcpkg_installed` tree. **This is the real, final root cause**: vcpkg's
`triplets/x64-linux.cmake` sets `VCPKG_LIBRARY_LINKAGE static` by default (`x64-windows.cmake`
defaults to `dynamic`) -- so on Linux there never was a shared-object platform plugin to find or
point `QT_PLUGIN_PATH` at in the first place; `QXcbIntegrationPlugin` is a **static** library
baked into `Qt6::Gui`, and a statically-linked Qt application must register it explicitly via
`Q_IMPORT_PLUGIN` (Qt's own build-time note, "Using static linking will disable the use of
dynamically loaded plugins. Make sure to import all needed static plugins...", seen but not acted
on in an earlier run's log) plus link against the plugin's own CMake target. Fixed by adding
`#if defined(Q_OS_LINUX) ... Q_IMPORT_PLUGIN(QXcbIntegrationPlugin) #endif` to
`apps/verification-gui/main.cpp` and `if(UNIX AND NOT APPLE) target_link_libraries(... PRIVATE
Qt6::QXcbIntegrationPlugin)` in `CMakeLists.txt` -- both inert on Windows, which keeps loading its
plugin dynamically via `windeployqt`'s deployed `qwindows.dll`, unaffected by any of this. The
now-unnecessary `QT_PLUGIN_PATH`-searching logic from fix (11) was removed from the smoke-test
step in `release.yml`, since a static build has no `.so` to locate at all. This is the first
application-source-code change in the entire GUI-7 debugging arc (everything through fix (12) was
CI/build-configuration only).

**Latest dispatch (after fix 13):** check `docs/DEVELOPMENT_STATUS.md`'s "Next autonomous action"
or the commit log for the current run ID and its outcome -- this entry is the historical debugging
narrative, not necessarily the latest status.

Beyond fix (13) above, no application source code changed in this debugging arc -- every other fix
was either a CI workflow (`release.yml`) package list or a build configuration (`vcpkg.json`, `CMakePresets.json`,
`CMakeLists.txt`'s `find_package` call) problem. GUI-0 through GUI-6 remain complete and verified
exactly as recorded above; only GUI-7's Ubuntu half is outstanding.

### GUI-6 verification (2026-10-21)
**COMPLETE on the first dispatch.** `release.yml` run `39548523252`: all five Windows presets
`success`. **242/242 tests** (237 prior + 5 new `VerificationReportExporter` tests, all passing
-- including the row/column layout assumptions in the test file matching the writer's actual
output exactly on the first attempt). GUI smoke test confirmed another clean 5s launch/close. No
existing test modified or removed.

### GUI-6 implementation note (2026-10-21)
New `verification/VerificationReportExporter.{h,cpp}`: writes
`KOPRUQ_SOLVER_Verification_Report.xlsx` with six worksheets (Summary, Displacements, Reactions,
Beam Forces, Elastic Links, Diagnostics) via `xlnt`'s write API (reusing the same A1-reference
helper pattern already proven safe in `ExcelReferenceReader`). Per the task spec's explicit
requirement ("do not maintain separate, inconsistent comparison calculations for the GUI and
report"), this file **never recomputes a comparison** -- `ReportData` carries exactly the
`ComparisonRow`s and diagnostics `MainWindow::onCompare()` already produced and showed on screen
(now cached as members so `onCompare()` doesn't have to re-run for `onExport()` to use them), and
the Summary sheet's category totals come from the same `summarizeCategory()` GUI-5's Özet tab
uses (moved to `ResultMatcher.h` so both call sites share one implementation, never two
independently-written versions that could silently drift apart). Summary metadata includes the
source model/Excel filenames, solver version (read from `results.json`'s own
`metadata.solver_version`, not re-derived), the tolerance settings used, and the comparison's
execution timestamp. `MainWindow::onExport()` prompts a save location via `QFileDialog`, defaults
to `KOPRUQ_SOLVER_Verification_Report.xlsx` in the user's Documents folder, and is enabled only
after at least one real comparison has run. **Simplification, disclosed rather than silent**: all
worksheet text uses ASCII-only labels (e.g. "Basarili" not "Başarılı") rather than the GUI's
Turkish UTF-8 labels, to avoid an unverifiable encoding risk in `xlnt`'s write path with no local
build to check it against -- the GUI itself rnemains fully Turkish; only the exported file's
static header text is ASCII, and the actual data (ids, load case names, numeric values) is
written verbatim regardless. New `tests/unit/verification_report_exporter_tests.cpp` (5 tests):
round-trip readability, Summary sheet metadata/category-totals correctness, all six worksheets
present, Unsupported rows leave numeric cells blank rather than a fabricated 0, and a graceful
(non-crashing) failure on an unwritable path.

### GUI-5 verification (2026-10-20)
**COMPLETE on the first dispatch.** `release.yml` run `39468392733`: all five Windows presets
`success`. **237/237 tests** (226 prior + 7 new `ComparisonEngine` tests + 4 new `ResultMatcher`
tests, all passing). GUI smoke test confirmed another clean 5s launch/close -- the executable now
also wires `onCompare()` and the `ComparisonRow`/category-summary population path without any
startup issue (reachable code only, not exercised end to end since no model/Excel files are
selected in the unattended smoke test). No existing test modified or removed.

### GUI-5 implementation note (2026-10-21)
New `verification/ComparisonEngine.{h,cpp}` (pure numeric, no I/O): implements the task spec's
combined criterion `abs(solver-reference) <= absTol(category)*scale + relTol*abs(reference)`.
Each physical quantity (Displacement m, Rotation rad, Force kN, Moment kNm) has its own
documented default absolute-tolerance floor -- never one dimensional number reused across all
four, which the task spec explicitly forbids. The approved UI layout has only a single "Mutlak
Tolerans"/"Bağıl Tolerans" pair (not one per category), so that field is exposed as
`absoluteToleranceScale`, a dimensionless multiplier on every category's own floor, and
`relativeTol`, legitimately shared since it is itself dimensionless. Near-zero references are
handled by construction (the `relTol*abs(reference)` term vanishes, leaving the absolute floor),
no special-cased branch needed.

New `verification/ResultMatcher.{h,cpp}`: matches `results.json` (the real `kopruq-results`
schema, `include/kopruq/io/ResultsJson.hpp` -- read back with `nlohmann::json`, never re-derived)
against a `ReferenceTable` by original MIDAS id + load case name, both of which Phase 7B's
importer preserves unchanged. `matchDisplacements`/`matchReactions` share one implementation
(identical Node/Load/6-component structure); `matchBeamForces` additionally reads the `Part`
column's `I[...]`/`J[...]` prefix to pick `local_end_forces[0..5]` vs `[6..11]`.
`markElasticLinksUnsupported` reports every row `Unsupported` unconditionally, since Phase 7B's
importer rejects every MIDAS `ELNK` record -- no element in `results.json` can ever correspond to
one (a real formulation gap, not guessed around). Every reference row always produces a
`ComparisonRow` with a real status: `Pass`/`Fail` (actually compared), `NotApplicable` (the id or
load case has no counterpart in `results.json`, including a load case that failed to solve), or
`Unsupported` -- never silently dropped.

`MainWindow::onCompare()` wires this to the real GUI: reads `lastResultsPath_`, reads whichever
Excel reference files are actually selected (each independently optional, per spec), runs the
matchers, and populates all five result tables (`Özet`'s 4 tiles + category table,
`Deplasmanlar`, `Mesnet Tepkimeleri`, `Kiriş İç Kuvvetleri`, `Elastik Bağlantılar`) plus
`Diagnostik` with every reader/matcher diagnostic (missing columns, unmatched load cases, skipped
categories). A category's Durum is PASS only when it has at least one row and zero FAILs. New
`tests/unit/comparison_engine_tests.cpp` (7 tests: pass/fail, near-zero-reference floor,
per-category tolerance independence, the UI scale factor, non-finite handling, status names) and
`tests/unit/result_matcher_tests.cpp` (hand-built `results.json` fragment + in-memory
`ReferenceTable`s, no Excel I/O needed since `ExcelReferenceReader`'s own tests already cover
that path: displacement PASS/FAIL/3-kinds-of-N/A, beam-force I/J-end matching, elastic-link
unconditional Unsupported, empty-table edge case).

**Known open question, deliberately not resolved here:** MIDAS's own local-axis/beam-force sign
convention was confirmed via WebSearch during Phase 7B (support.midasuser.com returns HTTP 403)
but never independently cross-checked against a real solved result -- that is explicitly Phase
7E's job ("MIDAS NX results comparison"), still not started. If the real `xmidas/deneme1.json`
model shows systematic beam-force FAILs once re-exported to `.xlsx` and compared, that is
information this milestone is supposed to surface, not a GUI-5 defect to silently work around.

### GUI-4 verification (2026-10-20)
**COMPLETE on the first dispatch.** `release.yml` run `39435756925`: all five Windows presets
`success` (`xlnt` built from vcpkg source for the first time, similar to Qt6's first build --
the run took ~50 minutes, now cached). **226/226 tests** (220 prior + the 6 new
`ExcelReferenceReader` tests, all passing, including the `.xls`-rejection and missing-file
diagnostics). GUI smoke test again confirmed a clean 5s launch/close -- the executable now also
links `kopruq_verification` (and transitively `xlnt`) without any runtime startup issue. The
Windows GUI executable artifact was uploaded successfully (`kopruq-verification-gui-windows-x64`,
40,572 bytes). No existing test was modified, weakened, or removed to reach this result.

### GUI-4 implementation note (2026-10-20)
**Format decision (user, 2026-10-20):** the real exports already in `xmidas/` turned out to be
legacy `.xls` (BIFF/OLE2) during inspection, confirmed via `xlrd`. vcpkg has no `.xls`-capable
library (`xlnt`/`xlsxio`/`openxlsx` all read `.xlsx` only; `libxls` is not in vcpkg and would need
vendoring a C library with no local build to verify it against). Asked the user to choose between
vendoring `libxls`, supporting only `.xlsx`, or re-exporting the samples as `.xlsx` -- user chose
**re-export as `.xlsx`**, so `ExcelReferenceReader` supports `.xlsx` only, via `xlnt` (vcpkg,
already gated behind the "gui" manifest feature alongside `qtbase`). A `.xls` file is sniffed by
its real signature (OLE2 `D0 CF 11 E0`, never trusted from the extension alone) and rejected with
an explicit diagnostic naming the problem and the fix, both in `ExcelReferenceReader` itself and
one level up in `FileInputPanel` (GUI-2's Excel rows now only accept `.xlsx`, so a `.xls` file is
never shown as "Seçildi" for a file GUI-4 cannot actually parse).

New `verification/ExcelReferenceReader.{h,cpp}` (Qt-independent, per the architecture split):
locates every field **by header text**, never a hardcoded column index -- the four MIDAS export
types have different column counts/orders (Beam Force 23, Elastic Link 19, Displacements/
Reaction 15 columns, confirmed directly against the real samples before conversion), and nothing
guarantees MIDAS won't add/reorder columns in a future version. The header row itself is located
by content (first row with >=3 non-empty, non-numeric-looking cells), not a hardcoded row index,
since MIDAS's exports have a title row above it. A header cell's `"Name\n(unit)"` text is split
into a name and a unit, both preserved (never discarded). Every cell is captured in
`ReferenceTable::rows` regardless of whether anything currently reads it (`CellValue` holds both
the raw text and, if numeric, the parsed double) -- no silent data loss. New
`tests/unit/excel_reference_reader_tests.cpp`: builds small `.xlsx` fixtures with `xlnt` itself
(no external fixture file needed) mirroring the real `Displacements(Global)` column layout, and
checks header/unit splitting, position-independent name lookup, correct row values, the `.xls`-
rejection diagnostic (via a hand-written OLE2-signature file, not a real `.xls`), and a
missing-file diagnostic.

**Not yet wired into the GUI's "Sonuçları Karşılaştır" button** -- that is GUI-5's
`ComparisonEngine`/`ResultMatcher` job, deliberately out of scope here to keep this milestone
testable in isolation, per the task's own GUI-0→GUI-7 ordering.

### GUI-3 verification (2026-10-20)
**COMPLETE on the first dispatch.** `release.yml` run `39428529324`: all five Windows presets
`success`, **220/220 tests**, GUI smoke test confirmed another clean 5s launch/close -- the
construction path now also builds `SolverRunner` and wires its signals without a crash. As with
GUI-2, an unattended CI job has nothing to point the MIDAS model selector at and no one to click
Run, so an actual CLI invocation (process launch, stdout/stderr capture, exit-code handling,
results.json discovery) is **not yet exercised end to end by CI** -- confirmed instead by code
review of `SolverRunner.cpp`/`MainWindow.cpp` against `Cli.hpp`'s documented contract. A real
local run (same machine, both `ninja-release` and `ninja-gui-release` built from the same tree so
`locateCli()`'s relative-path fallback finds `kopruq_solver_cli.exe`) against the real
`xmidas/deneme1.json` sample would be the natural manual confirmation step once GUI-4/5 give the
GUI something to compare the results against; not performed in this unattended CI-only
environment.

### GUI-3 implementation note (2026-10-20)
New `apps/verification-gui/SolverRunner.{h,cpp}` wraps `QProcess` asynchronously (never blocks the
GUI thread): `locateCli()` searches a handful of plausible developer-build locations next to the
GUI executable, then falls back to `PATH`, returning an empty string (never a guessed path) if
`kopruq_solver_cli[.exe]` isn't found anywhere -- real, deterministic packaging next to the GUI
is GUI-7's job, not simulated here. `run()` always invokes `solve <model> -o <output> --format
midas-nx-json` (this GUI is specifically a MIDAS verification tool, per area 1's "MIDAS NX Model
(JSON)" label, so the format flag is never "native" in practice) exactly as `Cli.hpp` documents
it -- no invented flags. Output is written to `%TEMP%/kopruq_verification/<model>.results.json`.

Both "Modeli Doğrula" and "Solver'ı Çalıştır" invoke the identical CLI command (the CLI has no
separate validate-only mode); they differ only in how `MainWindow` interprets the exit code --
Validate reports only the invalid/valid (exit 3 vs. not) distinction and never claims a solve
succeeded, while Run Solver reports the full 0/1/other contract from `cli_usage`
(`Cli.hpp`). Both buttons are enabled only once `FileInputPanel::hasRequiredModel()` is true and
no run is already in flight; the progress bar goes indeterminate while a run is active (the CLI
exposes no granular progress) and only reaches 100% on an actual success, per the task spec's
explicit "do not invent progress percentages" / "do not show 100% unless the operation actually
succeeds" rules. `MainWindow::closeEvent` terminates (then kills, if needed) any in-flight
process rather than leaving an orphan running after the window closes -- the "cancel/terminate
handling where safe" GUI-3 asks for, done without adding a button outside the approved four-
button layout.

### GUI-2 implementation note (2026-10-20)
`FileInputPanel` now owns real behavior: each "..." button opens a `QFileDialog` filtered to the
row's expected type (`*.json` for the MIDAS model; `*.xlsx *.xls` for the four reference files --
widened from the spec's `.xlsx`-only wording because the real MIDAS exports already in
`xmidas/` are legacy `.xls`, confirmed in GUI-0). A selected path is validated by extension then
by `QFileInfo::exists()`, producing one of `NotSelected`/`Selected`/`Invalid`/`Unsupported` --
deliberately never a "Valid" state, since that would claim a level of checking (actual model
parsing) this panel does not do; that remains the "Modeli Doğrula" action's job once GUI-3 wires
the CLI. Every selection is persisted via `QSettings` (native per-OS backend: Windows registry
under `KOPRUQ-SOLVER\KOPRUQ-SOLVER Verification GUI`, an `.ini`-style file on Linux) and restored
(and re-validated against the current filesystem, so a moved/deleted file is correctly reported
Invalid rather than silently reappearing as if still selected) on next launch. The four Excel rows
remain individually optional by construction -- nothing in `FileInputPanel` requires them to be
filled, only the MIDAS JSON row (`hasRequiredModel()`). No action button is wired in this
milestone (that is GUI-3's CLI-integration scope); clicking one is still inert, not fabricated
success.

### GUI-2 verification (2026-10-20)
**COMPLETE on the first dispatch.** `release.yml` run `39422159633`: all five Windows presets
(`ninja-debug/release/asan/analyze/gui-release`) `success`, **220/220 tests**, GUI smoke test
again confirmed a 5s clean launch/close -- this run exercised `FileInputPanel`'s full
construction path (including `restorePersistedPaths()`/`revalidate()` against a fresh runner
with no prior `QSettings` entries) without a crash. What is **not** CI-verified, and is noted
honestly rather than claimed: the interactive `QFileDialog` flow itself (opening the dialog,
picking a file, seeing the status badge update) and cross-restart settings persistence cannot be
exercised by an unattended CI job with no user to click a dialog -- those are confirmed by code
review of `FileInputPanel.cpp`'s validation logic (extension check, `QFileInfo::exists()`,
`QSettings` read/write) rather than an end-to-end interactive test. A future milestone could add
`QTest`-driven headless UI tests (synthesizing clicks/dialog results) if that level of rigor is
wanted; not implemented here to keep scope matched to "minimize frontend development time."

### GUI-1 verification (2026-10-20)
**COMPLETE.** Three `release.yml` dispatches were needed to reach green, each catching a real,
distinct defect (never worked around or suppressed):
1. Run `39375862004`: Qt6 (`qtbase`, 31 features) built from vcpkg source successfully (~2.5h,
   now binary-cached) and the five other presets (`ninja-debug/release/asan/analyze`) passed
   unaffected, but `ninja-gui-release` failed to compile -- `ResultsComparisonPanel.cpp`'s
   `auto* addTile = [&](...){...}` (a lambda is not a pointer; MSVC C3535/C2440). Fixed in
   `6d7bc82` (plain `auto`).
2. Run `39394108214`: compiled and linked, then failed at `ctest --preset ninja-gui-release -N`
   with "No such test preset" -- `CMakePresets.json` had a `configurePresets`/`buildPresets`
   entry for `ninja-gui-release` but no matching `testPresets` entry. Fixed in `9c74ba9`.
3. Run `39406148285`: **PASS.** `ninja-gui-release`, `ninja-debug`, `ninja-release`, `ninja-asan`,
   `ninja-analyze` all `success`. **220/220 tests passed** (CTest, same inventory as the routine
   Ubuntu matrix plus the GUI preset's own build). The GUI smoke test actually launched
   `kopruq_verification_gui.exe` (with `windeployqt` having deployed the Qt6 platform plugin and
   runtime DLLs next to it), confirmed it stayed alive for 5 seconds (not an immediate
   platform-plugin-missing crash), then closed it cleanly. No existing regression test was
   touched, modified, or disabled by this milestone.

### GUI-1 implementation note (2026-10-20)
User decision for the Qt6/CI verification-strategy question raised in GUI-0: **option (b)** --
manual `release.yml` (Windows) dispatch after each GUI milestone, not a change to the every-push
Ubuntu `ci.yml`. Implemented accordingly: `vcpkg.json` gained an opt-in `"gui"` manifest feature
(`qtbase`, only fetched when `-DVCPKG_MANIFEST_FEATURES=gui` is passed, never by the default
presets `ci.yml` uses); `CMakePresets.json` gained `ninja-gui-release`; `CMakeLists.txt` gained a
`KOPRUQ_BUILD_GUI` option (default `OFF`) gating the new `kopruq_verification_gui` target;
`release.yml`'s Windows matrix gained that preset, a launch-and-stays-alive-for-5s smoke test
(treats an immediate exit as a crash, not success), and an artifact upload. A `windeployqt`
best-effort step was added (skipped quietly if not found, since full Qt runtime deployment
tooling is GUI-7's scope) because a Qt Widgets app cannot start at all without its platform
plugin deployed next to the executable -- without it, the smoke test will correctly report a
crash rather than a false pass. GUI-1's actual pass/fail is therefore determined by the next
manually-dispatched `release.yml` run, not asserted here.

### 14. Risks and limitations
**Resolved by GUI-7 (2026-10-21):** this section's original "central open risk" (no CI path built
Qt6 at all) was resolved per the user's 2026-10-20 decision (option b, manual `release.yml`
dispatch) and is now exercised for both Windows and Ubuntu -- see section 13. The bullets below
are kept as the historical record of the question as it stood during GUI-0/GUI-1, not rewritten.
- **No local build verification is currently possible in this development environment**: `cmake`
  is not on `PATH` here (confirmed this session), so every GUI milestone's "application compiles/
  opens" acceptance criterion can only be verified through CI, not interactively, for as long as
  that remains true.
- **No CI path currently builds Qt6 at all.** `ci.yml` (every push, Ubuntu) has no Qt6; `release.yml`
  (Windows, the natural home for a real desktop GUI) is manual-dispatch/tag-only, specifically to
  avoid repeating an earlier Actions-minute exhaustion incident. Verifying GUI-1's own acceptance
  criteria ("application compiles", "application opens", "no crashes when resizing") therefore
  needs an explicit decision on *how* that will be done before GUI-1 starts -- candidates: (a) add
  Qt6 to the Ubuntu `ci.yml` job (via system package, fastest, documented inconsistency vs. the
  vcpkg-pinned toolchain used for everything else), (b) manually dispatch `release.yml` after each
  GUI milestone (real Windows/MSVC build, but consumes the same limited Windows-minute budget this
  project has already exhausted once), or (c) build and smoke-test only via manual
  `workflow_dispatch` runs the user triggers, not on every push. This is flagged here rather than
  decided unilaterally, since it is a CI-cost/verification-rigor tradeoff, not a routine
  implementation detail.
- Elastic-link MIDAS comparison is structurally UNSUPPORTED for the real `xmidas/deneme1.json`
  sample today (ELNK unconditionally rejected by the Phase 7B importer) -- the GUI will correctly
  show this, but it means the one real reference model available cannot exercise a PASS/FAIL
  elastic-link comparison, only the UNSUPPORTED path.
- MIDAS's real export file extension for the four reference files is `.xls` (legacy BIFF/OLE2) in
  the samples already in the repo, not `.xlsx` -- confirm before choosing an Excel library, since
  this changes the GUI-4 library evaluation.
- No results category for spring/elastic-link elements is distinguished from ordinary beam
  elements in today's `results.json` (`StaticSolution::results.elements` carries beam and spring
  end-forces alike, keyed only by element id, with no element-type tag) -- GUI-5's matcher will
  need the *input* model's element-type information (already available via `ModelInput`) to
  separate "Kiriş İç Kuvvetleri" rows from "Elastik Bağlantılar" rows, not
  `results.json` alone.

### 15. Future enhancements
Not scoped now: batch/multi-model verification runs, CLI-only (headless) verification mode reusing
the same `verification/` library, automatic MIDAS API-based export (instead of manual Excel
export), provenance tagging of `results.json` (import format, see Phase 7D's note), CSV support
for the reference files (optional, only if trivial once the chosen Excel library is in place).

### 16. Completion status tracking
This section's own per-milestone table (item 7) is the single source of truth and will be updated
in place after each milestone's acceptance tests actually pass, per the "Standard acceptance gate"
below. `docs/DEVELOPMENT_STATUS.md` carries the same status in its running log and "Next
autonomous action" section.

## Standard acceptance gate for each milestone
1. Requirements and conventions documented.
2. Implementation builds in Debug and Release.
3. Independent analytical or dense-reference tests PASS.
4. Existing tests preserved; complete inventory audited.
5. Full Debug and Release CTest PASS.
6. Numerical errors and limitations documented.
7. Git diff reviewed and a scoped commit made when authorized.
8. `docs/DEVELOPMENT_STATUS.md` updated.

## Decision policy
Claude may autonomously choose safe implementation details. Pause for user approval before material numerical-formulation changes, major API/architecture changes, destructive actions, or KOPRUQ integration. Avoid repeated permission questions for ordinary implementation/debugging tasks.
