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
