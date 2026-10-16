# KOPRUQ-SOLVER — Development Status

> Initial status assembled from user-provided development reports, **not** independently re-executed in this document. Claude must inspect repository state and refresh evidence before changing a milestone to COMPLETE.

## Development and reporting policy (effective 2026-10-16)
Full rules are in `CLAUDE.md`; summarized here per the user's instruction to
record the current policy in this document too:
- Work proceeds in approved work periods, each following
  PLAN → IMPLEMENT → BUILD → TEST → DEBUG → VERIFY → DOCUMENT → REPORT → WAIT
  FOR APPROVAL.
- A progress report is produced roughly every 30 minutes of active work (or
  at natural task boundaries if elapsed time isn't tracked precisely), after
  each major milestone, on any significant blocker, and before any
  high-impact decision. **Every progress report is a mandatory approval
  checkpoint** — development stops and waits for explicit approval ("Continue"
  / "Approved" / "Devam" / "Onaylıyorum"); silence is not approval.
- All progress reports are written in Turkish, in the fixed format specified
  in `CLAUDE.md` (Phase / Completed Work / Verification Results / Problems
  and Corrections / Development Status / Next Planned Tasks / Approval
  Request).
- This supersedes the previous looser "continuous work policy" of working
  through the whole roadmap unattended; where the two conflict, the more
  restrictive approval-gated rule governs.

## Current priority
**Phase 4.2 — JSON model input, results output and CLI solve path —
COMPLETE (verified on CI, run `38884699955`).** Phase 4.1/4.1b (member
loads, projected loads, load cases) complete. Next work awaits approval (see
"Next autonomous action").

**Phase 3.2 requalification — COMPLETE (verified on CI, run `38872157394`,
commit `1ed5ae4`).** Next: Phase 4, which has **not started** — its first
increment needs user approval of scope and numerical formulation (see "Next
autonomous action"). Full verified/unverified capability list:
`docs/REQUALIFICATION_REPORT.md`.
The local build blocker (ESET `@Trojan.ML/Augur` false positive, see below)
is still unresolved on the developer machine, but is now **sidestepped
legitimately via CI**: per the user's decision (2026-10-16), all Debug and
Release builds/tests run on a GitHub-hosted Windows runner
(`.github/workflows/ci.yml`, private repository), so no compiled binary is
created or executed locally. The local ESET exclusion request to IT remains
open and would restore local builds if granted.

**Verification source of truth from now on:** GitHub Actions run results
(Debug, Release, ASan and static-analysis jobs) for the pushed commit, not
local `build/` trees. Locally, only compile-to-`.obj` checks are run (no
linking, no executables).

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
| Phase 3.2R5 | COMPLETE (verified) | Benchmarks + linear-scaling fix, ASan, `/analyze` gate, diagnostics fields; Debug/Release/ASan 30/30 on CI run `38872157394`; commits `1a42c61`, `734b2fc`, `c564dc3`, `506a8be`, `1ed5ae4` |
| Phase 4.1 | COMPLETE (verified) | Member loads, self-weight, load cases/combinations, `fixed`; Debug/Release/ASan 41/41 on CI run `38874706685`; commits `4fefc25`, `9389ab3` |
| Phase 4.1b | COMPLETE (verified) | Projected-length option for global member loads; Debug/Release/ASan 46/46 on CI run `38878331252`; commits `08af7ed`, `7036b61`, `cecbe0d` |
| Phase 4.2 | COMPLETE (verified) | JSON model I/O (`kopruq-model` v1), results (`kopruq-results` v1), CLI `solve`; Debug/Release/ASan 61/61 on CI run `38884699955`; commits `2d9e4da`, `b3b6ab0`, `24f1c65` |
| Phase 4.3+ | NOT STARTED | Awaiting scope approval |

## Known blocking issue (worked around, not root-caused)
- CMake/vcpkg compiler detection fails with permission denied for `CMakeCXXCompilerId.exe`
  when vcpkg's manifest step re-runs its own compiler detection during
  reconfigure. Worked around for the existing `build/debug` and
  `build/release` trees by reconfiguring with `-DVCPKG_MANIFEST_INSTALL=OFF`
  (dependencies were already present in `vcpkg_installed`). This defers but
  does not fix the underlying permission denial.
- Do not disable security protections or terminate unrelated processes.

## New blocking issue (2026-10-16, R5 start) — ACTIVE, NOT A CODE DEFECT
While starting R5, newly linked `.exe` files in `build/debug` and
`build/release` began intermittently coming out **0 bytes** (link reports
success, or fails with `LNK1104: cannot open file`) or **disappearing
entirely** minutes after a successful build, with no source change and no
process of mine touching them. Diagnosis performed:
- `Get-MpComputerStatus`: Windows Defender real-time protection and
  antivirus are **disabled**.
- `Get-CimInstance ... AntiVirusProduct`: three registered security
  products — Windows Defender, **FortiClient**, **ESET Security**.
- Smart App Control (`VerifiedAndReputablePolicyState`) is **off** (0).
- The affected file was not different each time in a fixed way: retrying a
  build of the *same* target repeatedly eventually produces a valid,
  correctly-sized executable, but a *different* target already built
  earlier would then turn up 0 bytes or missing on the next check, with no
  rebuild of mine touching it. This points to a background process
  scanning/quarantining newly-created executables asynchronously, most
  consistent with FortiClient and/or ESET Security's real-time/EDR
  protection reacting to a short burst of many new small `.exe` files being
  created (a pattern these products commonly treat as a
  suspicious/ransomware-like indicator), not a build-script or compiler
  problem.
- This is **confined to `build/`**, which is not version-controlled; no
  source file, test file, or Git history was affected. `git status` stayed
  clean except for the intentional, not-yet-verified R5 benchmark edit.
- Per project rules, I did not disable Defender/FortiClient/ESET, did not
  terminate their processes, and did not request any such change myself.
  Stopped further rebuild attempts once the pattern escalated from
  individual 0-byte files to whole executables disappearing, rather than
  keep retrying into a worsening state.
- **This requires user/IT action to resolve durably**: most likely an
  exclusion for `C:\_ZHarput_Data\KOPRUQ-SOLVER\build\` (or the whole
  repository) in FortiClient's and/or ESET's real-time-scan/EDR policy.
  That is a security-setting change outside this session's authority to
  make.
- Until resolved, R5 benchmark numbers and a fresh CLI smoke check cannot be
  reliably captured. R1-R4's CTest evidence is unaffected: it was captured
  earlier in this session while the build tree was still stable, and
  `tests/unit/*` test logic does not depend on the CLI/benchmark apps that
  are being hit.

### ROOT CAUSE CONFIRMED (2026-10-16, ~09:10)
ESET Security's own detection log
(`C:\ProgramData\ESET\ESET Security\Logs\virlog.dat`, read with shared
access) contains **79 detections, all named `@Trojan.ML/Augur`** (ESET's
machine-learning "Augur" generic detection), with MSVC `link.exe` as the
triggering process and detection engine `34000 (20261015)`. Detected files
are exactly our build outputs — every `kopruq_*.exe` in `build/debug` and
`build/release` — plus CMake's `CMakeCXXCompilerId.exe`/
`CMakeCCompilerId.exe` probes in `build/r2-debug*`,
`build/requalification-r2-debug` and
`C:\_ZHarput_Data\Tools\vcpkg\buildtrees\detect_compiler\`. This is also
the root cause of the original R2 `CMakeCXXCompilerId.exe: Permission
denied` blocker. ESET's `ransomlog.dat` and `hipslog.dat` are empty, so the
ransomware shield and HIPS were not involved; Fortinet is not implicated.
Earlier text below speculating about FortiClient or a ransomware-pattern
heuristic is superseded by this finding.

Assessment: false positive on freshly compiled, unsigned, low-prevalence
binaries. Correct fix: an ESET PROTECT detection exclusion for
`C:\_ZHarput_Data\KOPRUQ-SOLVER\build\` and
`C:\_ZHarput_Data\Tools\vcpkg\buildtrees\` (and optionally a false-positive
submission to ESET), applied by IT. Not attempted from this session:
disabling ESET, killing its processes, or altering binaries/build output
locations to evade the detection would all be bypassing a security
control and are out of scope.

### Update (same day, after user approval to continue)
Rebuilt `build/debug` and confirmed, at that moment, all 5 CTest
executables present with correct nonzero sizes and a clean **26/26 PASS**
(`kopruq_solver_cli.exe`, not a CTest target, was 0 bytes at the same time).
Shortly after, with **no build command issued in between** — only `ls`/
`rm`/`stat` — 3 of those same 5 just-verified executables
(`kopruq_assembly_requalification_tests.exe`,
`kopruq_beam3d_requalification_tests.exe`,
`kopruq_linear_static_solver_requalification_tests.exe`) had disappeared
entirely and a 4th (`kopruq_recovery_requalification_tests.exe`) had gone to
0 bytes, leaving only 2 of 8 built executables intact. This rules out a
pure link-time race: something is removing/truncating already-written,
already-verified executables in this directory **asynchronously, after the
fact**, independent of any build activity on my part. Per the stricter
retry rule just added to `CLAUDE.md`, stopped immediately rather than
continuing to rebuild into a worsening state, and reported it as a blocker
instead. Source tree remained clean throughout (confirmed via `git
status`); no data loss.

## Test integrity
- Historical 25-test suite was overwritten and cannot be claimed recovered.
- **Verified 2026-10-16** (after R2 changes, commit `f050918`): Debug 12/12,
  Release 12/12, including the two preserved `ElementRecovery` tests, the
  Beam3D requalification suite (R1), and the new assembly requalification
  suite (R2).
- **Verified 2026-10-16 on CI** (GitHub Actions run `38861593856`, commit
  `1a42c61`, windows-2022, MSVC + Ninja + vcpkg at the manifest baseline):
  Debug **26/26**, Release **26/26** (`ctest -N` inventory: 26 tests in both),
  i.e. 2 preserved `ElementRecovery` + 5 Beam3D (R1) + 5 assembly (R2) +
  10 linear static (R3) + 4 recovery (R4).
- **Verified 2026-10-16 on CI** (run `38872157394`, commit `1ed5ae4`):
  Debug **30/30**, Release **30/30**, AddressSanitizer **30/30** —
  the 26 above + 4 `SolverDiagnostics` (R5).
- **Verified 2026-10-16 on CI** (run `38874706685`, commit `9389ab3`):
  Debug **41/41**, Release **41/41**, AddressSanitizer **41/41** —
  the 30 above + 11 `MemberLoads` (Phase 4.1). Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38878331252`, commit `cecbe0d`):
  Debug **46/46**, Release **46/46**, AddressSanitizer **46/46** — the 41
  above + 5 `ProjectedLoads` (Phase 4.1b). Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38884699955`, commit `24f1c65`):
  Debug **61/61**, Release **61/61**, AddressSanitizer **61/61** — the 46
  above + 13 I/O GoogleTest cases (`Sha256`, `ModelJson`, `ResultsJson`,
  `Cli`) + 2 CTest runs of the real CLI executable. Static analysis: 0
  blocking.
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

### 2026-10-16 — Phase 3.2R5 start — BLOCKED (environment, not code)
- Git HEAD / working tree: `502d51e`; one uncommitted, not-yet-verified
  change: `apps/benchmarks/solver_benchmark.cpp` now also times
  `LinearStaticSolver::recover()` per run (previously only
  assembly/reduction/symbolic/numeric/solve were measured), addressing the
  roadmap's "measure recovery separately" criterion. Not committed because
  it has not been successfully built and run end-to-end yet.
- Current task: reproducible benchmarks at increasing size with
  assembly/Kff/factorization/solve/recovery measured separately, full
  Debug/Release CTest, CLI smoke test, requalification report.
- Commands executed: multiple `cmake --build` attempts/retries on
  `build/debug` and `build/release`; one experimental, reverted
  `-DVCPKG_APPLOCAL_DEPS=OFF` reconfigure (it stopped vcpkg from copying
  required runtime DLLs next to Release test executables, causing them to
  fail at runtime with no DLL — reverted back to `ON` immediately; noted
  here so a future session does not retry the same dead end);
  `Get-MpComputerStatus`, `Get-CimInstance ... AntiVirusProduct`, registry
  check of `VerifiedAndReputablePolicyState` for diagnosis.
- Source/test files changed: `apps/benchmarks/solver_benchmark.cpp`
  (uncommitted).
- Debug focused / full CTest: last clean confirmation before the new
  blocking issue appeared was 26/26 (same run documented in the R4 entry
  above). Could not get a second clean confirmation afterward because
  individual executables kept coming out 0 bytes or disappearing (see "New
  blocking issue" above) — not because any test logic changed or failed.
- Release focused / full CTest: not re-confirmed this session after the
  issue appeared, for the same reason.
- Independent numerical reference and error: none produced yet for R5 —
  blocked before any benchmark run completed cleanly.
- Root cause of any failures: environmental, not a code defect — see "New
  blocking issue" above. Most likely FortiClient/ESET real-time/EDR
  protection reacting to many new small executables being created in a
  short window.
- Milestone status: **BLOCKED** (genuine external blocker; stopped per
  project rules rather than continuing to retry into a worsening state or
  touching security software).
- Next unblocked roadmap task: none — paused for user/IT input on the new
  blocking issue before continuing R5.

### 2026-10-16 — Phase 3.2R5: CI verification path + first benchmarks
- Git HEAD / working tree: `1a42c61` on `ci/github-actions`, fast-forwarded
  into `main` after the CI run below passed; working tree clean.
- Current task: restore a trustworthy Debug/Release verification path
  without bypassing ESET, then take R5's first benchmark measurements.
- Source/test files changed: `.github/workflows/ci.yml` (new);
  `apps/benchmarks/solver_benchmark.cpp` (recovery phase now timed
  separately — previously uncommitted, now verified by CI).
- Debug full CTest (CI): 26/26 PASS. Release full CTest (CI): 26/26 PASS.
- CLI smoke (CI, Release): `--version` → `KOPRUQ-SOLVER 0.1.0`, exit 0;
  `--help` → usage, exit 0; unknown command → `Unknown command: bogus`,
  exit 2. PASS.
- Benchmarks (CI runner, Release, median of 5 after warm-up; straight
  cantilever chain, node 1 fixed, unit load at the tip):

  | elements | DOFs | assembly ms | Kff reduction ms | symbolic ms | numeric ms | solve ms | recovery ms | normalized residual |
  |---:|---:|---:|---:|---:|---:|---:|---:|---:|
  | 100 | 606 | 0.247 | 0.093 | 0.136 | 0.018 | 0.008 | 0.366 | 3.5e-18 |
  | 1,000 | 6,006 | 3.086 | 1.119 | 1.375 | 0.237 | 0.086 | 5.317 | 1.1e-18 |
  | 10,000 | 60,006 | 99.30 | 7.06 | 11.95 | 2.73 | 0.870 | 253.1 | 3.1e-19 |

  `kopruq_sparse_assembly_benchmark` independently: 0.269 / 2.94 / 97.97 ms
  median for 100 / 1,000 / 10,000 elements.
- **Engineering finding (R5 performance gate): assembly and recovery scale
  quadratically.** From 1,000 to 10,000 elements (10×), Kff reduction,
  factorization and solve grow ~6–12× (linear, as expected for this banded
  sparse system), but assembly grows ~32× and recovery ~48×. Code
  inspection explains it: `MatrixAssembler::assemble()` and
  `LinearStaticSolver::recover()` locate each element's two nodes by a
  linear scan over `m.nodes` (and materials/sections by linear scan), and
  `recover()` repeats node scans per load and per reaction — O(elements ×
  nodes). Numerical results are unaffected (residuals ~1e-18); this is a
  scalability defect only. Not yet fixed.
- Milestone status: **R5 INCOMPLETE** — verification path restored,
  benchmarks captured, quadratic-scaling defect identified.
- Next unblocked roadmap task: replace the linear node/property lookups
  with ID→index maps built once per call, re-verify 26/26 on CI, and
  confirm near-linear scaling.

### 2026-10-16 — Phase 3.2R5: scaling fix, inventory audit, report
- Git HEAD: `734b2fc` (code) + docs commit, on `ci/perf-id-lookups`,
  fast-forwarded into `main` after CI passed.
- Source files changed: `include/kopruq/assembly/MatrixAssembler.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp` — element node/material/
  section lookups now via ID→pointer indices built once per call
  (`detail::index_by_id`, last match wins, as the old scans did). Local
  check: affected translation units compiled to `.obj` only (no link, no
  executable), no warnings.
- Debug full CTest (CI run `38863504705`): 26/26 PASS.
- Release full CTest (same run): 26/26 PASS. CLI smoke PASS.
- Benchmarks at 10,000 elements: assembly 99.30 → 19.44 ms, recovery
  253.1 → 31.41 ms; 1,000 → 10,000 growth now ~9.9× (assembly) and ~11.3×
  (recovery), i.e. linear. Residuals bit-identical before/after
  (abs 0.0467754, normalized 3.14632e-19).
- Test inventory audit: 26 `TEST` cases in 5 files = 26 CTest tests; no
  disabled/skipped tests; no test file deleted in Git history;
  `core_tests.cpp` unchanged since `1f67b16`.
- Gaps found (not fixed, recorded in the report): `residual_norm`,
  `relative_residual`, `fixed`, `element_equilibrium_error`,
  `element_moment_equilibrium_error` never populated; `NonFiniteSolution`
  never produced.
- Milestone status: **R5 INCOMPLETE** — only sanitizers/static analysis
  remain.

### 2026-10-16 — Phase 3.2R5 completion: ASan, static analysis, diagnostics
- Git HEAD: `1ed5ae4` (+ this docs commit) on `ci/r5-asan-analyze`,
  fast-forwarded into `main`.
- Files changed: `CMakePresets.json` (`ninja-asan`, `ninja-analyze`),
  `.github/workflows/ci.yml` (4-job matrix, analyze gate),
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `include/kopruq/results/AnalysisResults.hpp`,
  `tests/unit/solver_diagnostics_tests.cpp` (new, 4 tests), `CMakeLists.txt`.
- AddressSanitizer (RelWithDebInfo + `/fsanitize=address`): full CTest
  26/26 (run `38871473340`) and 30/30 (runs `38871760106`, `38872157394`),
  no sanitizer reports.
- `/analyze` (third-party angle-bracket headers excluded): first run 118
  unique warnings, second 142 (new test file) — every one C6326 on a
  GoogleTest assertion line in `tests\` (macro expands to
  `switch (0) case 0: default:`); 0 in `include\kopruq` and `apps\`. Job
  now fails on anything else; run `38872157394`: 0 blocking findings.
- Diagnostics implemented (`506a8be`): `residual_norm`, `relative_residual`
  (normwise backward error), `NonFiniteSolution` status, aggregate
  `element_equilibrium_error` / `element_moment_equilibrium_error`.
  `StaticSolution::fixed` deliberately left unpopulated — its meaning is
  unspecified.
- Debug / Release / ASan full CTest (run `38872157394`): 30/30 each.
  CLI smoke PASS. Benchmarks: linear scaling in every phase (1,000 →
  10,000 elements: 9.5–11.5×); absolute times vary ~1.5–2× between CI runs.
- Milestone status: **R5 COMPLETE; Phase 3.2 requalification COMPLETE.**

### 2026-10-16 — Phase 4.1: member loads, self-weight, load cases
- Approved conventions (user "devam" on the proposed defaults): global
  member loads per true length; consistent equivalent nodal loads and end
  forces `k*u - FEF`; self-weight via `Material::unit_weight` [kN/m^3] in
  global -Z; load cases from one factorization, combinations by
  superposition; `StaticSolution::fixed` = reactions on restrained DOFs.
- Files: `include/kopruq/loads/Loads.hpp` (new: `NodalLoad` moved here,
  `MemberLoad`, `LoadCase`, `LoadCombination`, equivalent-load quadrature),
  `include/kopruq/solvers/LinearStaticSolver.hpp` (`assembleLoadCase`,
  `recoverAssembled`, `solveCases`; `solve`/`recover` kept as wrappers),
  `include/kopruq/core/Material.hpp` (`unit_weight`),
  `include/kopruq/assembly/DofManager.hpp` (`AssemblyErrorCode::InvalidLoad`),
  `tests/unit/member_load_tests.cpp` (new, 11 tests), `CMakeLists.txt`.
- Debug / Release / ASan full CTest (run `38874706685`): 41/41 each.
  Static analysis: 189 C6326 in tests, 0 blocking. All prior 30 tests
  unchanged and passing; benchmark residuals bit-identical to before
  (nodal-load path arithmetic unchanged).
- Independent references: closed-form EB cantilever (uniform y, uniform z
  with theta_y = -dw/dx, axial, root-to-tip triangular), test-local
  virtual-work integral for a partial trapezoid, fixed-fixed two-element
  beam (qL^4/384EI, qL^2/12, qL^2/24), vector statics for an inclined member
  under global load, superposition and direct solve for combinations.
  All at relative tolerance 1e-9 (combinations 1e-10/1e-12).
- Closes the report's gap "no analytical Iy-plane check" (local-z tests).
- Known limitations: no concentrated member loads or distributed moments;
  reported strain energy is 0.5*u^T*K*u of the nodal field and excludes the
  fixed-end (interior) state when member loads are present.
- Milestone status: **Phase 4.1 COMPLETE.**

### 2026-10-16 — Phase 4.1b: projected global member loads
- User decisions (2026-10-16): global member loads per true length **and**
  per projected length must both be available; projection is onto the
  plane normal to the load direction (horizontal projection for gravity);
  existing element local axes (`reference_y`) are sufficient.
- `MemberLoad::projected` (default false = true length, unchanged): applied
  as `w * L_p / L`, `L_p / L = sqrt(1 - (x . d)^2)`; projected local loads
  rejected (`InvalidLoad`). Files: `include/kopruq/loads/Loads.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `tests/unit/projected_load_tests.cpp` (new, 5 tests), `CMakeLists.txt`.
- CI run `38877503974` failed in the Debug job only: post-build
  `gtest_discover_tests` timed out (CMake default 5 s) listing the unchanged
  `kopruq_solver_tests.exe`; Release/ASan passed 46/46. Root cause: cold
  start of a Debug executable on a shared runner. Fix (`cecbe0d`):
  `DISCOVERY_TIMEOUT 60` on every discovery call; no test changes.
- Debug / Release / ASan (run `38878331252`): 46/46 each; static analysis
  0 blocking; benchmark residuals unchanged.
- Milestone status: **Phase 4.1b COMPLETE.**

### 2026-10-16 — Phase 4.2: JSON model I/O, results, CLI solve path
- Approved: Phase 4.2 with `nlohmann-json` (vcpkg, 3.12.0 at baseline),
  used only by the new `kopruq_solver_io` INTERFACE target.
- Files: `include/kopruq/io/{ModelJson,ResultsJson,Sha256,Cli}.hpp` (new),
  `apps/cli/main.cpp`, `schemas/model.schema.json`,
  `schemas/results.schema.json` (full draft 2020-12 schemas replacing the
  placeholders), `examples/{cantilever,portal_frame}.json`,
  `tests/unit/io_tests.cpp` (13 tests), `tests/data/invalid_model.json`,
  `CMakeLists.txt`, `vcpkg.json`, `README.md`.
- Validation is done by the C++ reader (structure, types, ids, references,
  duplicates, value ranges, beam geometry, unknown keys), not by a
  JSON-Schema engine; `ModelJson.SchemaFileMatchesReaderKeys` keeps the
  published schema's property/required sets identical to the reader's.
- CI run `38883546173`: 59/60 in all three configurations, 1 failing test —
  `ModelJson.RejectsInvalidValuesWithPaths`. Root cause (production defect
  in the new reader): nlohmann stores C++-assigned integers as signed, and
  the reader accepted only unsigned ids, so valid ids in programmatically
  built JSON were rejected. Fixed (`24f1c65`) with a regression test.
  (The `b3b6ab0` commit message says 11 I/O tests; there are 12 there, 13
  with the regression test.)
- Debug / Release / ASan (run `38884699955`): 61/61 each; static analysis
  0 blocking (289 C6326 in tests only).
- Verified properties: SHA-256 FIPS vectors; exact model write→read round
  trip; JSON-pipeline solves bit-identical to direct API solves; every
  displacement, reaction and end force serialized exactly; byte-identical
  output for identical input; 30 invalid-input cases + an invalid file
  reported at the expected JSON paths; CLI exit codes 0/1/2/3/4.
- Milestone status: **Phase 4.2 COMPLETE.**

## Next autonomous action
Awaiting user approval (mandatory checkpoint). Candidate next increments
(choose one; each needs scope/formulation approval):
1. **Phase 4.3 — member point loads and distributed moments**, completing
   the load types common in bridge models (axle loads on a member,
   eccentric loads), with closed-form fixed-end-force references.
2. **Phase 5.1 — beam end releases** (moment hinges, pinned connections)
   via static condensation, with independent hand-calculated references.
3. **Phase 4.4 — immutable validated model + structured errors in the C++
   API** (the JSON reader already validates; the C++ API still accepts
   unchecked models).
