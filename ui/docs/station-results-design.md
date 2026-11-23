# Element station results (diagrams): technical design -- IMPLEMENTED 2026-11-23 (CLI `stations`, kopruq-element-stations v1)

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

## Implementation notes (2026-11-23)
- `include/kopruq/results/ElementStations.hpp` (method), `io/ElementStationsJson.hpp` (document), CLI `stations <model> [-o] [--stations n] [--format]`,
  schema `schemas/element_stations.schema.json`. Default 11 uniform stations plus load-segment ends; a point load inside a member gives a before/after pair.
- Decisions taken: default 11 stations; combinations use the combined load case and recovered end forces of the combination itself (exactly linear);
  response-spectrum peaks and truss/spring elements are not produced; output is built in memory (model 100: 7 cases, 94 members).
- Each element carries `closure_force` / `closure_moment` = |R(L; all loads) - f2|, |M(L; all loads) - m2|: equilibrium of the solver's recovered end forces with
  the member loads (independent of the station integration). Model 100: max 9.1e-13 kN / 1.4e-9 kNm.
- Not done: MIDAS beam-force export comparison (the sign flip is a verification-layer concern; no station data is converted).
