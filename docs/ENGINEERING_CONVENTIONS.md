# Engineering conventions

Nodes use the ordered degrees of freedom UX, UY, UZ, RX, RY, RZ. Canonical units are m, kN, kNm, kN/m², kN·s²/m, and rad. Coordinates are global right-handed Cartesian; elements will use deterministic right-handed local axes. Values are double precision. Global systems will be sparse, with stable node-then-DOF numbering. Positive translations follow global axes and positive rotations follow the right-hand rule. Matrix rows and columns use the stated DOF order.
# Phase 2 conventions

Global DOFs are assigned by ascending node ID, six consecutive entries per node in UX, UY, UZ, RX, RY, RZ order. Infinitesimal rigid rotation uses u = omega cross (x - x_ref) and theta = omega. Reversed connectivity requires an explicitly transformed reference orientation; an unchanged reference vector is not assumed physically equivalent.

# Phase 1 Beam3D

Beam3D uses x from node 1 to node 2. The supplied reference vector is projected perpendicular to x and normalized as local y; local z = x cross y. A parallel reference is rejected. Local displacement equals T times global displacement and Kglobal = T-transpose Klocal T. Euler-Bernoulli bending uses EIz in the local y/rz block and EIy in the local z/ry block. Shear deformation, warping, offsets and releases are not included.

Eigen expression-template objects must not retain references to temporary matrices. Materialize temporary operands explicitly, for example `Mat kl = local(...); Mat ke = T.transpose()*kl*T;`.
