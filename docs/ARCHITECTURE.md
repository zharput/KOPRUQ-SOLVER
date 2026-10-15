# Architecture

The library is layered as core domain models, finite elements, DOF management, assembly, solvers, result recovery, and public API. The CLI only adapts command-line input to the library.
# Phase 2

Global DOFs use ascending node IDs with six consecutive UX, UY, UZ, RX, RY, RZ entries. MatrixAssembler accumulates Beam3D element matrices with Eigen triplets and returns compressed sparse storage. Boundary and component diagnostics do not solve or constrain the system.

# Phase 1

Beam3D is a header-only, fixed-size Eigen element API independent of assembly, CLI and JSON. Geometry, axes, local stiffness, transformation, and validation are explicit operations.

# Phase 3.1

LinearStaticSolver assembles nodal loads, traverses compressed sparse K to form Kff, and solves Kff uf = Ff with Eigen SimplicialLDLT. Residuals use the Frobenius-scaled norm. Reactions and element forces are deferred to Phase 3.2.
