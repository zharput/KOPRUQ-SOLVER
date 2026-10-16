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

### R4 — Reaction, element-force and energy recovery requalification — COMPLETE (verified)
`AnalysisResults`/`ElementResults` declared `strain_energy`,
`element_strain_energy` and `energy_error` fields that `recover()` never
assigned (always zero); implemented them (per-element 0.5·uᵗKu, summed, vs.
external work 0.5·UᵗF, Clapeyron's theorem) — see commit `643c829`.
Independent `tests/unit/recovery_requalification_tests.cpp` (4 tests):
analytical single- and combined-load strain-energy consistency, an inclined
(non-axis-aligned) element's end-force/reaction/energy recovery, and a
two-element portal frame with independently recomputed per-element local/
global end forces, per-element equilibrium, and total energy consistency —
using fresh rotation/stiffness helpers that call no production assembly/
element/solver code. "Recovery must not repeat solver factorization"
verified by inspection: `recover()` contains no `SimplicialLDLT`
construction, `compute()` or `solve()` call. Energy-consistency tolerances
are at floating-point roundoff (`< 1e-9`–`1e-10` relative), since
`UᵗKU = Σ(TᵤUᵤ)ᵗkₗ(TᵤUᵤ)` is an exact identity, not an approximation.
Debug and Release full CTest 26/26 PASS.

### R5 — Performance, robustness and release gate — COMPLETE (verified)
Verified on CI (run `38872157394`, commit `1ed5ae4`): benchmarks at
100/1,000/10,000 elements with every phase timed separately and scaling
linearly after the quadratic-lookup fix (`734b2fc`, residuals
bit-identical); Debug, Release and AddressSanitizer CTest 30/30; MSVC
`/analyze` gate with 0 findings in production code; CLI smoke;
test-inventory audit; previously unpopulated residual/non-finite/aggregate
equilibrium diagnostics implemented and tested (`506a8be`). Details,
verified and unverified capabilities: `docs/REQUALIFICATION_REPORT.md`.

**Phase 3.2 requalification is complete.** Phase 4 has not started; its
first increment requires user approval of scope and formulation (see
`docs/DEVELOPMENT_STATUS.md`).

- Reproducible benchmark models at increasing size (e.g. 100, 1,000, 10,000 elements) within practical memory/time limits.
- Measure sparse assembly, Kff extraction, factorization, solve, and recovery separately.
- Confirm sparse complexity improvements do not compromise accuracy.
- Run full Debug/Release CTest, CLI smoke tests, test-inventory audit, sanitization/static analysis where supported.
- Produce a requalification report explicitly listing verified and unverified capabilities.

## Subsequent solver development — implement in dependency order
The phases below are planning targets, **not completed features**. Validate scope and numerical formulations before implementation.

### Phase 4 — Production model and load architecture
**4.1 member loads and load cases — COMPLETE (verified, CI run `38874706685`,
commits `4fefc25`, `9389ab3`):** uniform/trapezoidal member loads over full
or partial length in local or global axes (global per true length),
consistent equivalent nodal loads, end forces `k·u − FEF`, self-weight
(`Material::unit_weight`, global −Z), multiple load cases from one
factorization, combinations by superposition, `StaticSolution::fixed`.
Debug/Release/ASan 41/41.
- Immutable/validated model representation; nodes, beam sections, materials, supports, connectivity, local axes.
- ~~Nodal loads, distributed member loads, self-weight, consistent equivalent nodal loads, load cases and combinations.~~ Done in 4.1 (concentrated member loads and distributed moments not yet supported).
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
