# KOPRUQ-SOLVER — Phase 3.2 Requalification Report

Date: 2026-10-16. Commit under test: `1ed5ae4` (branch `ci/r5-asan-analyze`,
fast-forwarded into `main`). Evidence source: GitHub Actions run
`38872157394`, `windows-2022`, MSVC 14.4x + Ninja + vcpkg at manifest
baseline `20ceb8ad`, CMake presets `ninja-debug`, `ninja-release`,
`ninja-asan`, `ninja-analyze`.

This report states what the current test suite **independently verifies**
and what it **does not**. It is a requalification after the loss of the
historical 25-test suite; it does not restore or reproduce that suite.

## 1. Result summary

| Gate | Result |
|---|---|
| Debug full CTest | **30/30 PASS** |
| Release full CTest | **30/30 PASS** |
| AddressSanitizer (`/fsanitize=address`, RelWithDebInfo) full CTest | **30/30 PASS**, no sanitizer reports |
| MSVC static analysis (`/analyze`, project code only) | **0 blocking findings**. 142 unique warnings, all C6326 on GoogleTest assertion lines in `tests\` (the macros expand to `switch (0) case 0: default:`); none in `include\kopruq` or `apps\` |
| Test inventory | 30 `TEST` cases in 6 dedicated files = 30 CTest-registered tests; no `DISABLED_`/`GTEST_SKIP`; no test file ever deleted in Git history; `core_tests.cpp` unchanged since baseline `1f67b16` |
| CLI smoke (Release) | `--version` exit 0, `--help` exit 0, unknown command exit 2 — PASS |
| Benchmarks 100 / 1,000 / 10,000 elements | Run; normalized residual ≤ 3.5e-18; all phases scale linearly (§4) |

## 2. Test inventory

| File | Suite | Tests | Milestone |
|---|---|---:|---|
| `tests/unit/core_tests.cpp` | `ElementRecovery` | 2 | preserved pre-requalification tests |
| `tests/unit/beam3d_requalification_tests.cpp` | `Beam3DRequalification` | 5 | R1 |
| `tests/unit/assembly_requalification_tests.cpp` | `AssemblyRequalification` | 5 | R2 |
| `tests/unit/linear_static_solver_requalification_tests.cpp` | `LinearStaticRequalification` | 10 | R3 |
| `tests/unit/recovery_requalification_tests.cpp` | `RecoveryRequalification` | 4 | R4 |
| `tests/unit/solver_diagnostics_tests.cpp` | `SolverDiagnostics` | 4 | R5 |
| **Total** | | **30** | |

All expected values come from test-local analytical formulas or
independently written dense references, never from the production code
under test.

## 3. Verified capabilities

### Beam3D element (R1)
- 12×12 Euler–Bernoulli local stiffness: axial, torsional and both bending
  planes, coefficient values and signs (relative error ≤ 1e-12).
- Symmetry, finiteness, rank 6 with exactly six rigid-body modes
  (zero-mode threshold `1e-10·max|λ|`).
- Local axes and global transformation for horizontal, vertical, inclined
  and reversed members; strain-energy invariance under transformation.
- Rejection of zero length, parallel reference vector, non-finite and
  non-positive properties.

### DOF numbering and sparse assembly (R2)
- Deterministic DOF numbering by sorted node ID, independent of input order.
- Duplicate node ID rejection (**production defect found and fixed**,
  `07f7344`).
- Single-element 12×12 and two-element shared-node 18×18 global matrices vs.
  independent dense assembly (relative Frobenius error < 1e-11).
- Restraint classification, connectivity components, orientation cases,
  invalid references (missing node, empty model, duplicate element ID).

### Linear static solution (R3)
- Analytical cantilever tip responses — axial `FL/EA`, flexural `FL³/3EI`
  and `FL²/2EI`, torsional `TL/GJ`, combined force + moment superposition —
  relative error ≤ 1e-9; linearity check against summed single-load cases.
- Free/restrained partition and `reduceKff` vs. a hand-built sparse system;
  load-vector assembly and non-finite load rejection.
- Independent 3D portal frame (18 DOF, column + beam): fresh dense assembly,
  partition and LDLT solve match production displacements and reactions
  (≤ 1e-9 / 1e-8 relative).
- Status handling: no restraints, empty model, singular system
  (pinned beam with free rigid-body rotations).
- Bit-identical results on repeated solves; per-load-case equilibrium.

### Recovery (R4)
- Reactions on restrained DOFs from global statics (R3) and from the
  independent portal-frame reference.
- Element local/global end forces for an inclined (3-4-5) member and for
  each member of a two-element portal frame, vs. independent
  recomputation; per-element force and moment equilibrium (< 1e-9).
- Strain energy: element `½uᵀku`, total, and external work `½UᵀF`
  (**previously never computed — implemented**, `643c829`); energy error
  < 1e-10 in all cases, consistent with Clapeyron's theorem.
- Recovery does not refactorize: verified by code inspection, not by test.

### Solver diagnostics (R5)
- `residual_norm = ‖Kff·u − F‖` and `relative_residual = residual_norm /
  (‖Kff‖_F‖u‖ + ‖F‖)` (**previously never populated — implemented**,
  `506a8be`): definition checked against an independent dense `Kff`
  (relative residual < 1e-14); zero load gives exactly 0, not NaN.
- `NonFiniteSolution` status (**previously never produced — implemented**):
  deterministic overflow case (E = 1e-300, F = 1e308) is rejected instead
  of reported as success.
- Aggregate `element_equilibrium_error` / `element_moment_equilibrium_error`
  (**previously never populated — implemented**): force-weighted
  imbalance over all elements, matched against a test-local aggregation of
  a portal frame's end forces and bounded by the per-element maximum.

### Performance and robustness (R5) — see §4
- O(elements × nodes) lookups in assembly and recovery **found and fixed**
  (`734b2fc`); numerical output unchanged bit-for-bit.
- No memory errors under AddressSanitizer across the full suite; no
  static-analysis findings in production code.

## 4. Benchmarks (Release, CI runner, median of 5 after warm-up)

Model: straight cantilever chain of `n` Beam3D elements, node 1 fixed, unit
load at the tip. Times in milliseconds, run `38872157394`.

| Elements | DOFs | Assembly | Kff reduction | Symbolic | Numeric | Solve | Recovery | Normalized residual |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | 606 | 0.273 | 0.111 | 0.175 | 0.023 | 0.009 | 0.350 | 3.5e-18 |
| 1,000 | 6,006 | 2.714 | 1.086 | 1.628 | 0.284 | 0.102 | 3.746 | 1.1e-18 |
| 10,000 | 60,006 | 30.50 | 10.31 | 17.56 | 3.28 | 1.03 | 40.29 | 3.1e-19 |

Growth from 1,000 to 10,000 elements (10×) within this run: assembly 11.2×,
Kff reduction 9.5×, symbolic 10.8×, numeric 11.5×, solve 10.1×, recovery
10.8× — linear in every phase. Before the lookup fix (run `38861593856`,
commit `1a42c61`) assembly grew 32× and recovery 48× over the same range.

Absolute timings vary by about 1.5–2× between CI runs on shared runners
(10,000-element assembly: 19.4 ms in run `38863504705`, 30.5 ms here), so
only within-run ratios are meaningful. Residuals are identical in every run
before and after the fix (abs 0.0467754, normalized 3.14632e-19 at 10,000
elements).

Limits of this evidence: runner timings, not the developer workstation; a
single banded chain topology, which has no fill-in and is a best case for
sparse LDLT; sizes up to 10,000 elements only.

## 5. Not verified / not implemented

| Item | Status |
|---|---|
| `StaticSolution::fixed` | Declared, **never populated**; its intended content (prescribed displacements or reactions on fixed DOFs) is not specified anywhere — needs a decision |
| `SolverStatusCode::NumericalFailure` | Declared, never produced (used only as the default-initialized code) |
| Analytical closed-form check of the UZ–RY (Iy) bending plane under tip load | Covered only via stiffness coefficients (R1) and the dense portal references (R3, R5), not by a dedicated analytical cantilever |
| Recovery re-assembles `K` instead of reusing the solver's matrix | Correct but ~2× assembly work; avoiding it needs an API change |
| Benchmarks on non-banded (realistic bridge) meshes and > 10,000 elements | Not run |
| `Truss3D`, `Spring` | Header placeholders with an ID only — no stiffness, not assembled, not tested |
| `schemas/model.schema.json`, `results.schema.json`, `ModelSchema.hpp` | Stubs; no import/export exists |
| Distributed loads, self-weight, load cases/combinations, releases, offsets, modal/buckling/nonlinear analysis | Not implemented (Phase 4+) |
| CLI model input / solve path | Not implemented; CLI only prints version/help |
| Local builds on the developer workstation | Blocked by ESET `@Trojan.ML/Augur` false positive; IT exclusion requested; all verification runs on CI |

## 6. Defects found and fixed during requalification

| Commit | Defect |
|---|---|
| `07f7344` | `DofManager` accepted duplicate node IDs (check ran before the index was populated) |
| `643c829` | Strain-energy fields declared but never computed |
| `734b2fc` | Quadratic node/property lookups in assembly and recovery |
| `506a8be` | Residual fields and aggregate element-equilibrium fields never populated; non-finite solutions reported as success |

Test-harness defects corrected in R2 (wrong reference `G`, uninitialized
dynamic matrix, wrong DOF map, uninvoked lambda in `EXPECT_THROW`) are
recorded in `docs/REQUALIFICATION_STATUS.md`.

## Addendum — Phase 4.1 (2026-10-16)

Since this report, Phase 4.1 (CI run `38874706685`, commit `9389ab3`) added
member loads, self-weight, load cases and combinations with 11 independent
tests; Debug/Release/ASan are now 41/41. Two §5 items changed:
- The UZ–RY (Iy) bending plane now has dedicated analytical cantilever
  checks (`MemberLoads.CantileverUniformLocalZUsesIyAndThetaYSign`,
  `GlobalLoadOnInclinedMemberActsPerTrueLength`).
- `StaticSolution::fixed` now holds reactions on restrained DOFs.
Distributed loads, self-weight and combinations are implemented; releases,
offsets and further analysis types remain unimplemented.

## Addendum — Phases 4.1b–4.3 (2026-10-16)

CI run `38887606677` (commit `62cd955`): Debug/Release/ASan 76/76. Added
projected global loads (4.1b), JSON model/results I/O and CLI (4.2), member
point loads and distributed moments (4.3). Defects found and fixed in this
period: gtest discovery timeout on cold Debug runners (`cecbe0d`, build
only), signed-integer ids rejected by the JSON reader (`24f1c65`), and the
global force-equilibrium metric returning ~1 for moment-only loading
(`62cd955`). Open: the per-element normalized force error has the same
round-off/round-off form for elements carrying only moments.

## Addendum — Phase 5.1 (2026-10-16)

CI run `38894147560` (commit `41aec8a`): Debug/Release/ASan 83/83. Added
beam end releases (moment hinges, pinned/released DOFs) by static
condensation, with unstable-combination rejection and JSON support. One
build-only defect found and fixed in this period: MSVC C1128 on Debug/ASan
building the JSON/CLI header stack without `/bigobj` (`41aec8a`); no
numerical or test-logic defect.

## Addendum — Phase 4.4 (2026-10-16)

CI run `38896544673` (commit `a942083`): Debug/Release/ASan 97/97, first
attempt, 0 blocking static analysis findings. Added `validateModel()` for
direct C++ API callers, mirroring the JSON reader's checks. No defects
found during this increment.

## Addendum — Phase 6.1 (2026-10-16)

CI run `38902536528` (commit `bca8206`): Debug/Release/ASan 104/104. Added
modal (free-vibration) analysis: consistent mass matrix, Guyan mass
condensation for released ends, `ModalSolver`. Two test bugs found and
fixed in this period (full-vector residual including restrained DOFs; wrong
section property in the continuum-convergence check) -- no production
defect.

## Addendum — Phase 6.1b (2026-10-16)

CI run `38906210839` (commit `31fb203`): Debug/Release/ASan 110/110, first
attempt, 0 blocking static analysis findings. Added JSON
("kopruq-modal-results" v1) and CLI (`modal` command) support for modal
analysis. No defects found during this increment.
