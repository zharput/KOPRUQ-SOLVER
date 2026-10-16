# KOPRUQ-SOLVER — Development Status

> Initial status assembled from user-provided development reports, **not** independently re-executed in this document. Claude must inspect repository state and refresh evidence before changing a milestone to COMPLETE.

## Current priority
**Phase 3.2R5 — Performance, robustness and release gate** (next unblocked
task; R4 is verified complete).

## Established reported history
| Milestone | Status | Evidence / qualification |
|---|---|---|
| Phase 0–2 | Previously implemented | Scaffold, Beam3D, DOF numbering and sparse assembly; historical regression tests subsequently lost |
| Phase 3.1 | Previously implemented | Linear static solver, portal dense reference, optimized sparse Kff; historical 22/22 PASS no longer counts as current coverage |
| Phase 3.2 | Partially implemented | Reaction/element force recovery; historical 25 tests lost; two current ElementRecovery tests reported PASS |
| Phase 3.2R1 | COMPLETE (verified) | New Beam3D tests; Debug 7/7, Release 7/7; commit `907ea32` |
| Phase 3.2R2 | COMPLETE (verified) | New assembly tests; Debug 12/12, Release 12/12; commits `07f7344`, `f050918` |
| Phase 3.2R3 | COMPLETE (verified) | New linear static solver tests; Debug 22/22, Release 22/22 |
| Phase 3.2R4 | COMPLETE (verified) | Implemented + requalified strain-energy recovery; Debug 26/26, Release 26/26; commit `643c829` + test commit below |
| Phase 3.2R5 | NOT STARTED | R5 next |

## Known blocking issue (worked around, not root-caused)
- CMake/vcpkg compiler detection fails with permission denied for `CMakeCXXCompilerId.exe`
  when vcpkg's manifest step re-runs its own compiler detection during
  reconfigure. Worked around for the existing `build/debug` and
  `build/release` trees by reconfiguring with `-DVCPKG_MANIFEST_INSTALL=OFF`
  (dependencies were already present in `vcpkg_installed`). This defers but
  does not fix the underlying permission denial (consistent with
  Defender/Smart App Control interference); a clean build directory will
  likely hit it again and need the same workaround or a real root-cause fix.
- Do not disable security protections or terminate unrelated processes.

## Test integrity
- Historical 25-test suite was overwritten and cannot be claimed recovered.
- **Verified 2026-10-16** (after R2 changes, commit `f050918`): Debug 12/12,
  Release 12/12, including the two preserved `ElementRecovery` tests, the
  Beam3D requalification suite (R1), and the new assembly requalification
  suite (R2).
- Every new test is in a dedicated file; inventory audited before/after each
  milestone.

## Session work log
### 2026-10-16 — Phase 3.2R2 completion
- Git HEAD / working tree: started at `907ea32` with uncommitted R2 files; ended at `f050918` on `main` (merged from `requalification/r2-assembly`, fast-forward)
- Current task: unblock R2 build, verify independent assembly tests, fix any defects
- Commands executed: `cmake -S . -B build/{debug,release} -DVCPKG_MANIFEST_INSTALL=OFF`, `cmake --build`, `ctest --output-on-failure`, CLI smoke checks (`kopruq_solver_cli --version|--help|<unknown>`)
- Source/test files changed: `tests/unit/assembly_requalification_tests.cpp` (new), `CMakeLists.txt` (wire new target into `gtest_discover_tests`), `include/kopruq/assembly/DofManager.hpp` (production fix)
- Debug focused / full CTest: 5/5 new assembly tests PASS standalone; 12/12 full suite PASS
- Release focused / full CTest: 12/12 full suite PASS
- Independent numerical reference and error: test-local Euler–Bernoulli 12×12 reference matched `MatrixAssembler` output to relative Frobenius error < 1e-9 (coefficient scale) / machine precision; independent 18×18 two-element shared-node reference matched to relative Frobenius error < 1e-11
- Root cause of any failures:
  - Build: vcpkg manifest install re-ran compiler detection and hit a `Permission denied` on `CMakeCXXCompilerId.exe`; worked around (not fixed) by skipping manifest install on already-populated build trees.
  - Test harness (4 defects, corrected, not production bugs): uninitialized dynamic-size `Eigen::MatrixXd` zero-matrix in the local reference; reference used `G = 80e6` instead of the model's `G = E/(2(1+nu))`; 18×18 DOF map assumed node insertion order instead of `DofManager`'s sorted-ID order; an `EXPECT_THROW` wrapped a lambda it never invoked.
  - Production (1 defect, fixed in `DofManager.hpp`): duplicate-node-id check tested `offsets_` membership before `offsets_` was populated, so duplicate node IDs were silently accepted instead of raising `AssemblyError`.
- Milestone status: **COMPLETE**
- Next unblocked roadmap task: Phase 3.2R3 — linear static solver requalification

### 2026-10-16 — Phase 3.2R3 completion
- Git HEAD / working tree: started at `b364fc3`; new test file plus `CMakeLists.txt` wiring, no uncommitted changes outstanding at session end
- Current task: independently requalify `LinearStaticSolver` per R3 acceptance criteria
- Commands executed: `cmake --build build/{debug,release}`, direct run of `kopruq_linear_static_solver_requalification_tests.exe`, `ctest` full suite in both configs, CLI smoke checks
- Source/test files changed: `tests/unit/linear_static_solver_requalification_tests.cpp` (new, 10 tests), `CMakeLists.txt` (wire new target into `gtest_discover_tests`)
- Debug focused / full CTest: 10/10 new tests PASS standalone; 22/22 full suite PASS
- Release focused / full CTest: 22/22 full suite PASS
- Independent numerical reference and error: analytical Euler–Bernoulli/torsion cantilever formulas (axial, flexural, torsional, combined) matched to relative error < 1e-9; independent dense 18-DOF two-element portal-frame (column + cantilevered beam), assembled/partitioned/solved with a fresh Eigen `LDLT` dense solve (no production assembly/DOF/solver code), matched production displacements and node-1 reactions to relative error < 1e-8/1e-9; `reduceKff`/`assembleLoads` checked against hand-built sparse systems and DOF maps to machine precision
- Root cause of any failures: none — all 10 new tests passed on first run; no production or test-harness defects found in this milestone
- Milestone status: **COMPLETE**
- Next unblocked roadmap task: Phase 3.2R4 — reaction, element-force and energy recovery requalification

### 2026-10-16 — Phase 3.2R4 completion
- Git HEAD / working tree: started at `63c97c8`; ended with all changes committed on `main`
- Current task: requalify reaction/element-force/energy recovery, scoped to what R3 did not already cover
- Commands executed: `cmake --build build/{debug,release}`, direct run of `kopruq_recovery_requalification_tests.exe`, `ctest` full suite in both configs, CLI smoke checks; code inspection (`grep` for `SimplicialLDLT`/`compute`/`solve` in `LinearStaticSolver.hpp`) to verify recovery does not repeat factorization
- Source/test files changed: `include/kopruq/solvers/LinearStaticSolver.hpp` (production fix — implemented previously-dead `strain_energy`/`element_strain_energy`/`energy_error` fields), `tests/unit/recovery_requalification_tests.cpp` (new, 4 tests), `CMakeLists.txt` (wire new target into `gtest_discover_tests`)
- Debug focused / full CTest: 4/4 new tests PASS standalone; 26/26 full suite PASS
- Release focused / full CTest: 26/26 full suite PASS
- Independent numerical reference and error: Clapeyron's-theorem external-work formulas for axial/combined-load cantilevers; an inclined (non-axis-aligned) 3-4-5 beam with a purely axial applied load, displacement/reaction/energy checked via vector geometry, not any production transform; a two-element portal frame with per-element local/global end forces recomputed via independently-written rotation/stiffness helpers. All energy-consistency checks landed at floating-point roundoff (< 1e-9–1e-10 relative), consistent with strain energy being an exact identity (UᵗKU = Σ element quadratic forms), not an approximate check.
- Root cause of any failures: none at the test level — all 4 new tests passed on first run. One production gap found and fixed: `AnalysisResults`/`ElementResults` declared energy fields that `recover()` never populated (always zero); implemented per the roadmap's explicit "energy consistency" acceptance criterion.
- Milestone status: **COMPLETE**
- Next unblocked roadmap task: Phase 3.2R5 — performance, robustness and release gate

## Next autonomous action
Proceed with Phase 3.2R5 (performance, robustness and release gate) per
`docs/SOLVER_ROADMAP.md`: reproducible benchmark models at increasing size,
separately measured assembly/Kff-extraction/factorization/solve/recovery
timings, confirmation that sparse complexity improvements do not compromise
accuracy, full Debug/Release CTest, CLI smoke tests, a test-inventory audit,
and a requalification report listing verified and unverified capabilities.
Existing benchmark apps (`apps/benchmarks/solver_benchmark.cpp`,
`apps/benchmarks/sparse_assembly_benchmark.cpp`) should be inspected first
to see how much of R5 they already cover before adding new benchmark code.
