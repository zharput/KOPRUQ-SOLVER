# Element station results (diagrams): technical design -- DESIGN ONLY, not implemented

**Problem.** `results.json` has only the two end forces per element, but the diagrams need N, Vy, Vz, T,
My, Mz along each member. Moments between the ends depend on the member loads, which the results file
does not contain.

**Proposal (additive, new output).** A new command `stations` (or `--stations n` on a new schema
`kopruq-element-stations` v1; never a change to `kopruq-results`) writing, per load case/combination and
element, the six internal forces at `n` stations x_i = i L/(n-1) plus extra stations at point loads
and load-segment ends (where shear jumps / moment kinks).

**Method (exact, no new physics).** For a straight member in equilibrium the internal forces follow from
free-body statics of the part between node 1 and the cut: S(x) = -f1(local end force) - integral_0^x q dx - sum P_k
(k with x_k <= x), M(x) = -m1 + F x (cross product) - integral moments of q and P, moment loads
included, all in member-local axes. Inputs are quantities the solver already computes: the recovered local end
force (`k u - f_eq`, `ElementResults::local_end_force`) and the element's own load definition (`MemberLoad`,
`MemberPointLoad`, self-weight). Releases need nothing special (the released end force is zero);
thermal and temperature-gradient loads act only through the equivalent nodal loads (no internal
distributed term). Timoshenko vs Euler-Bernoulli does not matter: equilibrium is independent of kinematics.
Displacement along the member for the deformed shape (display only): Hermite (or Timoshenko
interdependent) interpolation of the end displacements, the same shape functions as the load vector.

**Conventions.** Output in the solver's own local convention with an explicit `convention` field; the MIDAS
sign table (`kBeamForceFlipsAtIEnd`) is applied only in verification, never in the station data.

**Verification plan (independent references).** (1) Closed forms: simply supported / cantilever / fixed-fixed
beam with uniform, trapezoidal and point loads, M(x) = q x (L - x)/2 etc. (2) x = 0 and x = L stations
reproduce the end forces. (3) Global check: the stations of a whole model reproduce the support reactions
by cutting. (4) MIDAS beam-force export parts (I/J) on models 05, 06, 100.

**Decisions needed.** Station count default (11?); whether combinations are produced by superposing case
stations (exact, linear) -- proposed yes; output size (100 000 stations x 6 x cases: stream, don't hold in memory).
