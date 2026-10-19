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
**Phase 7B's MIDAS ETMP (uniform temperature) wiring — COMPLETE/verified
(CI run `39247142910`, 219/219 Debug/Release/ASan, 2026-10-19).** Per the
user's explicit follow-up request, the MIDAS importer now uses Phase 7T's
thermal-load capability instead of rejecting `TU`/`ETMP`: the real
sample's 40 `ETMP` records import and the model **solves successfully**
with the thermal load actually active (not a zero no-op -- material
thermal expansion is now also assigned). `GTMP`/`BTMP` (gradient/beam-
section temperature) are recognized and explicitly rejected when present
(0 records in this sample); MIDAS's T1/T2 top-fiber sign convention could
not be confirmed from accessible documentation, so it was not guessed at.

**Phase 7B, 7M and 7S (increment 1) — COMPLETE/verified (CI run
`39227258173`, 199/199 Debug/Release/ASan, 2026-10-19).** The GitHub
Actions "no runner assigned" blocker (present 2026-10-18 through the
morning of 2026-10-19) was resolved by the user increasing the account's
Actions quota; every commit accumulated while it was blocked
(`0823f8f` through `f10dca0`, plus the fix in `59b0035`) has now been
compiled and tested for the first time, in one CI run. One real bug was
caught (Phase 7S's centroid divisor, see below); everything else passed
on the first try. The real `xmidas/deneme1.json` bridge model now
imports, validates and **solves successfully end to end** with
KOPRUQ-SOLVER's own `LinearStaticSolver`.

**Phase 7T (temperature loads) — COMPLETE/verified (CI run `39237884916`,
215/215 Debug/Release/ASan, 2026-10-19).** This completes the approved
7M -> 7S -> 7T implementation order in full. Two genuine issues were
caught and fixed during verification (session log below): a test-design
flaw (a scale-normalized equilibrium ratio that reads exactly 1.0 for
any nonzero reaction when the applied-load resultant is exactly zero by
design) and a real, pre-existing solver robustness gap (`solveCases`
crashed on a model with zero free DOFs -- `SimplicialLDLT::compute()` on
a 0x0 matrix aborted in Debug builds specifically, masked in Release).

**Phase 7A — MIDAS NX JSON import audit — COMPLETE (2026-10-18, audit
only, no code changed).** `docs/MIDAS_JSON_DATA_AUDIT.md` and
`docs/SOLVER_CAPABILITY_MATRIX.md` record a full inventory of the real
`xmidas/deneme1.json` export and a capability-by-capability gap analysis
against the current solver.

Phase 5 — COMPLETE IN FULL, 5.1-5.6 (verified, CI run `39104401638`,
167/167 Debug/Release/ASan). Truss elements (5.2), rigid end offsets
(5.3), spring/link elements including bearings (5.4), general rigid
links/multi-point constraints (5.5) and the Timoshenko beam option (5.6)
are all implemented. 5.5 and 5.6 were initially deferred as out of scope
for the 2026-10-17 work period, then explicitly requested and completed on
2026-10-18 per the user's follow-up instruction. Phase 6 (6.1, 6.1b, 6.2,
6.2b) remains complete and unaffected; 6.3 is cancelled per the user's
2026-10-17 scope decision. See "Next autonomous action" for the current
approval checkpoint (stopped before Phase 7).

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
| Phase 4.3 | COMPLETE (verified) | Member point forces/moments, distributed moments, JSON `point`/`kind`; force-equilibrium metric fix; Debug/Release/ASan 76/76 on CI run `38887606677`; commits `48ad305`, `b2a605f`, `62cd955` |
| Phase 5.1 | COMPLETE (verified) | Beam end releases by static condensation; unstable-combination rejection; JSON `releases`; Debug/Release/ASan 83/83 on CI run `38894147560`; commits `4dee2b5`, `cbef2e8`, `41aec8a` |
| Phase 4.4 | COMPLETE (verified) | `validateModel()`/`validateModelOrThrow()` for direct C++ API callers; `Restraints` promoted to namespace scope; Debug/Release/ASan 97/97 on CI run `38896544673`; commits `dba9837`, `a942083` |
| Phase 6.1 | COMPLETE (verified) | Consistent mass matrix, Guyan mass condensation for released ends, `ModalSolver`; Debug/Release/ASan 104/104 on CI run `38902536528`; commits `63402a5`, `d12a40a`, `bca8206` |
| Phase 6.1b | COMPLETE (verified) | JSON `kopruq-modal-results` v1, CLI `modal` command; Debug/Release/ASan 110/110 on CI run `38906210839`; commits `86f90d1`, `31fb203` |

## Known blocking issue (2026-10-16, Phase 6.2) -- GitHub Actions runner provisioning, ACTIVE
- Branch `ci/phase6.2-response-spectrum`, commit `a7997c6`: every CI run since
  ~14:00 UTC fails in every job (`ninja-release`, `ninja-debug`, `ninja-asan`,
  `ninja-analyze`) within 2 seconds of being created, with `runner_id: 0`,
  `runner_name: ""` and an empty `steps` list -- i.e. no runner was ever
  assigned and nothing executed, so there is no build/test failure to
  diagnose. Fetching each job's logs returns an Azure Blob Storage
  `BlobNotFound` error, consistent with no log ever being written.
- Tried: a rerun of the stuck run (`POST .../rerun`, attempt 3, same result)
  and a brand-new run from an empty commit (`a7997c6`) -- both failed
  identically.
- Runs earlier the same day (through ~13:33 UTC, including this exact
  workflow and commit chain) completed normally, so this is not the
  workflow file, the code, or a one-off transient blip; it started
  partway through this session's heavy same-day usage (dozens of
  4-job Windows-runner CI runs).
- Likely cause: a GitHub Actions usage/spending limit or included-minutes
  quota reached for the account (Windows runners bill at a 2x multiplier,
  and today's session ran many full 4-job matrices). Could not confirm via
  API: `GET /users/zharput/settings/billing/actions` returned 404 with this
  token's permissions.
- This requires the user's action (GitHub account billing/Actions settings),
  per project rules on external/billing matters -- not something this
  session can resolve or safely retry around. Stopped retrying once the
  pattern was confirmed identical across a rerun and a fresh run, rather
  than continuing to burn further attempts against the same limit.
- Local builds remain separately blocked by the ESET `@Trojan.ML/Augur`
  false positive (see below); until one of these is resolved, no
  Debug/Release/ASan verification is currently possible for new changes.
  Phase 6.2's code sits on `ci/phase6.2-response-spectrum`, unmerged into
  `main`, pending a clean CI run.

### Update 2026-10-17 -- persists on a new day, not a daily quota
Retried with a fresh empty commit (`96447ee`) on 2026-10-17: run `39020322855`
failed identically in all 4 jobs -- 2-second lifetime, `runner_id: 0`, zero
steps. Since this is a new UTC day and the pattern is unchanged, it is not a
daily-reset quota; most likely a monthly Actions spending/included-minutes
limit that will not clear itself before the user acts (or the next billing
cycle). Stopped retrying again; waiting for the user to check
https://github.com/settings/billing (or the repo's organization billing
page) before attempting CI again.

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
- **Verified 2026-10-16 on CI** (run `38887606677`, commit `62cd955`):
  Debug **76/76**, Release **76/76**, AddressSanitizer **76/76** — the 61
  above + 15 `PointLoads`. Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38894147560`, commit `41aec8a`):
  Debug **83/83**, Release **83/83**, AddressSanitizer **83/83** — the 76
  above + 7 `Releases`. Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38896544673`, commit `a942083`):
  Debug **97/97**, Release **97/97**, AddressSanitizer **97/97** — the 83
  above + 14 `Validate`. Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38902536528`, commit `bca8206`):
  Debug **104/104**, Release **104/104**, AddressSanitizer **104/104** — the
  97 above + 7 `Modal`. Static analysis: 0 blocking.
- **Verified 2026-10-16 on CI** (run `38906210839`, commit `31fb203`):
  Debug **110/110**, Release **110/110**, AddressSanitizer **110/110** — the
  104 above + 5 `ModalResultsJson`/`Cli` GoogleTest cases + 1 CTest run of
  the real CLI executable (`cli.modal_portal_frame_example`). Static
  analysis: 0 blocking.
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

### 2026-10-16 — Phase 4.3: member point loads and distributed moments
- Approved: user "devam" on the recommended option 1.
- `MemberPointLoad` (force [kN] or moment [kNm] at a position fraction,
  local or global axes) in `LoadCase::point`; `MemberLoad::kind`
  (`Force`/`Moment`) for distributed moments [kNm/m]. Moments enter the
  consistent load vector through the shape-function derivatives
  (theta_z = dv/dx, theta_y = -dw/dx) and, for torsion, the linear
  functions. JSON v1 extended additively (`point`, `kind`).
- Files: `include/kopruq/loads/Loads.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `include/kopruq/io/ModelJson.hpp`, `schemas/model.schema.json`,
  `tests/unit/point_load_tests.cpp` (new, 15 tests),
  `tests/unit/io_tests.cpp` (schema check covers `point_load`),
  `CMakeLists.txt`.
- CI run `38886496742`: 3/75 failed in every configuration — the point
  moment about z and y and the partial distributed moment tests — only on
  `force_equilibrium_error < 1e-12` (value 1); all displacements and
  reactions matched. Root cause (pre-existing production defect exposed by
  moment-only loading): the metric divided round-off net force by
  round-off force magnitudes. Fix (`62cd955`): force scale also includes
  applied and reaction moments about the model's bounding-box centre
  divided by its diagonal; the metric can only decrease for force-loaded
  cases. Regression test also proves it still detects a perturbed
  displacement field.
- Debug / Release / ASan (run `38887606677`): 76/76 each; static analysis
  0 blocking; benchmark residuals unchanged.
- Known limitation: the per-element `normalized_force_error` has the same
  round-off/round-off form; it stayed < 1e-12 in all current tests but can
  be ill-conditioned for an element carrying only moments. Not changed
  (would alter the documented aggregate definition); recorded for review.
- Local build note: since Phase 4.2 the local `build/` trees no longer
  configure (no local `nlohmann_json`; installing it needs vcpkg compiler
  detection, which ESET blocks), so local compile-only checks are no longer
  possible; CI is the only build.
- Milestone status: **Phase 4.3 COMPLETE.**

### 2026-10-16 — Phase 5.1: beam end releases
- Approved: user "devam" on the recommended option (end releases).
- `Beam3DConnection::releases` (12 member-local flags, default none, so
  existing models/results are unchanged); `kopruq/elements/Releases.hpp`
  does static condensation of K and the member loads' equivalent nodal
  loads; recovery completes the released end's own displacement before
  computing end forces, so k*u - f vanishes there exactly. Unstable
  release combinations (rigid-body motion of the released DOFs with the
  retained ones held, e.g. torsion or axial released at both ends) are
  rejected via the eigenvalues of the diagonally scaled K_rr. JSON v1
  extended additively (`beams[].releases.{start,end}`).
- Files: `include/kopruq/elements/Releases.hpp` (new),
  `include/kopruq/core/Model.hpp`, `include/kopruq/elements/Beam3D.hpp`
  (`UnstableRelease`), `include/kopruq/assembly/MatrixAssembler.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `include/kopruq/io/ModelJson.hpp`, `schemas/model.schema.json`,
  `tests/unit/release_tests.cpp` (new, 7 tests),
  `tests/unit/io_tests.cpp` (schema check covers `releases`),
  `CMakeLists.txt`.
- CI run `38892389294`: Release passed 83/83; Debug and ASan failed to
  *build* `apps/cli/main.cpp` with MSVC C1128 ("number of sections
  exceeded object file format limit"). Root cause: the JSON
  reader/writer/CLI header stack is template-heavy enough that Debug and
  RelWithDebInfo(ASan) object files exceed the format limit without
  `/bigobj`; Release's optimizer folds enough sections away to pass
  anyway. `kopruq_solver_tests` already carried `/bigobj` for the same
  reason. Fix (`41aec8a`): `/bigobj` on `kopruq_solver_io` too (covers the
  CLI and all io/point-load/release test executables). Not a numerical or
  test-logic defect.
- Debug / Release / ASan (run `38894147560`): 83/83 each; static analysis
  0 blocking; benchmark residuals unchanged.
- Milestone status: **Phase 5.1 COMPLETE.**

### 2026-10-16 — Phase 4.4: validated model in the C++ API
- Approved: user "devam" on the recommended option (validated model).
- `kopruq::validateModel(model, restraints={}, cases={}, combinations={})`
  (new `include/kopruq/core/Validate.hpp`): reports every issue, not just
  the first, mirroring the JSON reader's semantic checks but addressed by
  index/id (no JSON document here): duplicate/zero ids, invalid
  material/section ranges, missing references, degenerate geometry,
  unstable end-release combinations (checked without solving, via the
  same condensation used at assembly), restraints on unknown nodes, and
  per-load-case/combination checks. `validateModelOrThrow()` throws
  `ModelValidationError` with the full list. Purely additive:
  `solveCases()`/`recover()` are unchanged, still reporting only the
  first problem via whatever exception assembly happens to throw.
- `Restraints` moved from a nested alias inside `LinearStaticSolver` to
  `kopruq::Restraints` in `Model.hpp` (needed as this validator's
  restraints parameter type); `LinearStaticSolver::Restraints` kept as an
  alias to it, so no existing call site changes.
- Files: `include/kopruq/core/Validate.hpp` (new),
  `include/kopruq/core/Model.hpp`,
  `include/kopruq/solvers/LinearStaticSolver.hpp`,
  `tests/unit/validate_tests.cpp` (new, 14 tests), `CMakeLists.txt`.
- Debug / Release / ASan (run `38896544673`): 97/97 each, first attempt;
  static analysis 0 blocking; benchmark residuals unchanged.
- Verified properties: every issue category the JSON reader checks is
  independently flagged with its own path; several simultaneous issues
  are all reported together, not just the first; both example models
  validate clean; the throwing variant carries the complete list.
- Milestone status: **Phase 4.4 COMPLETE.**

### 2026-10-16 — Phase 6.1: modal (free-vibration) analysis
- Approved: user "devam" without picking among the three proposed options
  (5.2 springs/offsets, 6.1 modal, 8 bridge loading); chose 6.1 as the
  lowest-formulation-risk, best-independently-verifiable increment, and
  proceeded per the "decide and report" rule when ambiguity remains after
  offering a clear default.
- `kopruq/elements/MassMatrix.hpp`: consistent 12x12 local mass matrix by
  Gauss quadrature of the same Hermite/linear shape functions and sign
  convention as stiffness/loads; `g_standard = 9.80665` turns
  `Material::unit_weight` into mass density for dynamics only (no new
  public field). `Releases.hpp::condensedMass()`: Guyan mass condensation
  matching the existing stiffness condensation.
  `MatrixAssembler::assembleMass()`. `kopruq/solvers/ModalSolver.hpp`:
  lowest N natural frequencies and mass-normalized mode shapes from the
  free-DOF generalized eigenproblem.
- Files: `include/kopruq/elements/MassMatrix.hpp` (new),
  `include/kopruq/solvers/ModalSolver.hpp` (new),
  `include/kopruq/elements/Releases.hpp`,
  `include/kopruq/assembly/MatrixAssembler.hpp`,
  `tests/unit/modal_tests.cpp` (new, 7 tests), `CMakeLists.txt`.
- CI run `38900022668`: 102/104 passed; 2 failed in every configuration,
  both **test bugs**, not production defects:
  - The eigenproblem residual test computed `K*phi - omega^2*M*phi` over
    the full global vector, including restrained DOFs, where that
    expression equals the support reaction (generally nonzero), not zero;
    it only vanishes on the free DOFs. Fixed by restricting to the free
    rows (found via `MatrixAssembler::classify()`, independent of
    `ModalSolver`'s internals); the mass-orthonormality checks in the same
    test were already correct (zero mode-shape entries at restrained DOFs
    make those terms vanish from `phi^T*M*phi` regardless).
  - The continuum-convergence test used `E*IZ`, but `IY` (0.02) `< IZ`
    (0.04) in that section, so the first (lowest-frequency) mode bends
    about the local y-axis, governed by `IY`; `sqrt(IY/IZ) = 0.707`
    explained essentially all of the observed discrepancy. Fixed to
    `E*IY`.
- Debug / Release / ASan (run `38902536528`): 104/104 each; static
  analysis 0 blocking; benchmark residuals unchanged.
- Known limitations: no JSON/CLI support yet for modal analysis (C++ API
  only); rotary (cross-sectional) inertia about the bending axes is not
  included, standard for slender Euler-Bernoulli beams; `numModes <= 0`
  returns every free-DOF mode, which is O(n^3) via a dense solve and not
  intended for large models.
- Milestone status: **Phase 6.1 COMPLETE.**

### 2026-10-16 — Phase 6.1b: JSON/CLI support for modal analysis
- Approved: user "devam" without picking among four proposed options;
  chose the smallest one (closing the gap just created in 6.1) per the
  "decide and report" rule.
- `kopruq/io/ModalResultsJson.hpp`: new, separate format
  "kopruq-modal-results" v1 (not folded into the static "kopruq-results"
  format, which has a fixed, unrelated key set) --
  `schemas/modal_results.schema.json`. CLI gained a "modal <model.json>
  [-o <out>] [-n <num-modes>]" command; argument parsing and file I/O
  factored into small shared helpers so "solve" and "modal" do not
  duplicate that logic. "solve"'s own behaviour is unchanged.
- Files: `include/kopruq/io/ModalResultsJson.hpp` (new),
  `include/kopruq/io/Cli.hpp`, `schemas/modal_results.schema.json` (new),
  `tests/unit/modal_io_tests.cpp` (new, 6 tests), `CMakeLists.txt`
  (+1 CTest CLI run).
- Debug / Release / ASan (run `38906210839`): 110/110 each, first
  attempt; static analysis 0 blocking; benchmark residuals unchanged.
- Verified properties: every mode-shape component serialized exactly;
  deterministic output; the published schema's top-level and per-mode
  keys match the writer's actual output; CLI exit codes 0/1/2/3/4 for
  the new command, including a default `-n` and an override.
- Milestone status: **Phase 6.1b COMPLETE.**

### 2026-10-17 — GitHub Actions blocker and optimization (Stage 1 of 3)
- After Phase 6.1b, Phase 6.2 (response spectrum analysis) was started on
  `ci/phase6.2-response-spectrum` (code + 6 tests written, one test-side
  spectrum-range bug found and fixed). From 2026-10-16 ~14:00 UTC onward,
  every CI run on that branch failed within ~2 seconds in every job, with
  `runner_id: 0` and zero steps -- no runner was ever assigned, no build or
  test ever ran. A rerun and a fresh run from an empty commit failed
  identically; the pattern persisted into 2026-10-17. Phase 6.2 remains
  **unverified and unmerged** on that branch; Phases up to 6.1b are
  unaffected.
- The user then asked for a GitHub Actions optimization for both KOPRUQ
  (which has no `.github/workflows` at all -- nothing to optimize there)
  and KOPRUQ-SOLVER: move Windows-independent verification to
  `ubuntu-latest`, gate Windows `.exe` production to manual dispatch /
  version tags, add caching, minimal `permissions`, and upload the real
  `.exe` artifacts, without breaking the working build. Plan presented and
  approved; working in 3 approved stages, reporting and waiting for
  approval between each.
- Measured actual cost from this session's own CI history via the GitHub
  API (job `started_at`/`completed_at` across 38 parseable runs): 1,159.6
  wall-clock minutes; at the Windows 2x multiplier, **~2,319 billable
  minutes in under 24 hours** -- exceeding the GitHub Free plan's entire
  2,000-minute monthly allowance in one session. This is the confirmed
  root cause of the runner-provisioning failures.
- Stage 1 (branch `ci/optimize-stage1-ubuntu`, commit `1fce4b4`): new
  `gcc-debug`/`gcc-release`/`gcc-asan` CMake presets; `ci.yml` rewritten
  for `ubuntu-latest` (apt-installed Ninja, Linux vcpkg bootstrap, same
  CLI smoke test/benchmarks against the Linux Release build); added
  caching of the vcpkg tool itself (previously only the built-package
  cache was cached). Source code was checked first and has no Windows/MSVC
  dependency (no `_MSC_VER`/`Windows.h`/`__declspec`; `CMakeLists.txt`
  already has a non-MSVC branch).
- Its first run (`39029543763`) failed identically to the Windows runs:
  all 3 jobs, 2-second lifetime, `runner_id: 0`, zero steps. **This
  confirms the blocker is an account-wide Actions spending/included-minutes
  limit shared across OS types, not a Windows-only quota** -- moving to
  Ubuntu does not by itself restore CI. Consequently, **whether this
  codebase actually compiles under GCC/Clang is still unverified**; Stage
  1's code is pushed but unmerged, pending the user clearing the billing
  limit.
- **Update 2026-10-17, after user confirmation**: runners became available
  again (account-wide blocker cleared). Retried: `gcc-debug` and `gcc-asan`
  passed outright on the first attempt (110/110 CTest each), confirming
  the codebase compiles and runs cleanly under GCC/Clang -- no portability
  defect anywhere. `gcc-release` failed only in the CLI smoke-test step:
  `"$cli" bogus; code=$?` runs under bash's default `-e` (errexit), which
  aborts the step on the command's nonzero exit (2, exactly as designed)
  before `code=$?` is ever reached -- a workflow-script bug carried over
  from porting the Windows PowerShell version (pwsh `run:` steps are not
  errexit by default, so the same pattern worked there). Fixed by isolating
  the intentionally-failing call with `||` instead of `;` (commit
  `f66da33`). Re-run (`39035585549`): all 3 jobs green, 110/110 each.
- Measured real cost (run `39035585549`): `gcc-release` 5.05 min,
  `gcc-debug` 4.80 min, `gcc-asan` 14.83 min wall-clock = 24.68 total
  minutes; at Ubuntu's 1x multiplier, **~24.7 billable minutes per run**,
  vs. the old 4-job Windows run's ~62 billable minutes -- already a ~60%
  cut with only 3 of 4 jobs moved (the 4th, `/analyze`, moves to the rare
  release workflow in Stage 2, which is most of the remaining gap).
- Merged to `main` (fast-forward, `f66da33`).
- Milestone status: **Stage 1 COMPLETE (verified).**

### 2026-10-17 — Stage 2: release.yml for Windows EXE production
- `release.yml` added: the full MSVC debug/release/asan/analyze matrix,
  the CLI smoke test, the benchmark run and vcpkg applocal DLL packaging
  carried over unchanged from the pre-split `ci.yml`; triggers only on
  `workflow_dispatch` or a `v*.*.*` tag push. Added vcpkg tool caching
  (same technique as Stage 1's Linux job).
- `workflow_dispatch` cannot target a workflow that only exists on a
  non-default branch (`POST .../dispatches` returned 404 against the
  feature branch), so verification used a temporary branch-push trigger,
  removed in a follow-up commit once confirmed working -- `main` never
  carried the temporary trigger.
- First run (`39037599675`): all 4 jobs green, 110/110 CTest in each of
  the 3 test jobs, 0 blocking static-analysis findings. The uploaded
  `kopruq-solver-windows-x64` artifact (3.2 MB) turned out to contain
  every GoogleTest executable and `gtest*.dll` alongside the real
  product, because `build/release/*.exe` matches everything the Release
  build produces, not just the CLI and benchmarks.
- Fixed (`42a4280`): narrowed the artifact's `path:` to the three actual
  product binaries (`kopruq_solver_cli.exe`,
  `kopruq_solver_benchmark.exe`, `kopruq_sparse_assembly_benchmark.exe`);
  none need a DLL, since Eigen and nlohmann-json are header-only (the
  DLLs in the first artifact belonged only to the test executables).
  Re-run (`39038848289`): all 4 jobs green again; artifact now 449.8 KB,
  verified (downloaded and listed) to contain exactly those three files.
- Removed the temporary trigger (`e08ab68`) and merged to `main`
  (fast-forward). Confirmed via `GET .../actions/workflows` that
  `release.yml` is now registered and `active` on the default branch
  (dispatchable going forward).
- Milestone status: **Stage 2 COMPLETE (verified).**

### 2026-10-17 — Stage 3: concurrency, permissions review, final comparison
- Added `concurrency: group: ${{ github.workflow }}-${{ github.ref }}` to
  both workflows: `ci.yml` with `cancel-in-progress: true` (concrete,
  observed waste this prevents: this session pushed many commits in quick
  succession to the same branch while iterating, and every superseded run
  kept consuming minutes to completion anyway), `release.yml` with
  `cancel-in-progress: false` (queue instead -- a half-finished release
  silently cancelled by a second dispatch is more surprising than useful,
  and releases are infrequent enough that queuing is never a real wait).
- Permissions reviewed: both workflows already carried only
  `permissions: contents: read`, sufficient for checkout, cmake/ctest,
  cache restore/save and `actions/upload-artifact`; no job needs anything
  broader. No change needed.
- The push+pull_request double-trigger case (a PR opened from a `ci/**`
  branch would fire both events for the same commit) is left as-is: this
  session's workflow never opens PRs against this repo, relying on the
  branch-push trigger alone, and narrowing it would break that pattern for
  no measured benefit. Documented as an accepted, currently-dormant
  tradeoff, not silently changed.
- Verified (run `39045959939`): all 3 `ci.yml` jobs green after adding the
  concurrency block; merged to `main` (fast-forward).
- Milestone status: **Stage 3 COMPLETE (verified). GitHub Actions
  optimization (all 3 stages) COMPLETE.**

## GitHub Actions optimization — final before/after comparison (2026-10-17)
Measured from this session's actual GitHub API data (job
`started_at`/`completed_at`), not estimated.

| | Before (single `ci.yml`, Windows, every push/PR) | After (`ci.yml` Ubuntu + `release.yml` Windows) |
|---|---|---|
| Routine push/PR | 4 jobs (debug/release/asan/analyze), windows-2022, 2x multiplier | 3 jobs (debug/release/asan), ubuntu-latest, 1x multiplier |
| Measured cost per routine run | ~62 billable min (session average, 38 runs, 1,159.6 wall-clock min total) | ~24.7 billable min (run `39035585549`: 24.68 wall-clock min x 1x) |
| Full MSVC matrix (/analyze, real .exe) | Every push/PR | Only `workflow_dispatch` or a `v*.*.*` tag (measured ~103-110 billable min per invocation, runs `39037599675`/`39038848289`) |
| Superseded runs on rapid re-push | Ran to completion anyway | Cancelled immediately (`cancel-in-progress`) |
| `.exe` artifacts | Discarded after each job | Downloadable (`kopruq-solver-windows-x64`, verified exact contents) |
| vcpkg tool (clone + bootstrap compile) | Repeated every run | Skipped on a cache hit |

**This session's actual trigger pattern (46 pushes in <24h) under each system:**
- Before (measured): ~2,319 billable minutes -- exceeded the GitHub Free
  plan's entire 2,000/month allowance in under a day; the confirmed cause
  of the runner-provisioning failures.
- After (projected from measured per-run costs, same 46 pushes + the 2
  release.yml verification runs actually used): 46 x 24.7 + 2 x 106 ~=
  1,136 + 212 = **~1,348 billable minutes, a ~42% cut even under this
  unusually heavy same-day iteration pattern**, and the new
  `cancel-in-progress` behavior (which did not exist in the "before"
  measurement) would reduce it further for any future session that
  pushes corrections in quick succession the way this one repeatedly did.
- For an ordinary usage pattern (routine pushes dominate, releases rare --
  e.g. 100 pushes + 5 releases a month): before ~6,200 billable min/month
  (3x over quota); after ~100x24.7 + 5x106 = 2,470 + 530 = **~3,000
  billable min/month, a ~52% cut**, bringing routine usage much closer to
  the Free plan's 2,000-minute allowance (the remaining gap is primarily
  the number of actual release builds, which the user controls directly
  by choosing when to dispatch/tag).

### 2026-10-17 — Phase 6.2 verified and merged (resumed after the CI work)
- Merged `main` into `ci/phase6.2-response-spectrum` to pick up the new
  Ubuntu/Windows CI split; pushed, and `ci.yml` ran on `ubuntu-latest` for
  the first time this branch's code had ever actually completed a run
  (every earlier attempt failed before reaching any test: first on the
  spectrum-range bug fixed on 2026-10-16, then on the account-wide Actions
  billing blocker).
- Result: 115/116 passed; `ResponseSpectrum.MassParticipationSumsToOne-
  WithAllModes` failed identically in all 3 jobs (deterministic, not a
  sanitizer/platform issue). Root cause: a genuine formulation gap, not a
  portability one -- `iota` was 1 at every translational DOF *including
  restrained ones*, so `Gamma_i = phi_i^T*M*iota` picked up a mass-
  coupling term through the restrained DOFs that no finite set of
  free-DOF modes can ever fully capture; `mass_participation_ratio`
  summed to ~0.9265, not 1.0, even with every free-DOF mode included.
- Fixed (`b207adf`): `iota` is now 1 only at free translational DOFs
  matching `direction` (checked via DOF index mod 6), 0 at rotations and
  restrained DOFs -- matching the modal participating-mass convention
  most structural software reports (SAP2000, ETABS). `total_mass` is now
  the free DOFs' own mass in that direction, not the whole structure's
  physical mass (the restrained supports' mass share is excluded by
  construction, which is the point of the fix). Updated
  `ResponseSpectrumSolver.hpp`'s formulation comment accordingly.
- Test-side (`5807f51`): `TotalMassMatchesIndependentPhysicalTotal`
  renamed to `TotalMassMatchesIndependentFreeDofReduction` and rewritten
  to independently assemble the global mass matrix, drop the restrained
  node, and compute `iota_free^T*Mff*iota_free` (asserted strictly less
  than the full physical mass); `SingleElementModeMatchesIndependent-
  DenseComputation`'s `iota` corrected to the free node only.
- Re-run (`39051080335`): **116/116 passed in all 3 jobs** (`gcc-debug`,
  `gcc-release`, `gcc-asan`). Merged to `main` (fast-forward).
- Note on verification going forward: per the new CI architecture (Stages
  1-3), Ubuntu `ci.yml` is now the routine verification evidence for every
  change, including this one; the full Windows MSVC matrix in
  `release.yml` is reserved for actual release events (manual dispatch or
  a version tag), not run per merge, by design -- this merge intentionally
  does not have a corresponding Windows run, consistent with the
  cost-reduction goal just completed.
- Milestone status: **Phase 6.2 COMPLETE (verified).**

### 2026-10-17 — Phase 6.2b: JSON/CLI support for response spectrum analysis
- Approved: user "devam" without picking among five proposed options;
  chose the smallest one (closing the gap 6.2 just created, same pattern
  as 6.1 -> 6.1b) per the "decide and report" rule.
- Model JSON v1 gains an optional, additive "response_spectra" array:
  named requests (direction, optional num_modes, a spectrum of
  {period, sa} points) round-tripping through the same ResponseSpectrumInput
  the C++ API takes. New, separate "kopruq-response-spectrum-results" v1
  format (not folded into kopruq-results or kopruq-modal-results): per-mode
  summary + SRSS-combined peak response, omitting each mode's full
  per-node/per-element AnalysisResults (C++ API only) to keep the document
  a practical size. New CLI "spectrum <model.json> [-o <out>]" command.
  examples/portal_frame.json gained a representative "EQX" case.
- Files: `include/kopruq/io/ModelJson.hpp`,
  `include/kopruq/io/ResponseSpectrumResultsJson.hpp` (new),
  `include/kopruq/io/Cli.hpp`, `schemas/model.schema.json`,
  `schemas/response_spectrum_results.schema.json` (new),
  `examples/portal_frame.json`, `README.md`,
  `tests/unit/spectrum_io_tests.cpp` (new, 9 tests),
  `tests/unit/io_tests.cpp` (schema check covers the 2 new definitions),
  `CMakeLists.txt` (+1 CTest CLI run).
- `ubuntu-latest` (run `39055492002`): all 3 jobs green on the first
  attempt, 125/125 each. Merged to `main` (fast-forward).
- Milestone status: **Phase 6.2b COMPLETE (verified).**

### 2026-10-17 — Scope change: Phase 6.3, 7, 8 cancelled (user decision)
- User instruction (verbatim intent): cancel Phase 7, Phase 8, and Phase
  6.3 in `docs/SOLVER_ROADMAP.md`; these will not be implemented. Revise
  the plan accordingly and rewrite the roadmap.
- Cancelled, removed from the roadmap (not deferred):
  - **Phase 6.3** -- linear buckling / P-Delta (geometric stiffness).
  - **Phase 7** -- time-dependent and nonlinear analysis (time-history
    integration/damping, geometric/material nonlinearity, incremental-
    iterative convergence controls).
  - **Phase 8** -- bridge engineering capabilities (construction stages,
    prestressing/creep/shrinkage/temperature, bearings/soil springs,
    moving loads/influence lines, bridge-specific load combinations).
- Nothing already implemented depended on these phases (6.1/6.2/6.2b are
  unaffected; no code references buckling, time-history, or any
  bridge-specific module), so this is a pure scope reduction with no code
  impact.
- `docs/SOLVER_ROADMAP.md` rewritten: the former Phase 9 (high-performance
  production solver), Phase 10 (KOPRUQ integration) and Phase 11
  (AI-assisted optimization) are renumbered to Phase 7, 8 and 9
  respectively, since nothing in them depended on the cancelled phases.
  A "Scope change" note at the top of the roadmap records the decision
  and points here for the full rationale.
- The `Product objective` paragraph ("initially optimized for bridges")
  was left unchanged -- the user cancelled specific roadmap phases, not
  the mission statement; flag for a future explicit decision if that
  statement should also be revised given Phase 8's cancellation.
- Milestone status: **Scope change recorded and applied; no code
  changed.**

### 2026-10-17/18 — Phase 5 completion: truss, rigid offsets, spring/link elements
User instruction (verbatim intent): complete Phase 5, then Phase 6, without
asking for approval between them; stop before Phase 7.

- **5.2 truss elements** (`include/kopruq/elements/Truss3D.hpp`): a
  pin-ended 3D truss implemented as a Beam3D with every rotational DOF
  released, reusing 5.1's static condensation rather than a new
  stiffness/mass/load/recovery path. First attempt released RX at both
  ends; CI (run `39060519026`) caught `Beam3DErrorCode::UnstableRelease`
  -- releasing torsion at both ends creates a genuine, separate zero-energy
  spin mechanism (decoupled from everything else), which Releases.hpp's
  stability check correctly rejects. Fixed by releasing RX at node2 only
  (commit `a1e8212`): releasing either single end already drives the
  retained torsional stiffness to exactly zero (the torsion block is
  rank-1), so the fix changes nothing mechanically, only avoids the
  both-ends-free degeneracy. Independent verification: dense comparison to
  the classical EA/L*c c^T truss stiffness formula; a statically
  determinate two-bar A-frame matched against hand statics (method of
  joints) and the unit-load/virtual-work theorem, cross-checked against
  elongation-derived axial force.
- **5.3 rigid end offsets** (`include/kopruq/elements/Offsets.hpp`): a
  rigid-body kinematic transform G (u_point = u_node + theta_node x e)
  composed around the existing offset-point stiffness/mass/loads
  (K_node = G^T K_point G); zero offsets are an exact no-op, verified by a
  bit-identical regression test against the pre-existing beam path.
  Independent verification: rigid-body kinematics of G itself; G^T K G
  composed in the test from Beam3D::globalStiffness (an already-verified
  primitive) vs. the production-assembled matrix; equilibrium/energy
  residuals for a loaded, offset cantilever.
- **5.4 spring/link elements** (`include/kopruq/elements/SpringLink.hpp`):
  a two-node, six-independent-DOF discrete spring (no coupling between
  DOFs, unlike a beam), covering bearings/isolators/soil springs. CI (run
  `39068274432`) caught a genuine formulation defect: with non-coincident
  nodes, the decoupled per-DOF stiffness has no lever-arm coupling to the
  node separation, so it lacks a rigid-body null space under rotation --
  `Spring.TiltedLoadedSpringSatisfiesEquilibrium`'s moment_equilibrium_error
  was 0.86, not near-zero. Fixed (commit `189df29`) by requiring node1/node2
  to be at the same location (`checkSpringNodesCoincident`, enforced in
  assembly, `Validate.hpp` and the JSON reader) rather than generalizing the
  element -- the documented primary use (a single-point bearing/isolator)
  was always coincident-node, so nothing that ever worked regresses. A
  second CI failure (run `39069440192`) was a pure editing mistake --
  the new function landed outside its namespace/includes -- fixed in commit
  `7a6fb83` with no logic change.
- Independently derived expected results used statics, virtual-work and
  plain rigid-body kinematics throughout; none were computed by calling
  the production code under test. Both CI-caught issues were genuine
  formulation/mechanism defects this session's own equilibrium-residual
  tests were specifically designed to catch, not test-authoring mistakes --
  recorded here rather than glossed over.
- Final verification: CI run `39069625215`, commits `2228863`, `a1e8212`,
  `9126760`, `189df29`, `7a6fb83`. Debug/Release/ASan: **149/149 PASS**
  (up from 133 before this session's work; 16 new tests across
  `truss_tests.cpp`, `truss_io_tests.cpp`, `offset_tests.cpp`,
  `spring_tests.cpp`, `spring_io_tests.cpp`).
- Deferred, not attempted: general multi-point rigid links/constraints
  (different mechanism from 5.3's two-node offset; needs DofManager-level
  dependent-DOF elimination or Lagrange multipliers); the Timoshenko beam
  option (no stated requirement; would touch the most heavily-verified
  core element, Beam3D.hpp).
- Milestone status: **Phase 5 COMPLETE as scoped. Phase 6 already complete
  (6.1, 6.1b, 6.2, 6.2b; 6.3 cancelled) -- confirmed, no further action
  needed there.** Stopping before Phase 7 per the user's explicit
  instruction.

### 2026-10-18 — Phase 5 completion: rigid links (5.5) and Timoshenko beam (5.6)
User instruction (verbatim intent): complete the items deferred from
Phase 5 -- general rigid links and the Timoshenko beam option -- in full,
after reviewing the prior progress report.

- **5.6 Timoshenko beam**: `Section::Asy`/`Asz` (effective shear area,
  default 0 = rigid in shear). `Beam3D::localStiffness` now computes
  ay/by/cy/dy (and az/bz/cz/dz) with the standard (1+Phi) correction,
  Phi = 12EI/(GAs L^2); every call site building `Beam3DProperties` from a
  `Section` updated to pass Asy/Asz through (otherwise the correction
  would silently never apply). Mass matrix intentionally left
  Euler-Bernoulli (documented limitation, not an oversight).
  Independent verification: Asy=Asz=0 reproduces the plain
  Euler-Bernoulli formulas bit-for-bit; a cantilever tip deflection/
  rotation under a transverse point load matches the closed-form
  Timoshenko solution w=PL^3/(3EI)+PL/(GAs), theta=PL^2/(2EI) (theta
  independent of shear, as theory predicts). **Test-authoring bug caught
  by CI** (run `39103459292`): the point load's `position` was 0.0 (node1,
  the fixed end) instead of 1.0 (node2, the free tip), so every case
  measured a zero-displacement no-op; fixed in commit `a2f5290`, no
  production code touched.
- **5.5 general rigid links**: `RigidLink.hpp`, a slave node's 6 DOFs
  entirely determined by a master's via rigid-body kinematics (one master,
  many slaves possible), implemented as a DOF-elimination layer wrapped
  around the existing, unmodified assembled K/F/U
  (K_reduced=T^T K T, etc.). To keep the overwhelmingly common
  no-rigid-links case at zero risk, `LinearStaticSolver::solveCases` has
  an early branch to a separate `solveCasesWithRigidLinks` function
  (a parallel implementation, not conditionals threaded through the
  original) when `m.rigid_links` is non-empty. `ModalSolver`/
  `ResponseSpectrumSolver` explicitly reject models with rigid links
  (not yet supported there) rather than silently ignoring the constraint.
  Independent verification: the reduction matrix's kinematics checked
  directly against the cross-product definition; a cantilever with a
  rigid arm past its tip, loaded at the arm's end, checked against the
  closed-form cantilever-under-combined-force-and-moment solution (the
  equivalent load from elementary statics) and, separately, against the
  rigid-body kinematic relationship itself.
- Final verification: CI run `39104401638`, commits `edc0e6d`, `a2f5290`,
  `18b6a70`. Debug/Release/ASan: **167/167 PASS** (up from 149; 18 new
  tests across `timoshenko_tests.cpp`, `timoshenko_io_tests.cpp`,
  `rigid_link_tests.cpp`, `rigid_link_io_tests.cpp`).
- Milestone status: **Phase 5 COMPLETE IN FULL (5.1-5.6), no remaining
  deferred items.** Stopping again before Phase 7 per the still-standing
  instruction from the 2026-10-17 approval checkpoint.

### 2026-10-18 — Phase 7A: MIDAS NX JSON import audit (first execution, audit only)
User instruction (verbatim intent): make KOPRUQ-SOLVER able to import a real MIDAS Civil NX JSON
export, analyze it with KOPRUQ-SOLVER's own solver, and export JSON results, independently
verified against analytical references first and MIDAS's own results where available. Added
Phases 7A-7F to the roadmap, kept Phase 8/9 deferred as before. First execution explicitly
scoped to audit + roadmap update only, ending at a mandatory approval checkpoint before any
Phase 7B implementation.

- Inspected `xmidas/deneme1.json` directly (PowerShell `ConvertFrom-Json`; no Python available
  in this environment). Full findings: `docs/MIDAS_JSON_DATA_AUDIT.md`. Summary: 106 nodes, 94
  beam elements, 2 materials, 5 sections, 9 fully-fixed supports, 4 elastic links, 5 rigid links,
  5 load cases (self-weight, 3 superimposed dead load stages, 1 temperature), 40 member loads, 40
  element-temperature records, 1 modal case (Lanczos/20 modes), 2 response-spectrum cases
  (Eurocode 8 elastic spectrum, CQC). This is a full MIDAS database dump (150+ categories), not a
  minimal structural subset -- most categories are empty for this model.
- Official MIDAS documentation (`support.midasuser.com` API manual and Export-to-JSON pages)
  returned HTTP 403 (authenticated support portal) -- not used, per the task's own fallback
  instruction. One public page (concrete material modulus formula) was reachable via search and
  corroborates finding (1) below.
- Five real gaps found, in blocking-severity order: (1) material elastic modulus/Poisson/unit
  weight are not in the export -- only a design-code grade name (`C35/45` etc.) and
  characteristic strength; MIDAS derives `E` internally via a code formula. (2) section area/
  Iy/Iz/J are not in the export -- only a shape code (`SB`,`B`,`SR`) and raw dimensions; needs a
  section-shape property library. (3) the sample's 4 elastic links connect **non-coincident**
  nodes, which Phase 5.4's `SpringLink` deliberately rejects (a real formulation limit found and
  enforced by CI during that phase, not an oversight) -- this specific model cannot be imported
  onto `SpringLink` as it stands. (4) the sample's rigid links coexist with modal/response-
  spectrum requests, but `ModalSolver`/`ResponseSpectrumSolver` (Phase 5.5) explicitly reject any
  model with rigid links. (5) element-temperature loads (`ETMP`/case `TU`) have no implementation
  in any phase. `docs/SOLVER_CAPABILITY_MATRIX.md` lays out every capability the sample touches
  against current solver status.
- MIDAS's own modal frequencies (all 20 modes) are readable directly from
  `xmidas/deneme1_Eigenvalue.out` (plain text) -- useful, ready-made independent reference data
  for a future Phase 7E comparison. No numeric static/spectrum results were found in readable
  form; those live in binary `.rst`/`.mec`/`.nfxp` files, not parsed in this audit.
- `docs/SOLVER_ROADMAP.md` updated with Phases 7A (COMPLETE, this execution) through 7F (not
  started), inserted after Phase 7 (production solver, also not started) and before Phase 8/9
  (still deferred, unchanged).
- Milestone status: **Phase 7A COMPLETE. No code changed. Mandatory approval checkpoint: Phase
  7B (the actual importer) requires explicit user approval before starting**, per the user's own
  instruction for this execution.

### 2026-10-19 — Phase 7B implementation (CI-blocked) + Phase 7S/7M/7T roadmap addition
Phase 7B (MIDAS JSON importer) was implemented per the user's "devam" approval following the
Phase 7A report: `MidasMaterialLibrary.hpp`, `MidasSectionLibrary.hpp`, `MidasOrientation.hpp`
(the local-axis convention, confirmed against MIDAS's own manual text found via web search after
WebFetch to midasuser.com returned HTTP 403 throughout), and `MidasJsonImporter.hpp`, plus
`xmidas/deneme1.json` added to the repo so CI can read it. Two explicit clarifying questions were
asked and answered before implementing: missing material Poisson/unit-weight -> use disclosed
standard values rather than fail; local axis convention -> user asked to confirm via MIDAS itself,
then supplied a documentation link, whose content was retrieved via web search (not WebFetch,
which stayed blocked) and matched the general convention already drafted.

GitHub Actions has been returning "no runner assigned" (empty steps, `runner_id:0`, 2-3 second
failures) since partway through 2026-10-18, continuing into 2026-10-19 -- the same pattern as an
earlier, account-wide billing/quota episode in this project's history, reported to the user twice;
not resolved as of this entry. **No commit since `0823f8f` has been compiled or run anywhere.**
Import counts (106 nodes, 94 beams, 2 materials, 5 sections, 9 supports, 7 individual rigid
links, 4 load cases, 70 member loads) were cross-checked independently via PowerShell
`ConvertFrom-Json` on the raw source file, but that only confirms the *expected* values the C++
test asserts against, not that the C++ itself is correct.

Immediately after, the user opened a second, separate instruction: extend the roadmap (audit/
update only, no implementation) with three new general-purpose capabilities -- Phase 7S
(parametric/general section property engine), Phase 7M (Eurocode material library by grade
name), Phase 7T (uniform/gradient temperature analysis) -- generalizing what 7B's MIDAS-specific
helpers currently do ad hoc, plus adding thermal/temperature support KOPRUQ-SOLVER has never had
in any phase. `docs/SOLVER_ROADMAP.md` updated (7S/7M/7T inserted between 7B and 7C, with scope,
required properties, section families subject to per-family approval, and explicit "not yet
implemented" status); nothing implemented. Milestone status: **roadmap updated; mandatory
approval checkpoint, per this execution's own explicit instruction.**

### 2026-10-19 — GitHub Actions blocker resolved; Phase 7B/7M/7S(1) verified in one run
User increased the GitHub Actions quota on their account; the "no runner assigned" blocker
present since 2026-10-18 cleared immediately (run `39223638115` got a real runner on the first
rerun attempt after the increase). All six commits accumulated while blocked
(`0823f8f`,`281b2b2`,`bd5ebbe`,`64ce16f`(docs),`557d375`,`f10dca0`) were compiled and tested for
the first time in that run.

Result: only ONE real defect, in Phase 7S's brand-new polygon section engine -- the centroid
accumulator divided by 12 instead of 6 (a transcription slip next to the correctly-12/24-divided
second-moment accumulators), giving centroids exactly half their correct value. Invisible in
every test using an origin-centered section (0/12 == 0/6 == 0); caught only by
`OffsetRectangleCentroidAndParallelAxisAreConsistent`, a test written specifically to check
translation invariance. Fixed in commit `59b0035`, re-verified: CI run `39227258173`,
**199/199 Debug/Release/ASan, all green**.

Everything else -- Phase 7B's full MIDAS importer (including orientation conversion and the
end-to-end real-sample test) and Phase 7M's Eurocode material library -- passed on the very
first compile, including the test that matters most: the real `xmidas/deneme1.json` bridge
model imports, passes `validateModel`, and **solves successfully** through
`LinearStaticSolver::solveCases` with no exception and `status.success == true`. This is the
first time this session's MIDAS import work has been verified by anything other than hand
analysis and independent PowerShell cross-checks.

Milestone status: **Phase 7B COMPLETE. Phase 7M COMPLETE. Phase 7S increment 1 COMPLETE
(general polygon engine + hollow circle; I/H/T/channel/angle/girder sections still explicitly
deferred, each needing its own approval). Phase 7T (temperature loads) is next.**

### 2026-10-19 — Phase 7T implementation and verification (2 issues caught and fixed)
`Material::thermal_expansion`, `MemberThermalLoad`/`LoadCase::thermal`, and
`include/kopruq/loads/ThermalLoads.hpp` (uniform + gradient equivalent nodal loads, derived via
the same virtual-work integral already used throughout this project) implemented and wired into
`LinearStaticSolver::assembleLoadCase` via the existing `accumulate()`/`frameOf()` machinery.
JSON I/O, schema and `Validate.hpp` updated. First CI run (`39233490979`) caught a test-design
flaw in `ThermalLoads.FreeUniformExpansionMatchesAlphaDeltaTL`: `force_equilibrium_error` is a
ratio that reads exactly 1.0 for ANY nonzero reaction when the applied-load resultant is exactly
zero (true by design for a thermal load, which has no real external resultant) -- fixed
(`9c53fd1`) by checking the raw `force_balance_norm` instead. Second CI run (`39235802175`)
caught a real, pre-existing solver robustness gap:
`ThermalLoads.FullyRestrainedUniformGivesEAAlphaDeltaT` and
`...FullyRestrainedGradientGivesEIKappaMoment` both restrain every DOF of a 2-node model (by
design, to measure the fixed-end thermal force/moment directly) -- a legitimate, trivially-solved
model, but `SimplicialLDLT::compute()` on the resulting 0x0 `Kff` aborted the process in the
Debug build specifically (Release tolerated it, masking the issue there). Fixed (`1dd5323`) in
both `solveCases` and `solveCasesWithRigidLinks`: a model with zero free DOFs now skips the
factorization/solve entirely rather than relying on Eigen's undefined-across-configurations
behavior for a 0x0 matrix.

Final verification: CI run `39237884916`, commits `bd0a079`, `9c53fd1`, `1dd5323`.
**Debug/Release/ASan: 215/215 PASS** (up from 199; 16 new tests across `thermal_load_tests.cpp`,
`thermal_io_tests.cpp`). This completes the 7M -> 7S -> 7T implementation order in full.

### 2026-10-19 — MIDAS ETMP wired to Phase 7T thermal loads
Per the user's explicit "2. MADDEYE DEVAM EDELİM" instruction, picked item 2 from the previous
report's candidate list: wire Phase 7B's MIDAS importer to Phase 7T instead of rejecting
`TU`/`ETMP`. Inspected the real export's `ETMP` records directly (PowerShell `ConvertFrom-Json`):
all 40 are the simple `{ID, LCNAME, GROUP_NAME, TEMP}` shape, `TEMP=40.0` for every one -- a
single uniform value, confirmed (via a web search of MIDAS's own documentation, since
support.midasuser.com itself returns HTTP 403) to be a SEPARATE category (`GTMP`) from gradient
temperature, whose `TZ`/`TY` fields and unconfirmed T1/T2 top-fiber sign convention are
deliberately NOT mapped -- `GTMP`/`BTMP` are recognized and rejected explicitly instead, with 0
records in this sample so that path is untested against real data (covered by three new tests
against a minimal hand-built MIDAS-shaped JSON snippet instead).

CI (run `39245168495`) caught a compile error (not a logic bug): a plain `R"(...)"` raw string
literal was terminated early by the `)"` sequence embedded in `"EN04(RC)"` inside the test file's
own synthetic JSON content -- fixed (`b43f605`) with a custom delimiter (`R"json(...)json"`).

Final verification: CI run `39247142910`, commits `2b19d2a`, `b43f605`. **Debug/Release/ASan:
219/219 PASS** (up from 215; 4 new tests). The real `xmidas/deneme1.json` model, now with its
`TU` thermal case actually active (not a zero no-op -- `MidasMaterialLibrary.hpp` also assigns
the EN1992-1-1 S3.1.3(5) thermal expansion coefficient), still **solves successfully**.

## Next autonomous action
Phase 3.2, Phase 4, Phase 5 (5.1-5.6), Phase 6 (6.1, 6.1b, 6.2, 6.2b; 6.3 cancelled), Phase 7A,
7B (including ETMP thermal wiring), 7M, 7S(1) and 7T are all complete and verified -- CI run
`39247142910`, 219/219 Debug/Release/ASan. Candidate next steps (say which, or propose a
different scope):
1. Phase 7C/7D: MIDAS-format CLI input (`--format midas-nx-json`) and JSON results export.
2. **Phase 7 (production solver, unrelated to MIDAS import)**: sparse direct/iterative strategy,
   optional SuiteSparse, profiling/parallelism -- still not started.
3. Extend rigid-link support to ModalSolver/ResponseSpectrumSolver, or cut a `v0.1.0` release tag.
4. Confirm MIDAS's GTMP T1/T2 sign convention (e.g. from the user, who may have access to the
   official API manual) to unblock gradient-temperature import, if ever needed.
