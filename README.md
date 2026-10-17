# KOPRUQ-SOLVER

Standalone C++20 3D frame finite-element solver for bridge engineering, with
independently verified numerical results. Units: m, kN, kNm, rad; six DOFs
per node.

## Capabilities

- 3D Euler–Bernoulli beam element (axial, torsion, biaxial bending), sparse
  global assembly with deterministic DOF numbering.
- Linear static analysis: nodal loads; uniform and trapezoidal member loads
  over full or partial length in local or global axes (global per true or
  projected length); self-weight; multiple load cases from one
  factorization; load combinations by superposition.
- Results: displacements, support reactions, element local/global end
  forces, element and global equilibrium checks, strain energy, residuals.
- Beam end releases (hinges) by static condensation. Modal (free-vibration)
  analysis and single-direction response spectrum analysis (SRSS
  combination; the spectrum shape is caller-supplied, not computed).
- JSON model input and results output with path-qualified validation errors
  and reproducible metadata (`schemas/`, `examples/`).

What is and is not verified: `docs/REQUALIFICATION_REPORT.md`. Current status
and roadmap: `docs/DEVELOPMENT_STATUS.md`, `docs/SOLVER_ROADMAP.md`.

## Command line

```
kopruq_solver_cli solve examples/portal_frame.json -o results.json
kopruq_solver_cli modal examples/portal_frame.json -n 6
kopruq_solver_cli spectrum examples/portal_frame.json
kopruq_solver_cli --version
```

Exit codes: 0 success, 1 analysis failed (results still written), 2 usage
error, 3 invalid model, 4 file I/O error.

## Build

The project uses the Visual Studio Build Tools bundled CMake, Ninja, MSVC x64 environment, and standalone vcpkg at `C:\_ZHarput_Data\Tools\vcpkg`. From a normal PowerShell terminal, run:

```powershell
./scripts/configure.ps1
./scripts/build.ps1
./scripts/test.ps1
```

The scripts set `VCPKG_ROOT` for the current process only and do not modify the global PATH. Release builds use the `ninja-release` configure and build presets; `ninja-asan` and `ninja-analyze` add AddressSanitizer and MSVC static analysis. The vcpkg registry is pinned by the `builtin-baseline` in `vcpkg.json`.

The installed Visual Studio vcpkg may require PowerShell Core during compiler detection. Install it separately only if vcpkg reports that requirement; this project does not install tools automatically.

## Continuous integration

`.github/workflows/ci.yml` builds and runs the full test suite on a GitHub-hosted Windows runner in Debug, Release and AddressSanitizer configurations, runs MSVC `/analyze`, a CLI smoke test and the benchmarks. While the development workstation's endpoint protection quarantines freshly built executables (see `docs/DEVELOPMENT_STATUS.md`), CI is the verification source of truth.
