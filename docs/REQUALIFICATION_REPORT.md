# KOPRUQ-SOLVER — Phase 3.2 Requalification Report

Date: 2026-10-16. Commit under test: `734b2fc` (branch `ci/perf-id-lookups`,
fast-forwarded into `main`). Evidence source: GitHub Actions run
`38863504705`, `windows-2022`, MSVC 14.4x + Ninja + vcpkg at manifest
baseline `20ceb8ad`, CMake presets `ninja-debug` / `ninja-release`.

This report states what the current test suite **independently verifies**
and what it **does not**. It is a requalification after the loss of the
historical 25-test suite; it does not restore or reproduce that suite.

## 1. Result summary

| Gate | Result |
|---|---|
| Debug full CTest | **26/26 PASS** |
| Release full CTest | **26/26 PASS** |
| Test inventory | 26 `TEST` cases in 5 dedicated files = 26 CTest-registered tests; no `DISABLED_`/`GTEST_SKIP`; no test file ever deleted in Git history; `core_tests.cpp` unchanged since baseline `1f67b16` |
| CLI smoke (Release) | `--version` exit 0, `--help` exit 0, unknown command exit 2 — PASS |
| Benchmarks 100 / 1,000 / 10,000 elements | Run; normalized residual ≤ 3.5e-18; near-linear scaling of all phases (§4) |
| Sanitizers / static analysis | **NOT RUN** — outstanding R5 item |

## 2. Test inventory

| File | Suite | Tests | Milestone |
|---|---|---:|---|
| `tests/unit/core_tests.cpp` | `ElementRecovery` | 2 | preserved pre-requalification tests |
| `tests/unit/beam3d_requalification_tests.cpp` | `Beam3DRequalification` | 5 | R1 |
| `tests/unit/assembly_requalification_tests.cpp` | `AssemblyRequalification` | 5 | R2 |
| `tests/unit/linear_static_solver_requalification_tests.cpp` | `LinearStaticRequalification` | 10 | R3 |
| `tests/unit/recovery_requalification_tests.cpp` | `RecoveryRequalification` | 4 | R4 |
| **Total** | | **26** | |

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

### Performance (R5) — see §4
- O(elements × nodes) lookups in assembly and recovery **found and fixed**
  (`734b2fc`); numerical output unchanged bit-for-bit.

## 4. Benchmarks (Release, CI runner, median of 5 after warm-up)

Model: straight cantilever chain of `n` Beam3D elements, node 1 fixed, unit
load at the tip. Times in milliseconds.

| Elements | DOFs | Assembly | Kff reduction | Symbolic | Numeric | Solve | Recovery | Normalized residual |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | 606 | 0.205 | 0.085 | 0.121 | 0.017 | 0.009 | 0.275 | 3.5e-18 |
| 1,000 | 6,006 | 1.974 | 0.909 | 1.219 | 0.219 | 0.081 | 2.780 | 1.1e-18 |
| 10,000 | 60,006 | 19.44 | 6.19 | 11.26 | 2.48 | 0.833 | 31.41 | 3.1e-19 |

Before the lookup fix (commit `1a42c61`), the 10,000-element row was
assembly 99.30 ms and recovery 253.1 ms, growing ~32× and ~48× for 10× more
elements. After the fix, every phase grows ~6–11× for 10× more elements.
Residuals are identical before and after (abs 0.0467754, normalized
3.14632e-19 at 10,000 elements).

Limits of this evidence: runner timings, not the developer workstation; a
single banded chain topology, which has no fill-in and is a best case for
sparse LDLT; sizes up to 10,000 elements only.

## 5. Not verified / not implemented

| Item | Status |
|---|---|
| `StaticSolution::residual_norm`, `relative_residual`, `fixed` | Declared, **never populated** by `solve()` |
| `AnalysisResults::element_equilibrium_error`, `element_moment_equilibrium_error` | Declared, **never populated** (per-element errors and the max are populated) |
| `SolverStatusCode::NonFiniteSolution`, `NumericalFailure` | Declared, **never produced**; solution finiteness is not checked |
| Analytical closed-form check of the UZ–RY (Iy) bending plane under tip load | Covered only via stiffness coefficients (R1) and the dense portal reference (R3), not by a dedicated analytical cantilever |
| Recovery re-assembles `K` instead of reusing the solver's matrix | Correct but ~2× assembly work; avoiding it needs an API change |
| Benchmarks on non-banded (realistic bridge) meshes and > 10,000 elements | Not run |
| AddressSanitizer / static analysis | Not run |
| Distributed loads, self-weight, combinations, releases, springs, other element types, modal/buckling/nonlinear analysis | Not implemented (Phase 4+) |
| CLI model input / solve path | Not implemented; CLI only prints version/help |
| Local builds on the developer workstation | Blocked by ESET `@Trojan.ML/Augur` false positive; IT exclusion requested |

## 6. Defects found and fixed during requalification

| Commit | Defect |
|---|---|
| `07f7344` | `DofManager` accepted duplicate node IDs (check ran before the index was populated) |
| `643c829` | Strain-energy fields declared but never computed |
| `734b2fc` | Quadratic node/property lookups in assembly and recovery |

Test-harness defects corrected in R2 (wrong reference `G`, uninitialized
dynamic matrix, wrong DOF map, uninvoked lambda in `EXPECT_THROW`) are
recorded in `docs/REQUALIFICATION_STATUS.md`.
