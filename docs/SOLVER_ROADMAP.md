# KOPRUQ-SOLVER — Engineering Development Roadmap

## Product objective
Build a standalone, production-grade C++20 three-dimensional finite-element structural solver, initially optimized for bridges, with independently verifiable numerical results, robust diagnostics, reproducible benchmarks, and a stable API for future KOPRUQ integration. No reliance on MIDAS or OpenSees for analysis.

## Design principles
- SI engineering units: m, kN, kNm, kN/m², rad.
- 6 DOFs per 3D beam node; deterministic indexing and sparse global matrices.
- Separate model input, validation, element formulations, assembly, load generation, solution algorithms, results recovery, verification, and persistence/API.
- Numerical correctness before optimization. Each new capability requires independent reference tests.
- No unapproved edits to the separate KOPRUQ frontend project.

## Immediate priority: Phase 3.2 requalification
The historical 25-test suite was lost. It has **not** been restored. Rebuild confidence with new independent tests, preserving all existing tests.

### R1 — Beam3D core — COMPLETE (reported)
Independent local stiffness, orientation, global transform, rigid modes, energy, invalid inputs. Reported Debug and Release 7/7 PASS; commit `907ea32`.

### R2 — DOF mapping and sparse assembly — COMPLETE (verified)
Independent `tests/unit/assembly_requalification_tests.cpp` covering sorted
nonconsecutive node IDs, DOF ordering, independent 12×12 and 18×18 dense
references, restraint classification, component detection, orientation and
invalid inputs. Build blocker worked around by reconfiguring existing build
trees with `-DVCPKG_MANIFEST_INSTALL=OFF` (root cause of the
`CMakeCXXCompilerId.exe` permission denial not fixed; will resurface on a
clean build tree — see `docs/REQUALIFICATION_STATUS.md`). One production
defect found and fixed (`DofManager` accepted duplicate node IDs). Debug and
Release full CTest 12/12 PASS; commits `07f7344`, `f050918`.

### R3 — Linear static solver requalification — COMPLETE (verified)
Independent `tests/unit/linear_static_solver_requalification_tests.cpp` (10
tests): analytical axial/flexural/torsional cantilever tip displacements
and reactions, a combined-load superposition case with a cross-check
against the sum of single-component solves, direct independent checks of
`reduceKff` and `assembleLoads` (including non-finite-load rejection), an
independent dense 18-DOF two-element portal-frame (column + cantilevered
beam) assembled, partitioned and solved without calling
`MatrixAssembler`/`DofManager`/`LinearStaticSolver`, `NoRestraints` and
empty-model `InvalidModel` rejection, a genuinely singular pinned-beam case
(free rigid-body rotation about the pin), and determinism plus per-load-case
equilibrium across two distinct load cases. Tolerances are relative to
displacement/reaction magnitude, `1e-8`–`1e-9`. Debug and Release full
CTest 22/22 PASS.

### R4 — Reaction, element-force and energy recovery requalification
- Reactions only on restrained DOFs: R = KU - F; free DOF residuals separate.
- Independent axial/transverse/inclined 3D reaction benchmarks.
- Independent local/global element end-force checks, orientation and signs.
- Force equilibrium, moment equilibrium about a declared origin, element equilibrium, energy consistency.
- Recovery must not repeat solver factorization.
- Independent multi-element portal-frame recovery tests and tolerance documentation.

### R5 — Performance, robustness and release gate
- Reproducible benchmark models at increasing size (e.g. 100, 1,000, 10,000 elements) within practical memory/time limits.
- Measure sparse assembly, Kff extraction, factorization, solve, and recovery separately.
- Confirm sparse complexity improvements do not compromise accuracy.
- Run full Debug/Release CTest, CLI smoke tests, test-inventory audit, sanitization/static analysis where supported.
- Produce a requalification report explicitly listing verified and unverified capabilities.

## Subsequent solver development — implement in dependency order
The phases below are planning targets, **not completed features**. Validate scope and numerical formulations before implementation.

### Phase 4 — Production model and load architecture
- Immutable/validated model representation; nodes, beam sections, materials, supports, connectivity, local axes.
- Nodal loads, distributed member loads, self-weight, consistent equivalent nodal loads, load cases and combinations.
- Reliable import/export and schema versioning; structured errors and reproducible result metadata.

### Phase 5 — Additional structural elements and releases
- Beam end releases, rotational springs, eccentric offsets, rigid links and constraints with independent benchmarks.
- Truss elements, spring/link elements, bearing models; numerical stability and mechanisms.
- Timoshenko beam option if engineering requirements justify it; explicit formulation comparison.

### Phase 6 — Advanced linear analyses
- Eigenvalue/modal analysis with mass matrix validation, mode normalization, rigid-body modes.
- Response spectrum and modal combination methods with code-specific configuration isolated from solver kernel.
- Linear buckling and P–Delta/geometric stiffness as separate verified capabilities.

### Phase 7 — Time-dependent and nonlinear analysis
- Time-history integration and damping; independent single/multi-DOF references.
- Geometric and material nonlinearity only after formal formulation/verification plan.
- Incremental-iterative convergence controls and diagnostic reporting.

### Phase 8 — Bridge engineering capabilities
- Construction stages and staged activation, prestressing, creep/shrinkage, temperature effects as separately validated modules.
- Bearings, soil springs, foundations and bridge-specific boundary conditions.
- Moving loads and influence lines/surfaces; bridge load combinations and code-specific libraries.

### Phase 9 — High-performance production solver
- Sparse direct/iterative solver strategy, optional SuiteSparse integration, profiling and parallelism where justified.
- Stable API, reproducibility, scalability and packaging.

### Phase 10 — KOPRUQ integration (EXPLICIT APPROVAL REQUIRED)
- Define a versioned solver input/output contract and process/library boundary.
- Integrate only after user approves modifications to `C:\_ZHarput_Data\KOPRUQ`.
- Bridge alternative generation, batch analysis, candidate comparison, and optimization integration.

### Phase 11 — AI-assisted optimization (later, separate module)
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
