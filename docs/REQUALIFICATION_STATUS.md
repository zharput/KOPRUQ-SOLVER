# Beam3D Requalification Status

The historical 25-test suite was lost before this requalification and could
not be recovered from Git, VS Code Local History, File History, OneDrive, or
available workspace copies. This document records a new independent Beam3D
baseline; it is not a claim that the historical suite was restored.

The dedicated suite independently checks Euler–Bernoulli axial, torsional and
biaxial bending coefficients, signs, symmetry, finite values, six rigid-body
modes and six positive deformational modes. It also checks independently
constructed horizontal, vertical and inclined axes, an independent inclined
transformation, strain-energy invariance, reversed connectivity and invalid
geometry/property rejection.

Tolerances are relative to the largest matrix/eigenvalue scale where
appropriate; zero-mode checks use `1e-10 * max(|lambda|)`. Absolute checks are
used only for dimensionally explicit analytical coefficients.

The current test inventory consists of the two preserved ElementRecovery tests
and the dedicated Beam3D requalification tests. The solver recovery,
assembly, boundary and performance subsystems remain unverified against the
lost historical inventory and require a separate regression requalification.

## R2: DOF and sparse assembly — PASS

R2 adds an independent assembly suite covering sorted nonconsecutive node IDs,
DOF ordering, single-element dense comparison, an independent 18x18
two-element reference with shared-node accumulation, restraint classification,
component detection, orientation cases and invalid assembly inputs. Expected
matrices are assembled from test-local Euler–Bernoulli coefficients and
explicit DOF maps, not from `MatrixAssembler`. Matrix comparison uses a
relative Frobenius tolerance of `1e-11` and coefficient checks use a
scale-aware tolerance.

Build blocker resolution: the build tree under `build/debug` and `build/release`
already had compiler detection cached from prior configure runs. Reconfiguring
with `-DVCPKG_MANIFEST_INSTALL=OFF` (packages were already present in
`vcpkg_installed`) avoided re-running vcpkg's compiler-detection step, which is
where the `CMakeCXXCompilerId.exe` permission denial was occurring. The
underlying permission issue (consistent with Defender/Smart App Control
interference) was not root-caused and will resurface on a clean build tree.

Four defects in the independent test harness itself were found and corrected
during verification, none of which involved `MatrixAssembler` or `DofManager`:
- the local 12x12 reference used an uninitialized dynamic-size `Eigen::MatrixXd`
  instead of a zero-initialized `12x12`, tripping an Eigen assert;
- the reference used a fixed `G = 80e6` instead of the model's
  `G = E / (2(1+nu)) approx 80.77e6`, so torsional terms did not match;
- the independent 18x18 two-element DOF map assumed insertion order instead of
  the sorted node-ID order `DofManager` actually produces (shared node 10 maps
  to DOFs 0-5, not 6-11);
- an `EXPECT_THROW` wrapped a lambda but never invoked it, so the assertion
  never exercised the code under test.

One production defect was found and fixed in
`include/kopruq/assembly/DofManager.hpp`: the duplicate-node-id check tested
membership in `offsets_`, which is populated only after the ID-collection loop
completes, so duplicate node IDs were silently accepted instead of raising
`AssemblyError`. The check now uses a `std::set<Id>` populated during the same
pass that collects IDs.

Verification performed after both fixes: full CTest in `build/debug` and
`build/release` (`ninja-debug` / `ninja-release` presets), 12/12 passing in
both configurations, including the two preserved `ElementRecovery` tests and
the Beam3D requalification suite. CLI smoke check (`kopruq_solver_cli
--version`, `--help`, and an unknown command) confirmed expected exit codes;
the CLI does not yet expose model loading, so this is not a solve-path
validation.
