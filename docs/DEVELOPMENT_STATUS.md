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
**Phase 3.2R5 — Performance, robustness and release gate — BLOCKED** by an
acute build-environment issue discovered while starting R5 (see "Known
blocking issue" below). R1-R4 remain verified complete; nothing about their
evidence is affected, since it was captured before this instability began
and `build/` is not version-controlled.

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

## Next autonomous action
Paused. Phase 3.2R5 is next once the build-environment issue above is
resolved (most likely a FortiClient/ESET exclusion for the `build/`
directory or the repository, decided and applied by the user/IT — outside
this session's authority). Once the build tree is stable again: finish and
verify the `solver_benchmark.cpp` recovery-timing change, run it at the
existing 100/1,000/10,000-element sizes, confirm sparse complexity scaling
and residuals, re-run full Debug/Release CTest, redo the CLI smoke test,
audit the complete test inventory, and write the final requalification
report listing verified and unverified capabilities.
