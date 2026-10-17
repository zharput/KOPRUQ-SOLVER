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

Not started, still in scope:
- Rotational springs, eccentric offsets, rigid links and constraints with independent benchmarks.
- Truss elements, spring/link elements, bearing models; numerical stability and mechanisms.
- Timoshenko beam option if engineering requirements justify it; explicit formulation comparison.

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
