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
Not started; blocked on explicit approval to proceed, and on resolving 7A's material/section
gaps (a dedicated code-formula material table and a section-shape property library), plus a
decision on the non-coincident `ELNK` records and the rigid-links/dynamics restriction. Scope:
a MIDAS-specific adapter producing KOPRUQ-SOLVER's existing internal `Model`/`LoadCase` types,
preserving original MIDAS IDs, canonical units (m, kN, kNm, kN/m², rad), explicit failure
(not silent defaults) for anything unresolved.

### Phase 7C — Standalone EXE analysis (MIDAS-format CLI input)
Not started; depends on 7B. `--format midas-nx-json` CLI option alongside the existing native
JSON path; existing CLI behavior preserved.

### Phase 7D — JSON results export (MIDAS-import-oriented results schema)
Not started; depends on 7B/7C. Likely largely satisfied by extending the existing
`kopruq-results`/`kopruq-modal-results`/`kopruq-response-spectrum-results` schemas rather than a
new one, pending review once 7B's model shape is known.

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
