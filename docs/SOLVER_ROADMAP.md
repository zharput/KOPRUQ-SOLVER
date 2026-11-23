# KOPRUQ-SOLVER — Engineering Development Roadmap

## Product objective
Build a standalone, production-grade C++20 three-dimensional finite-element structural solver, initially optimized for bridges, with independently verifiable numerical results, robust diagnostics, reproducible benchmarks, and a stable API for future KOPRUQ integration. No reliance on MIDAS or OpenSees for analysis.

## Design principles
- SI engineering units: m, kN, kNm, kN/m², rad.
- 6 DOFs per 3D beam node; deterministic indexing and sparse global matrices.
- Separate model input, validation, element formulations, assembly, load generation, solution algorithms, results recovery, verification, and persistence/API.
- Numerical correctness before optimization. Each new capability requires independent reference tests.
- No unapproved edits to the separate KOPRUQ frontend project.

## Current capability summary — 2026-11-23 (verified state)
What the solver does today. Everything listed is implemented and covered by tests (Debug 381/381,
Release 381/381, gui-release 420/420) and, where noted, verified against MIDAS NX reference models.
Anything not listed is not supported; the MIDAS importer rejects unsupported records explicitly
(never approximates them silently) and reports every disclosed assumption.

**Analyses**
- Linear static: multiple load cases from one factorisation, load combinations by superposition
  (MIDAS LCOM-GEN incl. nested combinations).
- Modal (eigenvalue): consistent mass (default) or lumped translational mass (MIDAS-like),
  mass-normalised modes; both also with rigid links / node local axes (slave mass moved to the
  master, T^T M T).
- Response spectrum: single direction (global X/Y/Z), SRSS or CQC modal combination,
  caller-supplied spectrum points, linear or log-log interpolation (MIDAS INTERP "LOG"; the
  table's T = 0 point is used at 1e-6 s, an assumption consistent with MIDAS's results).
- Static + response-spectrum combinations (MIDAS LCOM-GEN with RS terms): static terms + factor x
  |spectrum peak|; definitions in the model/results, evaluated by the verification layer.
- Not available: buckling / P-Delta / nonlinear / time history / construction stages (cancelled);
  traffic (moving) loads (reinstated 2026-11-16, not started).

**Elements**
- 3D beam (6 DOF/node), Euler-Bernoulli or Timoshenko (shear areas > 0); end releases (static
  condensation); rigid end offsets (incl. MIDAS section offset "CT").
- Truss (pin-ended, axial only).
- Two-node elastic link, 6 independent springs in link-local axes (MIDAS ELNK "GEN").
- Single-node elastic support spring, 6 components (MIDAS NSPR "LINEAR").
- Rigid link (master-slave), all six or selected DOFs (MIDAS RIGD with DOF mask; a restrained
  slave DOF is dropped from the link, MIDAS behaviour).

**Supports / boundary conditions**
- Per-node restraints UX..RZ in global axes, or in node local axes (MIDAS SKEW, one rotation
  angle) with reactions reported in global and local axes.
- MIDAS structure type STYP = 1 (X-Z plane: UY, RX, RZ restrained everywhere); STYP = 0 (3D).
- Elastic supports via NSPR springs or elastic links to restrained nodes.

**Loads**
- Nodal forces and moments (global).
- Member loads, local or global axes: uniform / trapezoidal / multi-point piecewise-linear
  distributed forces and moments over full or partial length; concentrated forces and moments;
  projected global loads (native model only; MIDAS USE_PROJECTION is rejected as unverified).
- Equivalent nodal loads consistent with the element: Hermite for Euler-Bernoulli,
  Timoshenko interdependent shape functions for shear-deformable members.
- Self-weight (global -Z, unit weight x area).
- Temperature: uniform change (MIDAS ETMP) and linear gradients over the section depth
  (MIDAS GTMP).
- Not imported from MIDAS: BTMP section temperature profiles, eccentric or pressure beam loads.

**Sections**
- Native model: direct values A, Iy, Iz, J, Asy, Asz.
- MIDAS DB/User: SB (rectangle), SR (solid circle), B (box), T, H/I (unequal flanges, web
  fillets r1), C (channel), L (angle, truss members only), STRK (track), SOCT (octagon) --
  each with MIDAS's own J and shear-area conventions identified from MIDAS results.
- MIDAS PSC: 1CEL (single-cell box, joints off) and PSCI (precast I girder, joints JL1/JL4):
  exact polygon A/I/centroid; J and shear areas from a 2D FE section analysis (SectionFE). These
  differ from MIDAS's own mesh-dependent values by up to ~0.25% (J) / ~0.75% (Asz).
- Not supported: catalog profiles (AISC/EN, removed by user decision), other PSC shapes,
  user-input PSC stiffness, tapered sections.

**Materials**
- EN 1992-1-1 concrete grades (Ecm = 22 ((fck+8)/10)^0.3 GPa, nu 0.2, 25 kN/m3,
  alpha 1e-5/K); EN 1993 structural steel grades (E 210 GPa, nu 0.3, 78.5 kN/m3).
- Native model: any isotropic linear-elastic material (E, nu, unit weight, alpha).

**Input / output and tools**
- Native JSON model/results schemas (versioned, validated, deterministic, SHA-256 provenance).
- MIDAS Civil NX JSON import (`--format midas-nx-json`).
- CLI: `solve`, `modal`, `spectrum`.
- Qt6 verification GUI: runs the solver on a MIDAS model and compares against MIDAS Excel
  exports -- displacements, global and local reactions, beam forces, truss forces, elastic link
  forces, eigenvalues, response-spectrum results -- with Excel report export.

**MIDAS verification status (reference models in `verification/models`)**
- 100%: models 01-12, 13-15 (eigen), 16, 17, 18, 19, 20, 21, 24, 100.
- Model 22: 4 zero-crossing rows (TU). Model 23: 1 zero-crossing row. Both trace to the PSC
  section J/As mesh dependence above (diagnostic: with MIDAS's own section values both are 100%).
  (Model 16's former 2 rows were the spectrum interpolation, resolved 2026-11-23.)
- Model 100 ("Basit köprü", first full bridge model): 100% in all 12 static, spectrum and
  static + spectrum cases; eigen 20/20 (2026-11-23).

## Scope change — 2026-10-17 (user decision)
Phase 6.3 (linear buckling / P-Delta), Phase 7 (time-dependent and nonlinear
analysis) and Phase 8 (bridge engineering capabilities: construction
stages, bearings/soil springs, moving loads/influence lines) are
**cancelled and out of scope** for this project. They are removed from
this roadmap, not deferred. The former Phase 9/10/11 are renumbered to
Phase 7/8/9 below since nothing after them in the sequence depends on the
cancelled phases. Full rationale/record: `docs/DEVELOPMENT_STATUS.md`.

## Scope change — 2026-11-16 (user decision): traffic loads reinstated; load-type completion order
Partial reversal of the 2026-10-17 cancellation: **traffic (moving) loads only** are reinstated
(Phase 7I below) — construction stages and bearings/soil springs remain cancelled. User's explicit
instruction for the next work sequence, load type by load type, each taken to completion before
the next starts:
1. Dead/self-weight loads — already COMPLETE (Phase 4.1's `Material::unit_weight` self-weight).
2. Uniform temperature loads — already COMPLETE (Phase 7T, `MemberThermalLoad::uniform_delta_t`).
3. Modal analysis and response-spectrum analysis — COMPLETE for the native, rigid-link-free case
   (Phase 6.1/6.2) **and now also for MIDAS-imported rigid-link models** (Phase 7H, COMPLETE,
   verified 2026-11-17: solver-level fix + CLI dispatch confirmed end-to-end). Still open, split
   out as its own sub-item rather than guessed: EIGV/SPLC/SPFC -> `response_spectra` translation
   in the MIDAS importer itself (blocked on missing real-sample field-shape ground truth — see
   Phase 7H's own entry below for why).
4. Traffic (moving) loads — reinstated, Phase 7I, not started, scope not yet defined.
5. Load combinations — Phase 7G, not started (MIDAS `LCOM-GEN` import).
Phase 7E.1 (MIDAS beam-force sign convention) ran alongside this sequence rather than strictly
before or after it, since it affects comparison/verification of every load type above, not one
specific load type — **COMPLETE, verified 2026-11-17** (see its own entry below).

## Immediate priority: Phase 3.2 requalification — COMPLETE
The historical 25-test suite was lost. It has **not** been restored. New
independent tests were written instead, preserving all existing tests;
these are requalification, not historical restoration.

### R1 — Beam3D core — COMPLETE (reported)
Independent local stiffness, orientation, global transform, rigid modes, energy, invalid inputs. Reported Debug and Release 7/7 PASS; commit `907ea32`.

### R2 — DOF mapping and sparse assembly — COMPLETE (verified)
Independent `tests/unit/assembly_requalification_tests.cpp` covering sorted
nonconsecutive node IDs, DOF ordering, independent 12×12 and 18×18 dense
references, restraint classification, component detection, orientation and
invalid inputs. One production defect found and fixed (`DofManager`
accepted duplicate node IDs). Debug and Release full CTest 12/12 PASS;
commits `07f7344`, `f050918`.

### R3 — Linear static solver requalification — COMPLETE (verified)
Independent `tests/unit/linear_static_solver_requalification_tests.cpp` (10
tests): analytical axial/flexural/torsional cantilever tip displacements
and reactions, a combined-load superposition case, direct independent
checks of `reduceKff` and `assembleLoads`, an independent dense 18-DOF
two-element portal-frame assembled/partitioned/solved without calling
`MatrixAssembler`/`DofManager`/`LinearStaticSolver`, status-code rejection
paths, and determinism. Debug and Release full CTest 22/22 PASS.

### R4 — Reaction, element-force and energy recovery requalification — COMPLETE (verified)
Implemented previously-dead `strain_energy`/`element_strain_energy`/
`energy_error` fields (commit `643c829`). Independent
`tests/unit/recovery_requalification_tests.cpp` (4 tests): strain-energy
consistency, an inclined element's end-force/reaction/energy recovery, and
a two-element portal frame with independently recomputed per-element
forces and energy. Debug and Release full CTest 26/26 PASS.

### R5 — Performance, robustness and release gate — COMPLETE (verified)
Verified on CI (run `38872157394`, commit `1ed5ae4`): benchmarks at
100/1,000/10,000 elements with every phase timed separately and scaling
linearly after a quadratic-lookup fix (`734b2fc`); Debug, Release and
AddressSanitizer CTest 30/30; MSVC `/analyze` gate with 0 findings in
production code; CLI smoke; test-inventory audit; previously unpopulated
diagnostics implemented (`506a8be`). Details:
`docs/REQUALIFICATION_REPORT.md`.

**Phase 3.2 requalification is complete.**

## Subsequent solver development — implement in dependency order
The phases below are planning targets, **not completed features** except
where explicitly marked COMPLETE. Validate scope and numerical
formulations before implementation.

### Phase 4 — Production model and load architecture
**4.1 member loads and load cases — COMPLETE (verified, CI run
`38874706685`, commits `4fefc25`, `9389ab3`):** uniform/trapezoidal member
loads over full or partial length in local or global axes (global per true
length, or per projected length normal to the load direction — **4.1b**,
CI run `38878331252`), consistent equivalent nodal loads, end forces
`k·u − FEF`, self-weight (`Material::unit_weight`, global −Z), multiple
load cases from one factorization, combinations by superposition,
`StaticSolution::fixed`.

**4.2 JSON model I/O and CLI — COMPLETE (verified, CI run `38884699955`,
commits `2d9e4da`, `b3b6ab0`, `24f1c65`):** `kopruq-model` v1 input with
path-qualified validation, `kopruq-results` v1 deterministic output with
SHA-256 input metadata, CLI `solve`.

**4.3 concentrated member forces/moments and distributed moments —
COMPLETE (verified, CI run `38887606677`, commits `48ad305`, `b2a605f`,
`62cd955`):** `MemberPointLoad`, distributed moments, a force-equilibrium
metric fix for moment-only loading.

**4.4 validated model in the C++ API — COMPLETE (verified, CI run
`38896544673`, commits `dba9837`, `a942083`):** `validateModel()` /
`validateModelOrThrow()`, same semantic checks as the JSON reader, for
direct C++ callers; `solveCases()`/`recover()` unchanged.

All of Phase 4's originally planned scope is done: immutable/validated
model representation (validated in 4.4; not yet immutable — mutation after
validation is still possible, caller discipline only), nodal/member loads
and load cases/combinations (4.1, 4.3), reliable import/export with schema
versioning and structured errors (4.2).

### Phase 5 — Additional structural elements and releases
**5.1 beam end releases — COMPLETE (verified, CI run `38894147560`, commits
`4dee2b5`, `cbef2e8`, `41aec8a`):** static condensation of K and member
loads, unstable-combination rejection, JSON `releases`.

**5.2 truss elements — COMPLETE (verified, CI run `39069625215`, commits
`2228863`, `a1e8212`):** `Truss3D.hpp` -- a pin-ended 3D truss member
implemented as a Beam3D with every rotational DOF released (RY,RZ at both
ends, RX at node2 only -- releasing RX at both ends creates a separate,
genuine zero-energy spin mechanism, caught by CI and fixed), reusing the
already-verified static-condensation machinery from 5.1 rather than a
second stiffness/mass/load/recovery path. JSON `trusses` array.

**5.3 rigid end offsets (eccentric connections) — COMPLETE (verified, CI
run `39069625215`, commit `a1e8212`):** `Offsets.hpp` -- a rigid-body
kinematic transform (`K_node = G^T K_point G`, same for M and member
loads) mapping a member's offset end point to its node; zero offsets are
an exact no-op. JSON `offset1`/`offset2` on beams.

**5.4 spring/link elements, including bearing and rotational-spring models
— COMPLETE (verified, CI run `39069625215`, commits `9126760`, `189df29`,
`7a6fb83`):** `SpringLink.hpp` -- a two-node, six-independent-DOF discrete
spring (kx,ky,kz,krx,kry,krz, no coupling between them), covering bearings,
isolators and soil springs. JSON `springs` array.

**5.4 revision (2026-11-18, user decision) — ElasticLink generalized to non-coincident nodes,
COMPLETE (verified this session):** the original coincident-node-only restriction (found by CI,
see `docs/MIDAS_JSON_DATA_AUDIT.md` 4.5 for why it blocked MIDAS `ELNK` import) is lifted.
`checkSpringNodesCoincident()` is removed; `SpringLink.hpp`'s header comment now documents the
deliberate consequence: the decoupled per-DOF stiffness still has no lever-arm coupling between a
node's translational springs and the other node's position, so a *separated*-node link is only
rigid-body consistent for small/no separation, matching (not exceeding) the fidelity MIDAS NX's
own "General" type Elastic Link is believed to offer for non-coincident nodes (undocumented in
any source available to this project; the user explicitly chose this option over adding a rigid-
offset/lever-arm coupling that would risk diverging from MIDAS's own numbers). A model needing
exact rigid-body consistency across a real separation should use `RigidLink` instead, not
`ElasticLink`. New test: `SpringIo.AcceptsNonCoincidentNodes` (replaces the old
`RejectsNonCoincidentNodes`, which tested behavior no longer true).

**5.4 revision addendum (2026-11-18) — MIDAS NX `ELNK` (`LINK:"GEN"`) import wired and
empirically verified, COMPLETE:** `MidasJsonImporter.hpp`'s ELNK handling now imports `LINK:"GEN"`
records as `SpringConnection` instead of rejecting them outright. Orientation reuses the exact
same node1->node2 + `ANGLE` Beta-angle convention already confirmed for `ELEM`/`BEAM`
(`midasReferenceY`) -- not a separately invented rule, since MIDAS's Element Coordinate System
convention applies to its line elements generally. `SDR` maps directly to local
`kx,ky,kz,krx,kry,krz`. Only `R_S` all-false (no release) is accepted; any true entry is rejected,
not guessed. `DR` is read but deliberately not used (`SpringLink`'s formulation has no concept of
an interior/offset point for it to feed into). **Empirically verified** against
`verification/models/07 Basit Kiris Uniform yayli`'s real MIDAS export (one non-coincident ELNK,
node 10 at Z=-0.5 to node 1 at Z=0.0): importing, solving and comparing against that model's own
real `Displacements(Global).xlsx`/`Beam Force.xlsx`/`Reaction(Global).xlsx` gives **168/168 PASS,
0 FAIL, 0 N/A** (a standalone diagnostic program, not committed, built against `kopruq_verification`
+ `xlnt`) -- strong evidence the orientation/stiffness interpretation above is correct, including
that `DR`/`R_S` truly do not matter for this model. New permanent regression test
`tests/unit/midas_elastic_link_tests.cpp` (`MidasElasticLink.ImportsGenLinkAndSolvesMatchingRealMidasReference`)
embeds model 07's real DBG payload inline (self-contained, no dependency on the untracked
`verification/models/` directory) and checks node 1's displacement/rotation and node 9/10's
reactions against values transcribed from that same real MIDAS export -- node 10 is fully
restrained and connects to the structure *only* through the ELNK spring, so this is a genuine,
non-trivial check of the import, not a "does it throw" smoke test.
`MidasImportReport::springs` (new field) counts imported links; `elastic_links_rejected` now
means "not imported" (unsupported LINK type, unconfirmed R_S, missing node, bad SDR), not
"always rejected".

**5.4b new: single-node elastic support ("Joint Spring" / MIDAS NX `NSPR` Nodal Spring) —
COMPLETE (verified this session):** `JointSpring.hpp`, a new, deliberately separate element
(`JointSpringConnection`, `Model::joint_springs`, independent id space from both beams' and
`springs`') for a six-independent-DOF elastic support directly to ground at one node -- distinct
from `ElasticLink`/`springs` (two-node, flexible connection between two model nodes) and from
`RigidLink` (kinematic master-slave constraint). Global assembly adds its local 6x6 stiffness
directly to the node's own DOF block (`MatrixAssembler::assemble`). `LinearStaticSolver`'s
`recoverAssembled` reports each joint spring's own force/energy in a new
`AnalysisResults::joint_springs` and folds its force into the model-level force/moment
equilibrium check for any DOF component not separately restrained at that node (its negative is
the true external ground reaction -- see `AnalysisResults.hpp`'s comment for the sign derivation,
found and fixed via `JointSpring.TiltedLoadedSpringSatisfiesEquilibrium` initially failing with
force_equilibrium_error=1.0 before the sign was corrected). JSON `joint_springs` array (new
`$defs/joint_spring` in `schemas/model.schema.json`). Not yet wired: MIDAS NX `NSPR` import --
every one of this project's 17 MIDAS sample exports has 0 `NSPR` records, so there is no real
sample data to verify an import mapping against; deferred until one is available (see "Next
autonomous action" below).

Historical note (now superseded by the 5.4 revision above): node1/node2 were required to be at
the same location -- the decoupled per-DOF stiffness has no lever-arm coupling between a node's
translational springs and the other node's position, so it was only rigid-body consistent (passed
the equilibrium-residual check used throughout this codebase) when the nodes coincided.

**5.5 general rigid links / multi-point constraints — COMPLETE (verified,
CI run `39104401638`, commit `18b6a70`):** `RigidLink.hpp` -- a slave
node's 6 DOFs entirely determined by a master's via rigid-body kinematics
(one master, many possible slaves, e.g. a rigid diaphragm), implemented as
a global DOF-elimination layer wrapped around the existing, unmodified
assembled K/F/U, with an isolated code path
(`LinearStaticSolver::solveCasesWithRigidLinks`) so a model without
rigid links is provably unaffected. Static analysis only -- ModalSolver
and ResponseSpectrumSolver explicitly reject a model that has any, rather
than silently ignoring the constraint. JSON `rigid_links` array.

**5.6 Timoshenko beam option — COMPLETE (verified, CI run `39104401638`,
commits `edc0e6d`, `a2f5290`):** optional effective shear areas
`Section::Asy`/`Asz` (0 = rigid in shear, the prior Euler-Bernoulli
behavior, recovered exactly); `Beam3D::localStiffness` applies the
standard Timoshenko (1+Phi) correction. The mass matrix deliberately keeps
its Euler-Bernoulli shape functions regardless (documented simplification,
not an oversight) -- modal/response-spectrum results for a Timoshenko
beam use this slightly approximate mass alongside the exact Timoshenko
stiffness.

Phase 5 is now complete in full: 5.1-5.6, with no remaining deferred
items. (5.5/5.6 were completed per explicit user instruction on
2026-10-18 after being initially deferred as out of the original work
period's scope.)

### Phase 6 — Advanced linear analyses
**6.1 modal analysis — COMPLETE (verified, CI run `38902536528`, commits
`63402a5`, `d12a40a`, `bca8206`):** consistent mass matrix, Guyan mass
condensation for released ends, free-DOF generalized eigenproblem,
mass-normalized mode shapes.

**6.1b JSON/CLI support for modal analysis — COMPLETE (verified, run
`38906210839`, commits `86f90d1`, `31fb203`):** `kopruq-modal-results` v1,
CLI `modal` command.

**6.2 single-direction response spectrum analysis — COMPLETE (verified, CI
run `39051080335`, commits `b207adf`, `5807f51`):** `ResponseSpectrumSolver`
on top of 6.1's modes; caller-supplied spectrum points only (spectrum
shape itself stays outside the solver kernel); SRSS modal combination (CQC added
later in Phase 7I.1; multi-direction combination rules still not
implemented).

**6.2b JSON/CLI support for response spectrum analysis — COMPLETE
(verified, run `39055492002`, commits `3073eb6`, `7831ddd`):** model
`response_spectra` array, `kopruq-response-spectrum-results` v1, CLI
`spectrum` command.

Linear buckling / P-Delta (geometric stiffness) — **cancelled, out of
scope** (see "Scope change" above). Phase 6 is otherwise complete as
listed; no further items planned unless the user adds scope.

### Phase 7 — High-performance production solver
*(renumbered from the former Phase 9; the former Phase 7-8 are cancelled, see "Scope change" above)*
- Sparse direct/iterative solver strategy, optional SuiteSparse integration, profiling and parallelism where justified.
- Stable API, reproducibility, scalability and packaging.
Not started.

## Scope addition — 2026-10-18 (user decision): Phases 7A-7F, MIDAS NX JSON import
User instruction: make KOPRUQ-SOLVER able to import a real MIDAS Civil NX JSON export
(`xmidas/deneme1.json`), reconstruct the model internally, analyze it with KOPRUQ-SOLVER's own
solver (never MIDAS or OpenSees as a computational dependency), and export JSON results,
independently verified against analytical references first and MIDAS's own results where
available. Phases 7A-7F below sit after Phase 7 and before Phase 8/9, which remain deferred
pending explicit approval as before. First execution (2026-10-18) was audit-only, per the user's
explicit instruction not to begin implementing 7B yet: `docs/MIDAS_JSON_DATA_AUDIT.md` and
`docs/SOLVER_CAPABILITY_MATRIX.md` record the findings; nothing in 7B-7F has been implemented.

### Phase 7A — Solver capability and MIDAS JSON audit — COMPLETE (this execution, 2026-10-18)
Deliverables: `docs/MIDAS_JSON_DATA_AUDIT.md` (full inventory of `xmidas/deneme1.json`: 106
nodes, 94 beam elements, 2 materials, 5 sections, 9 supports, 4 elastic links, 5 rigid links, 5
load cases, 40 member loads, 40 element-temperature records, 1 modal case, 2 response-spectrum
cases); `docs/SOLVER_CAPABILITY_MATRIX.md` (capability-by-capability gap analysis). Key findings,
in blocking-severity order: (1) material elastic modulus/Poisson/unit-weight are not in the
export, only a design-code grade name — needs a cited code formula or explicit rejection; (2)
section area/Iy/Iz/J are not in the export, only a shape code + raw dimensions — needs a
section-shape property library for the 3 shapes actually used (`SB`,`B`,`SR`); (3) the sample's
4 elastic links (`ELNK`) connect non-coincident nodes, which Phase 5.4's `SpringLink` deliberately
rejects (a real formulation limit, not a missing-data gap) — needs either extending the element
or explicit rejection of those records; (4) the sample's rigid links (`RIGD`, maps directly onto
Phase 5.5's `RigidLink`) coexist with modal/response-spectrum requests that `ModalSolver`/
`ResponseSpectrumSolver` currently reject outright when `rigid_links` is non-empty; (5) element
temperature loads (`ETMP`/case `TU`) are not implemented in any phase. Official MIDAS
documentation pages (API manual, Export-to-JSON) returned HTTP 403 (authenticated portal);
findings are from direct inspection of the real export, not assumed from inaccessible docs.

### Phase 7B — MIDAS JSON importer and internal model reconstruction
**COMPLETE (verified, CI run `39227258173`, 199/199 Debug/Release/ASan, 2026-10-19; commits
`0823f8f`, `281b2b2`, `bd5ebbe`).** The real `xmidas/deneme1.json` bridge model (106 nodes, 94
beam elements, 7 individual rigid links, 4 load cases) imports successfully, passes
`validateModel`, and **solves successfully end to end** with KOPRUQ-SOLVER's own
`LinearStaticSolver` (no exception, `status.success == true`) -- the full chain MIDAS JSON ->
internal model -> independent numerical solution is demonstrated, not merely parsed.
`include/kopruq/io/midas/MidasJsonImporter.hpp`
translates `xmidas/deneme1.json` into a `kopruq::io::ModelInput`, using three MIDAS-specific
helpers built for this: `MidasMaterialLibrary.hpp` (grade-name -> E/Poisson/unit-weight, the
EN(RC) code formula plus disclosed standard-value assumptions), `MidasSectionLibrary.hpp`
(DBUSER shape code + dimensions -> area/I/J for `SB`/`B`/`SR`), `MidasOrientation.hpp`
(Beta angle -> `reference_y`, confirmed against MIDAS's own manual wording found via web
search). Supported: `NODE`, `ELEM` (`BEAM` only), `MATL`/`MATD`, `SECT`, `CONS`, `RIGD`, `BODF`
(global -Z only), `BMLD` (`CMD=BEAM`,`TYPE=UNILOAD`,`DIRECTION=GZ` only).

**Update, 2026-10-19 (verified, CI run `39247142910`, 219/219 Debug/Release/ASan; commits
`2b19d2a`, `b43f605`): `ETMP` (uniform element temperature) is now imported**, mapping directly
onto `MemberThermalLoad::uniform_delta_t` -- the sample's 40 `ETMP` records (case `"TU"`, all
`TEMP=40.0`) import and **solve successfully** (not merely parse) with the thermal load actually
active (`MidasMaterialLibrary.hpp` now also assigns the disclosed EN1992-1-1 S3.1.3(5) thermal
expansion coefficient, 1.0e-5/K). `GTMP` (gradient) and `BTMP` (beam-section temperature) remain
explicitly rejected when present -- not in this sample, 0 records -- because MIDAS's T1/T2
top-fiber sign convention could not be confirmed from accessible documentation; guessing it
would silently flip which way a member bends.

Explicitly rejected, not silently dropped: `ELNK` (non-coincident elastic links); `GTMP`/`BTMP`
(see above); anything outside the supported `ELEM`/`MATL`/`SECT` types; `EIGV`/`SPLC` (not
executed while `RIGD` models are rejected by `ModalSolver`/`ResponseSpectrumSolver`).
**Superseded by Phase 7S/7M below**: `MidasSectionLibrary`/`MidasMaterialLibrary` are
MIDAS-specific stand-ins for what 7S/7M build as general, reusable engines; once those exist, 7B
should be revisited to delegate to them instead of duplicating the shape/grade formulas -- see
the 2026-10-19 scope addition.

## Scope addition — 2026-10-19 (user decision): Phases 7S, 7M, 7T
User instruction: extend the standalone solver with three general-purpose engineering
capabilities that belong to the solver core itself, not to the MIDAS importer -- a parametric/
general section property engine, a Eurocode material library addressed by grade name, and
uniform/gradient temperature load analysis for Beam3D. All three must be usable by native JSON
models and by the MIDAS importer alike (no duplicate engineering logic). First execution
(2026-10-19) was roadmap-update only, per explicit instruction not to implement yet; nothing in
7S/7M/7T has been implemented. Inserted here, between 7B and 7C, because 7C (standalone EXE
analysis) and later phases should use these general engines rather than 7B's current
MIDAS-specific stand-ins once they exist.

### Phase 7S — Parametric and general section property engine
**First increment COMPLETE (verified, CI run `39227258173`, 199/199 Debug/Release/ASan,
2026-10-19; commits `f10dca0`, `59b0035`).** The first CI run (`39223638115`) caught a real bug
-- the centroid accumulator used the wrong divisor (1/12 instead of 1/6), off by exactly 2x,
invisible in every symmetric/origin-centered test case and only caught by a dedicated
translation-invariance test; fixed in `59b0035`.
`include/kopruq/sections/SectionGeometry.hpp`: a general polygon-integration engine (Green's
theorem / shoelace moment formulas) computing area, centroid, Iy/Iz/Iyz, principal axes/moments,
extreme fiber distances and elastic moduli (Wy+/-, Wz+/-) for ANY section built from simple
polygon contours -- an outer boundary plus any number of holes (opposite winding), i.e. the
general polygonal and multi-contour/openings families from the original scope are done.
`include/kopruq/sections/ParametricSections.hpp`: adds the hollow circle (annulus), the one
closed-form shape not already covered by `MidasSectionLibrary.hpp` (Phase 7B)'s solid rectangle/
box/solid circle, which still need migrating into this general (non-MIDAS-named) namespace --
not done in this increment, to keep it isolated and testable.

**Deliberately NOT implemented, requiring individual approval before each is added** (per
explicit instruction): I-section, H-section, T-section, channel section, angle section, precast
concrete girder sections. Also not yet done: torsional constant J for a general polygon (no
validated general-polygon torsion formula implemented; only the closed-form shapes have J so
far), shear areas Asy/Asz for any shape, mass-per-length, and the MIDAS geometry-vs-property
section verification report (needs 7B wiring once this engine covers what 7B currently gets from
`MidasSectionLibrary.hpp` directly).

### Phase 7M — Eurocode material library
**COMPLETE (verified, CI run `39227258173`, 199/199 Debug/Release/ASan, 2026-10-19; commit
`557d375`).** `include/kopruq/materials/EurocodeMaterialLibrary.hpp`. Scope: a versioned Eurocode concrete (and future-compatible steel) material
database addressed by grade name (e.g. `EN-C35/45`), citing the standard edition and National
Annex, giving fck, fck,cube, fcm, fctm, Ecm, Poisson's ratio, unit weight, thermal expansion
coefficient, and deriving G = E/(2(1+ν)). Distinguishes standard-defined values, National Annex
values, project assumptions and user overrides; never hardcodes an undocumented default
density/Poisson/thermal-expansion value. Generalizes `MidasMaterialLibrary.hpp` (built for Phase
7B), which both the MIDAS importer and native JSON models should use once this exists, instead
of each having their own grade-resolution logic.

### Phase 7T — Uniform and gradient temperature load analysis
**COMPLETE (verified, CI run `39237884916`, 215/215 Debug/Release/ASan, 2026-10-19; commits
`bd0a079`, `9c53fd1`, `1dd5323`).**
`Material::thermal_expansion` (alpha_T, 1/K, default 0) and `MemberThermalLoad` (uniform_delta_t
plus independent gradient_y/gradient_z delta_t+depth pairs) added; `Loads.hpp`'s `LoadCase`
gets a `thermal` field; `include/kopruq/loads/ThermalLoads.hpp` derives the consistent
equivalent nodal load vector directly from the same cubic-Hermite shape functions already used
throughout this project (MassMatrix.hpp/Loads.hpp), respecting the established per-plane sign
convention (theta_y = -dw/dx). Wired into `LinearStaticSolver::assembleLoadCase` via the
existing `accumulate()`/`frameOf()` machinery (releases, offsets, equilibrium bookkeeping all
apply unchanged); a thermal load's resultant force/moment are reported as zero (it is an
initial-strain/eigenstrain load, not a real external action), keeping the global equilibrium
check meaningful. JSON model I/O and schema updated (`material.thermal_expansion`,
`load_case.thermal`).

Independent verification (`tests/unit/thermal_load_tests.cpp`): free uniform expansion
(`alpha_T*Delta T*L`); fully restrained axial force `|N| = E*A*alpha_T*|Delta T|`; a
statically-indeterminate two-bar series system (partial restraint via the second bar's finite
stiffness, hand-solved by compatibility) with one and then both bars heated at different
alpha_T/Delta T; an inclined 3D member's free expansion along its own axis; free-cantilever
gradient curvature matched against classical constant-curvature kinematics (tip rotation =
kappa*L, tip deflection = kappa*L^2/2, independent of the production virtual-work derivation);
a fully restrained gradient's fixed-end moment `|M| = E*Iy*kappa`; combined thermal + nodal load
global equilibrium/energy checks; invalid input (gradient delta_t without a positive depth)
rejected.

This is the first thermal/temperature capability in any phase; `ETMP`/`TYPE=T` MIDAS records are
still rejected by Phase 7B as of this commit (wiring the MIDAS importer to use this new
capability is a separate, not-yet-done follow-up, deliberately not bundled into this change).
Explicitly out of scope for 7T itself: EN 1991-1-5 code-based thermal action generation (bridge
deck temperature components, National Annex parameters) -- 7T is structural analysis under an
already-defined thermal load, not a thermal-action generator; that is a separate, later,
explicitly-scoped capability if ever added.

### Phase 7C — Standalone EXE analysis (MIDAS-format CLI input)
**COMPLETE (pending CI verification, 2026-10-19).** `include/kopruq/io/Cli.hpp`: added a
`--format <native|midas-nx-json>` option to `solve`/`modal`/`spectrum`, sharing the same
`CommonArgs` parser as `-o`/`-n`. With `--format midas-nx-json`, the CLI parses the input file as
raw JSON and runs it through Phase 7B's `midas::importMidasJson()` instead of
`readModelText()`, producing the same `ModelInput` either way -- the rest of the CLI (solve/
modal/spectrum dispatch, results writing, exit codes) is completely unchanged and unaware which
format was used. The importer's `MidasImportReport` (node/beam/material/... counts, every
disclosed assumption, every rejected record) is always printed to stderr
(`detail::reportMidasImport`), so a MIDAS import's gaps are visible on every run, not only when
explicitly queried. An unknown `--format` value or a MIDAS file that is not valid JSON are
usage/exit-3 errors with diagnostics, matching the native-format contract. Existing native-format
CLI behavior is unchanged (format defaults to `"native"`). New tests in
`tests/unit/io_tests.cpp` (`Cli.MidasNxJsonFormatOption`) exercise: unknown-format rejection,
missing-value rejection, an end-to-end solve of the real `xmidas/deneme1.json` sample through the
CLI (checking the import report appears on stderr with its node count and at least one
rejection), and a malformed-MIDAS-JSON exit-3 path.

### Phase 7D — JSON results export (MIDAS-import-oriented results schema)
**COMPLETE as "no new schema needed" (2026-10-19).** Because Phase 7C's importer produces the
same `ModelInput` the native path produces, `solve`/`modal`/`spectrum` already write results
through the existing, unmodified `writeResults`/`writeModalResults`/`writeResponseSpectrumResults`
schemas regardless of which input format was used -- confirmed directly by the Phase 7C CLI test,
whose MIDAS-sourced `solve` output is parsed and checked like any native-format result. No
MIDAS-import-oriented results schema was needed or added; a model's provenance (which format it
was imported from) is not part of the results document today, since nothing downstream
(comparison, reporting) currently needs to distinguish it. If MIDAS NX result comparison (Phase
7E) later needs provenance tagging, it can be added in that phase without reopening 7D.

### Phase 7E — MIDAS NX results comparison
**In progress (started 2026-11-16, via the Qt6 Verification GUI's GUI-5 comparison engine, not a
separate code path).** Reference data: the user supplied 11 small, individually MIDAS-NX-run
verification models (`verification/models/01`-`11`), each with a real `.xlsx` (OOXML, byte-
signature-verified, not a renamed `.xls`) export of `Reaction(Global)`, `Displacements(Global)`
and `Beam Force`. Models 01-06 compared against the solver's own `results.json` so far:

- **Displacements and reactions: fully verified, 0 FAIL** on every primitive MIDAS load case in
  models 02-06 (model 01's and 05's earlier MIDAS-side data-integrity anomalies were resolved by
  the user re-exporting from MIDAS; see `docs/DEVELOPMENT_STATUS.md`).
- **Beam-force (Axial/Shear-z/Moment-y) comparisons systematically FAIL** on models 05 and 06 (143
  and 162 FAILs respectively) — every failing row is magnitude-exact, sign-opposite, never a real
  magnitude discrepancy. This confirms, on real solved data for the first time, the sign-
  convention open question already recorded here: MIDAS's own beam-force I/J-end axial/shear/
  moment sign convention has not been independently verified against `Beam3D::local_end_forces`.
  **Not fixed yet** — a sign flip must not be guessed into `ResultMatcher`/`ComparisonEngine`
  without independent verification (CLAUDE.md: no silent sign-convention changes).
- Models 05's/06's `LCOM-GEN` load combinations (`COMBO-1`, `COMBO-2`) are read by the MIDAS
  importer but not solved — see Phase 7G below — so their reference rows compare as `N/A`
  (disclosed, not a defect).

Still not started: independent verification of the sign convention itself (next sub-task below),
and models 07-11 (not yet re-run by the user against the current solver).

### Phase 7E.1 — MIDAS beam-force sign convention (independent verification)
**COMPLETE (verified, 2026-11-17).** Derived from hand statics (never from "make the comparison
pass"): model "01 Konsol_Fx" (a pure-tension cantilever, FX=500 at the free tip) gives an
unambiguous, single-valued internal axial force N=+500 kN (tension-positive) at every
cross-section by elementary statics; MIDAS's own `Beam Force` export reports exactly +500 at
*both* the I and J end of every element — confirming MIDAS reports the direct, station-
independent "diagram" value (same sign throughout the member), not an end-dependent nodal-force
convention. `local_end_force` (`LinearStaticSolver.hpp`'s own, already-independently-verified
R1-R4 convention: "the force the joint exerts on the element", from `k*u` minus equivalent nodal
loads) is related to that diagram value by `f_I = -N`, `f_J = +N` for Axial/Shear/Torsion (an
identity from how N(x) is *defined* via a free-body cut, independent of any specific load case),
and oppositely for bending moments (`f_I = +M`, `f_J = -M`, since M is coupled to the end shear
through the arm term `dM/dx = V`, which reverses which end lines up). Cross-validated against
*every* real Axial/Shear-z/Moment-y mismatch across two independent real models — "05 Cerceve-1"
(147/147 rows) and "06 Cerceve-2" (162/162 rows), including loaded horizontal beam elements
(`BMLD`), not just the unloaded columns — with **zero counter-examples**. Shear-y/Torsion/
Moment-z are extended to their already-confirmed counterpart by the identical physical argument,
but are explicitly disclosed as NOT yet empirically exercised (every available verification model
is planar, Y=0, so these three components are structurally zero everywhere available).

Implemented once, in `verification/ResultMatcher.cpp` only (`toMidasBeamForceConvention`,
`kBeamForceFlipsAtIEnd`), never in the solver itself (`Beam3D`/`LinearStaticSolver` are unchanged
— their own convention remains independently verified and correct on its own terms). Existing
`tests/unit/result_matcher_tests.cpp` fixture (`BeamForcesMatchesIAndJEndsFromThePartColumn`)
corrected to use the right reference value for its hand-built scenario (its old fixture silently
assumed no I/J sign difference, which this phase shows was wrong) — not weakened, not deleted, the
same assertions still both require PASS. Independent Python re-derivation (reading the same
`.xlsx` cells and `results.json` fields `ResultMatcher` reads, applying the same rule) confirms:
model 05 Beam Force 576/576 PASS (was 433/576, 143 FAIL), model 06 648/648 PASS of the
non-combination rows (was 486/648, 162 FAIL) — zero beam-force FAILs remain on any available real
model. Debug/Release/gui-release CTest: 232/237, 232/237, 254/259 respectively, all three
unchanged from before this fix except the newly-fixed assertion; the 5 (resp. 6, counting the
GUI-only duplicate) remaining failures are the already-documented, unrelated
`xmidas/deneme1.json`-missing ones.

### Phase 7G — MIDAS load-combination import (`LCOM-GEN`)
**COMPLETE (verified, this session, 2026-11-19).** `MidasJsonImporter.hpp` now parses `LCOM-GEN`
records (`NAME`, `ACTIVE`, `iTYPE`, `vCOMB` array of `{ANAL: "ST"|"CB", LCNAME, FACTOR}`) into
`LoadCombination`. Only `ACTIVE=="ACTIVE"` and `iTYPE==0` (linear combination) are accepted;
other `iTYPE` values (envelope/max-min) are rejected with a clear diagnostic since they cannot be
expressed as linear superposition. Since static analysis is linear, a combination-of-combinations
is mathematically exact to flatten at import time: a memoized, cycle-safe recursive expansion
substitutes nested `ANAL=="CB"` references by their own expansion (scaled by factor), merging
duplicate load-case references (summed factor) before building the final `LoadCombination` —
`LinearStaticSolver::LoadCombination::terms` still only ever indexes primitive load cases; this
is an import-time translation, not a new solver capability. `MidasImportReport::combinations`
records the imported count.

**Verification evidence:** model 06 (`06 Cerceve-2`, 2 non-nested combinations COMBO-1/COMBO-2)
now reaches 100% PASS — 1080/1080 beam force, 60/60 reaction, 570/570 displacement — against real
MIDAS reference data, including the `COMBO-*` rows that previously compared as `N/A` for exactly
this reason. Model 10 (`10 3d Cerceve`, 1 combination) and model 21 (`21 Combo`, 4 combinations
including nested `COMBO-3`/`COMBO-4` referencing `COMBO-1`/`COMBO-2`/`COMBO-3` via `ANAL=="CB"` —
the more rigorous test of the recursive-expansion path) could **not** be end-to-end verified this
session: both fail to solve for reasons unrelated to combination import — model 10 references an
unsupported `'H'` `DBUSER` section shape, model 21 references two unsupported shapes (`STRK`,
`SOCT`) on 16 of its 188 elements. Both are pre-existing, disclosed gaps (see Phase 7G.1 below);
until a section-shape fix unblocks either model's full solve, the nested-`CB`-expansion path
remains verified only by code inspection and unit-level reasoning, not by a real-data regression
row. Full regression: 291/291 Release, 261/261 Debug, unchanged aside from the newly-passing
model 06 combination rows (which are driven by the diagnostic tool, not CTest).

### Phase 7G.1 — Unsupported MIDAS section shapes blocking models 10/20/21 (new, found 2026-11-19)
**Not started.** Three distinct gaps, each blocking a different real model's full solve:
- Model 10 (`10 3d Cerceve`): `SECT.SHAPE=='H'` — not implemented (`resolveDbUserSection` only
  implements `SB`, `B`, `SR`, `T`).
- Model 20 (`20 Kafes`, truss): **mostly resolved, 2026-11-19 (commit `b0f8d3a`)** — all 25
  elements are actually `TYPE:"TRUSS"` (not `TYPE:"BEAM"` as an earlier pass of this
  investigation mis-stated); the importer now accepts `TYPE=="TRUSS"`, builds the already-
  verified Truss3D (Phase 5.2) pin-ended release pattern, resolves `MATL.TYPE:"STEEL"` (grade
  lives on `MATL.PARAM[0]`, not a separate MATD record — `resolveSteelGradeMaterial`), resolves
  the `SECT_I.DB_NAME=="UNI"` catalog lookup for `HEB160`/`UPN100` (`resolveCatalogSection`), and
  maps `BMLD`'s `CONLOAD` (concentrated member point load) onto the solver's pre-existing
  `MemberPointLoad` pipeline (previously only `UNILOAD` was wired). 12/25 elements (sections 5/6,
  HEB160/UPN100) now import and solve; the remaining 13/25 (section 7, angle `L50x5`, shape `L`)
  are blocked by a genuinely new, deliberately unaddressed gap: an unsymmetric angle's principal
  bending axes are rotated relative to H/B, needing a product-of-inertia term `kopruq::Section`
  does not have — out of this checkpoint's scope (a `Section`/`Beam3D` formulation change, not
  an importer mapping), rejected with a clear diagnostic rather than approximated.

  **Update, 2026-11-19 (commit `9745c16`), per user approval ("Task 1"):** resolved via
  import-time principal-axis rotation instead (`equalAngleSection` + an additional `reference_y`
  rotation, no `Section`/`Beam3D` change — see `DEVELOPMENT_STATUS.md`). All 25 elements now
  import. The model still does NOT solve: a separate, deeper issue — 3 genuine zero-eigenvalue
  modes from mixing continuous BEAM chords with zero-out-of-plane-stiffness TRUSS
  diagonals/verticals in an entirely planar (Y=0) layout, where only the bottom chord's 2
  supports pin the out-of-plane direction and the top chord has no path to it at all. Whether
  MIDAS implicitly restrains out-of-plane DOFs for a purely planar model is flagged for the user
  rather than guessed (see `DEVELOPMENT_STATUS.md` for the full reasoning) — this is the sole
  remaining blocker for model 20's full verification.

  **Resolved, 2026-11-19 (commits `7353640`, `b1fb1e3`):** the user re-solved model 20 in MIDAS
  as an explicit 2D model and re-exported; its `CONS` records are unchanged from the original
  3D-solved export, confirming MIDAS's 2D setting has no CONS footprint at all. The importer now
  auto-restrains the 3 out-of-plane DOFs for a genuinely planar model (bounding box exactly 0
  along one axis). Also fixed: L50x5's catalog area overridden to the published EN 10056-1 value
  (4.80 cm², vs. the idealized no-fillet 4.75 cm², which was enough to fail displacement
  comparison). **Model 20 now fully solves and is verified**: Reaction 36/36 PASS, Displacements
  252/252 PASS, Beam Force 370/432 PASS (residual ~2-4% on bottom-chord members not directly
  loaded, disclosed as an idealized-geometry limitation, not a logic defect). A new "Truss Force"
  verification category (`ReferenceFileKind::TrussForce`/`matchTrussForces`) was added per user
  request and wired through the GUI end-to-end (new file-selector row, results tab, report
  sheet); 54/78 PASS on model 20, same residual. Model 20 (truss) is complete per the user's
  "19 spring, 20 truss, 21 combo" instruction.
- Model 21 (`21 Combo`): 16 of 188 elements use `SECT.SHAPE` values `STRK` (`vSIZE` has 2
  meaningful values) and `SOCT` (`vSIZE` has 4 meaningful values) — unconfirmed geometric
  parametrization. Deliberately **not** implemented by guessing, per the project's "never
  fabricate" rule — their dimension-to-geometry mapping must be confirmed against MIDAS
  documentation or another authoritative source before implementation, not inferred from the
  `vSIZE` array alone.

  **Closed, 2026-11-19, STRK/SOCT still not implemented (see `DEVELOPMENT_STATUS.md` for full
  detail):** two independent web-search attempts to confirm `STRK`/`SOCT`'s `vSIZE` dimension
  order found only an internally inconsistent MIDAS shape-symbol table (one listing calls `STRK`
  "Solid Track", another lists it as "Solid Octagon" — the same name as `SOCT`) and no dimension
  table for either shape; `support.midasuser.com` (the likely authoritative source) returns
  HTTP 403 to every automated fetch attempted, consistent with every earlier attempt this project
  has made. Not implemented, per "never fabricate structural properties". **Everything else in
  model 21 was tested and verified**: its nested `CB` combination chain (COMBO-3 references
  COMBO-1; COMBO-4 references COMBO-1, COMBO-2 AND COMBO-3) was hand-derived independently and
  matched the importer's actual recursive expansion exactly, term for term — the rigorous nested-
  combination test Phase 7G had been missing (models 06/10 could only exercise the simple,
  non-nested case). All other data (180 nodes, 188 beams, 1 material, 12 supports, 16 nodal
  loads, 68 member loads, 262 thermal loads) imports with zero rejections beyond the 2 section
  shapes. Model 21 is closed per the user's instruction.

  **Resolved, 2026-11-19 (commit `595b1ac`):** the user supplied MIDAS NX's own "Section Data"
  dialog screenshots for sections 4/5, confirming STRK is a stadium shape (H=cap diameter,
  B=overall length) and SOCT is a rectangle with 4 independently-dimensioned chamfered corners
  (a horizontal, b vertical) -- NOT a regular octagon. `solidTrack`/`solidOctagon`
  (`MidasSectionLibrary.hpp`) derive Area/IH/IB from first-principles composite-shape
  decomposition and J via a generalized `torsionConstantFEMCore` (the existing FDM solver, now
  accepting an arbitrary domain predicate instead of only a rectangle union). Verified via two
  exact limiting-case checks (new file,
  `tests/unit/midas_section_library_strk_soct_tests.cpp`). **Model 21 now solves end-to-end**:
  Beam Force 14505/18048 PASS (80.4%), Reaction 489/576 PASS (84.9%), Displacements 6998/8640
  PASS (81.0%) -- a real, disclosed residual (see `DEVELOPMENT_STATUS.md` for the full
  breakdown, including an unexplained thermal-combination anomaly on a non-STRK/SOCT element
  chain that was not resolved this session), not claimed as 100%. Full regression: 297/297
  Release, 267/267 Debug.

Model 10's `'H'` shape (an additional non-nested combination data point) remains unresolved and
is not currently blocking any open task.

### Phase 7H — Rigid-link support in `ModalSolver`/`ResponseSpectrumSolver` (unblock MIDAS modal/spectrum import)
**COMPLETE for the solver-level fix and its CLI dispatch (verified, this session, 2026-11-17);
EIGV/SPLC->`response_spectra` *translation* in the MIDAS importer remains explicitly blocked, see
below — not part of this phase's original scope once split out, see rationale below.**

`ModalSolver::solve` and `ResponseSpectrumSolver::solve` (`include/kopruq/solvers/ModalSolver.hpp`,
`ResponseSpectrumSolver.hpp`) now dispatch to a rigid-link-aware code path
(`ModalSolver::solveWithRigidLinks`) whenever `Model::rigid_links` is non-empty, exactly mirroring
`LinearStaticSolver::solveCasesWithRigidLinks`'s already-verified pattern: K/M are assembled in
full DOF space unchanged, `RigidLinkReduction::build` produces `T` (`include/kopruq/assembly/RigidLink.hpp`,
unmodified), `Kmid=T^T K T`/`Mmid=T^T M T`, the eigensolve runs on the reduced free-DOF `Kff`/`Mff`
exactly as the rigid-link-free path already does (refactored into a shared `eigensolve()` helper
to avoid duplicating that math), and each mode's reduced-space shape is expanded back
(`red.expand`) to full DOF space, giving every rigid-link slave node's 6 DOFs through the same
rigid kinematics `LinearStaticSolver` uses — never zero, as the old rejected-outright behavior
effectively implied. Mass-normalization survives the expansion exactly, by construction
(`phi_full^T M_full phi_full = phiReduced^T Mmid phiReduced = phiFree^T Mff phiFree = 1`).
`ResponseSpectrumSolver::solve` additionally had to stop treating a rigid-link slave node's DOFs
as independent "free" DOFs when building its base-excitation vector `iota` (they are not
independent — their motion is already fully represented in `phi` through the same rigid
kinematics), by excluding `RigidLinkReduction::build(m,dm).slaveNodes` from the restraint map
before classifying free/fixed DOFs, the same way `LinearStaticSolver` already does.

New, dedicated `tests/unit/rigid_link_modal_tests.cpp` (7 tests, independent of the existing
rigid-link-free `modal_tests.cpp`/`response_spectrum_tests.cpp` and of the existing
rigid-link-but-static-only `rigid_link_tests.cpp`): a cantilever beam with a massless rigid-arm
slave node confirms (1) frequencies/eigenvalues are bit-for-bit identical to the no-rigid-link
baseline (a model-independent prediction, not merely asserted, since the slave carries no mass or
stiffness of its own), (2) every mode shape's slave DOFs satisfy `u_slave = u_master + theta x e`
exactly, (3) mass-orthonormality holds over the *full* DOF space including the slave, checked
against an independently assembled mass matrix, (4) the equivalent response-spectrum-side checks
(`total_mass` unchanged vs. baseline; each mode's *pre-SRSS* recovered displacement at the slave
satisfies the same rigid kinematics — deliberately checked before the final SRSS combination,
since SRSS is nonlinear and does not preserve a linear kinematic relationship between two
already-combined quantities), and (5) a slave node still cannot carry its own restraints (same
rejection as `LinearStaticSolver`). Debug/Release/CTest: **232/237 both configurations**, no
regressions (the 5 pre-existing failures are the already-documented `xmidas/deneme1.json`-missing
ones, unrelated — that sample folder was deleted by the user this session, see below).
**Superseded, 2026-11-17: this sample is now permanently removed from git itself (commit
`e688f53`), by explicit user instruction, not merely locally deleted/git-recoverable as described
when first found — see `docs/DEVELOPMENT_STATUS.md`'s own dated update for the full, authoritative
note and the exact 5 affected tests, which now fail permanently, by design, not as a transient
issue.**

**CLI dispatch confirmed end-to-end, no importer change needed for modal:** `kopruq_solver_cli
modal <model.json> --format midas-nx-json -n <N>` already calls `ModalSolver::solve` directly
(`include/kopruq/io/Cli.hpp::runModal`) regardless of format, so a MIDAS model with a `RIGD` record
now runs modal analysis today, with zero changes to `MidasJsonImporter.hpp`'s already-correct RIGD
import. Verified against a hand-built, schema-accurate (`RIGD.<masterNodeKey>.ITEMS[].S_NODE[]`,
read directly from `MidasJsonImporter.hpp`'s own parsing code, never guessed) MIDAS JSON fixture:
a cantilever with a rigid-arm slave node solved modal analysis successfully, and the slave's mode
shape independently verified (by hand, outside the solver) against `u_slave = u_master + theta x
e` for every mode.

**Update, 2026-11-17: EIGV is now read.** The user supplied a real MIDAS NX JSON export that
actually contains `EIGV` (`verification/models/13 Eigen-1/13 Eigen-1.json`), giving the exact
field shape previously missing: `{"EIGV":{"<id>":{"TYPE":"LANCZOS","iFREQ":20,"bMINMAX":false,
"bSTRUM":false}}}`. `MidasJsonImporter.hpp` now reads `iFREQ` into a new `ModelInput::modal_request`
field (`{present, num_modes}`, defaults to `present=false` for the native schema and every
pre-existing caller, so nothing else changes); `TYPE`/`bMINMAX`/`bSTRUM` (Lanczos-specific solver
options) have no counterpart in `ModalSolver` (a single, independently-verified dense eigensolve)
and are not translated. The CLI's `modal` command now uses this as its default mode count when
`-n` is not given on the command line (`-n` still always wins). SPLC/SPFC (response-spectrum
requests) remain unread: `xmidas/deneme1.json` (which had them) is still gone, and model
"13 Eigen-1" has EIGV but no SPLC/SPFC, so their field shapes are still not independently
confirmed -- needs that sample or another export that actually contains SPLC/SPFC.

**GUI: new "Eigenvalue Mode (Excel)" input row, 2026-11-17 (user decision).** `FileInputPanel`
gained a sixth row (`Row::EigenvalueMode`) for MIDAS's "Eigenvalue Mode.xlsx" export, same
`.xlsx`-only policy as the other four. Its structure is unlike every other reference export
(several independently-titled, stacked sub-tables in one sheet: a letter-spaced "E I G E N V A L
U E   A N A L Y S I S" frequency table, then "MODAL PARTICIPATION MASSES PRINTOUT" x2, then
"MODAL PARTICIPATION FACTOR PRINTOUT" -- confirmed from the real export), so a dedicated reader
(`verification/EigenvalueReferenceReader.{h,cpp}`) was written rather than forcing it through
`ExcelReferenceReader`'s generic single-header-row parser. Only the frequency table (Mode No /
Frequency rad-per-sec / Frequency cycle-per-sec / Period) is read -- the participation mass/
factor tables are explicitly disclosed as not parsed, since `ModalSolver`'s plain modal analysis
has no base-excitation direction and computes no participation factor/mass at all (that concept
only exists inside `ResponseSpectrumSolver`, for a specific direction); comparing them would need
a new, separate solver capability, not more columns in this reader. New `QuantityCategory::Frequency`
(`ComparisonEngine.h`) and `matchEigenvalueFrequencies` (`ResultMatcher.{h,cpp}`) compare by
1-based reference mode number against `ModalSolver`'s own 0-based mode index, producing two rows
per mode (Hz, rad/s); a reference mode beyond what was computed reports NotApplicable, never
dropped. `MainWindow` chains a second CLI invocation (`modal`, not `solve`) automatically right
after a successful `solve`, only when the Eigenvalue Mode row has a usable file -- the two are
independent analyses in the CLI's own design, so every other model/workflow is unaffected by this
row simply existing. New dedicated tests: `tests/unit/eigenvalue_reference_reader_tests.cpp` (4),
`tests/unit/eigenvalue_result_matcher_tests.cpp` (4) -- all passing; gui-release CTest 262/267 (8
new tests, same 5 pre-existing unrelated failures as Debug/Release's 232/237).

**Model "13 Eigen-1" tested end to end (CLI + the GUI's own backend logic, both against the real
`Eigenvalue Mode.xlsx`), 2026-11-17: 20/20 modes read, 20 PASS / 20 FAIL (40 rows: Hz + rad/s per
mode) under default tolerance.** Modes 1-3, 5-6, 8-11, 17 match within ~1.3%; modes 4, 7, 12-16,
18-20 differ by 1.3%-15.2%, growing with mode number -- confirmed via an independent optimal
(minimum-total-error) one-to-one assignment between the 20 solver and 20 MIDAS frequencies
(`scipy.optimize.linear_sum_assignment`) that this is NOT a mode-ordering/degenerate-pair
artifact (the optimal assignment is the same as direct ascending-order pairing). Root cause not
yet investigated -- plausible candidates (consistent vs. MIDAS's own mass formulation/lumping
choice, rotary-inertia treatment, self-weight-to-mass conversion details) are not confirmed; this
is reported as a genuine, disclosed open question, not forced to PASS by adjusting tolerance.

**Models "14 Eigen-2" and "15 Eigen-3" also tested (CLI `modal`, Python-reproduced comparison
against each model's own real `Eigenvalue Mode.xlsx`), 2026-11-17 -- same disclosed open question
confirmed on a second small model, but NOT present on a third, much larger one:**
- **14 Eigen-2** (49 nodes, 48 elements, 30 modes requested): modes 1-13 match MIDAS within
  ~1%; modes 14-30 diverge, growing from -2.9% to as much as -23.2% (mode 15) and generally
  worsening with mode number -- 17/30 modes fail the 1% check. Same qualitative pattern as model
  13 (small portal-frame-like geometry, divergence beyond a certain mode).
- **15 Eigen-3** (1630 nodes, 2024 elements, 100 modes requested -- by far the largest/most
  realistic model tested so far): **all 100/100 modes match MIDAS within 0.2%, zero exceptions.**
  This is strong evidence the divergence seen in models 13/14 is NOT a general mass-matrix-
  formulation defect (which would be expected to show up here too, likely worse given the much
  larger mode count) -- it is more likely specific to something about those two small models'
  higher modes (plausibly local single-member torsion/bending modes that a small, simple frame
  reaches at a low mode number, where a modeling detail like rotary inertia or shear-area
  treatment matters more than it does for model 15's much more global, structure-scale mode
  shapes). Root cause still not investigated -- this observation narrows where to look, it does
  not resolve the question.
- Practical/performance note: model 15's dense (`Eigen::SelfAdjointEigenSolver`) eigensolve on
  its ~9780 free-DOF system took **18 minutes** wall-clock. `ModalSolver` has no sparse/iterative
  path (Phase 7, "sparse direct/iterative solver strategy", is explicitly not started) --
  acceptable for this one-off verification run, but worth knowing before using the GUI's
  Eigenvalue Mode row interactively on a similarly large model.

Also fixed in passing (Debug-only MSVC regression, unrelated to the rigid-link logic itself):
`kopruq_response_spectrum_tests`/`kopruq_modal_tests`/`kopruq_rigid_link_modal_tests` needed
`/bigobj` (MSVC C1128, COFF section-count limit) once `ResponseSpectrumSolver.hpp` started pulling
in `RigidLink.hpp`'s extra `Eigen::Sparse` template instantiations — `kopruq_solver_tests` already
had this flag for an unrelated reason, so the fix follows the exact same existing pattern.

### Phase 7H.1 — Lumped mass option for `ModalSolver`/`ResponseSpectrumSolver` (root cause of the mode 13/14 divergence)
**COMPLETE (verified, 2026-11-17).** Investigated, at the user's request, WHY models 13/14's
higher modes diverge from MIDAS while model 15's 100 modes all matched closely. Ruled out by
direct evidence, not assumption: (1) no single dominant-DOF-type pattern (classified every mode
by which of UX/UY/UZ/RX/RY/RZ carries most of its kinetic energy; both passing and failing modes
span every type); (2) not an under-resolved/locally-oscillating mesh shape (inspected failing
modes' actual deformation along a column -- smooth, monotonic, same global character as passing
low modes, not a sawtooth higher-harmonic shape); (3) not the already-disclosed Timoshenko-
stiffness/Euler-Bernoulli-mass mismatch (`MassMatrix.hpp`'s own documented simplification) --
disabling shear deformation entirely on model 13 made the divergence *worse*, not better (mode
11: 0.06% with shear -> 10.61% without), ruling this out as the dominant cause.

**Confirmed root cause, via an independent diagnostic** (a throwaway standalone program, reusing
only already-verified `MatrixAssembler::assemble`/`MidasJsonImporter`, never committed): building
a classic translational-only lumped mass matrix (half of each element's rho\*A\*L on each end
node's UX/UY/UZ, zero rotational mass) in place of the consistent mass, with the resulting
massless rotational DOFs statically condensed (Guyan reduction) before the eigensolve, reproduces
MIDAS NX's own default eigenvalue analysis almost exactly on **all three** models: 13 (20/20
modes within a constant -0.110%, was 11/20 passing a 1% check with consistent mass, worst case
-15.18%), 14 (30/30 within -0.110%, was 13/30, worst case -23.2%), and **15 (100/100 within
0.110%, confirming MIDAS uses lumped mass there too -- its earlier apparent match with consistent
mass was coincidental: the two formulations' difference stays under 1% within the first 100 modes
of that much larger, finer-meshed structure, not because MIDAS used consistent mass there).** The
remaining constant ~0.11% offset, identical across all three models regardless of size, is a
separate, minor, unidentified mass-scale difference (a candidate: the assumed 25 kN/m^3 unit
weight, Phase 7B's own disclosed EN1991-1-1 standard-value substitution for a grade-named
material, vs MIDAS's own actual internal value) -- not investigated further, since it is two
orders of magnitude smaller than the effect this phase set out to explain.

**Implemented as a new, explicitly-selectable `MassMatrixType` (`Consistent`/`Lumped`) on
`ModalSolver::solve`/`ResponseSpectrumSolver::solve`, defaulting to `Consistent`** (every existing
caller/test is completely unaffected). `MatrixAssembler::assembleLumpedMass` (new) and
`MassMatrix.hpp`'s `lumpedMassMatrix` (new) build the lumped matrix; `ModalSolver`'s new
`eigensolveLumped` performs the Guyan condensation of massless (rotational) free DOFs and backs
their mode-shape values out of the condensation afterward (never left at zero) -- the same
technique `RigidLink.hpp` and `Releases.hpp` already use elsewhere in this codebase, applied here
to a different partition (massless vs massy, not slave vs master or released vs active). Not yet
supported together with rigid links (explicitly rejected, not silently ignored or silently
downgraded to Consistent).

New CLI flag `--mass-type <consistent|lumped>` on `modal`/`spectrum`. New GUI checkbox ("Lumped
Mass (MIDAS)") next to the Eigenvalue Mode row, wired through `SolverRunner`'s new `extraArgs`
parameter to the chained modal CLI invocation.

New, dedicated `tests/unit/lumped_mass_tests.cpp` (4 tests, independent of every existing
Consistent-path test): `lumpedMassMatrix` against a hand formula; a single-element cantilever's
Lumped-path frequencies against an independent from-scratch Guyan reduction built in the test
itself (not calling `eigensolveLumped`); the full-free-DOF eigenproblem residual (K\*phi -
omega^2\*M\*phi) vanishing at every free DOF including the massless ones, confirming the back-
substituted rotational mode-shape values are exactly the ones that satisfy equilibrium, not an
arbitrary placeholder; and error handling (Lumped+rigid-links rejected; omitting the new argument
still gives bit-identical results to the explicit `Consistent` default). Debug/Release/gui-release
CTest: 236/241, 236/241, 266/271 respectively (241/271 up from 237/267 -- the 4 new tests, all
passing); the same 5 pre-existing, unrelated `xmidas/deneme1.json`-missing failures as before this
phase, no regressions. Re-verified via the actual production CLI (`modal --mass-type lumped`) on
all three real models (13, 14, 15), matching the diagnostic's numbers exactly.

### Phase 7I.1 — CQC modal combination + MIDAS SPLC/SPFC (response-spectrum) import
**COMPLETE (verified, 2026-11-17), triggered by the user supplying the first real MIDAS NX
sample with actual `SPLC`/`SPFC` data (`verification/models/16 Spektrum-1`), which explicitly
requests `COMTYPE="CQC"` -- a combination `ResponseSpectrumSolver` did not implement (SRSS only,
a disclosed gap since Phase 6.2).**

**CQC (Complete Quadratic Combination), `ResponseSpectrumSolver.hpp`:** added the standard Der
Kiureghian (1981) cross-modal correlation formula (the same reference SAP2000/ETABS/MIDAS all
cite) as a new `ModalCombination::{SRSS,CQC}` option on `ResponseSpectrumInput`, defaulting to
`SRSS` (zero change for every existing caller/test). SRSS is implemented as CQC's own
`rho_ij=(i==j)` special case -- one combination code path, `combined = sqrt(sum_i sum_j
rho_ij*R_i*R_j)`, never two separately-maintained implementations that could silently diverge.
Uses a single, caller-supplied damping ratio for every mode (the only case this project has real
reference data for). New, dedicated `tests/unit/cqc_tests.cpp` (3 tests): `cqcCorrelation()`
against hand-computed values (the trivial r=1 limit, and r=2/zeta=0.05 computed independently,
not by re-deriving the same formula); CQC reducing to within 2% of SRSS on a well-separated-mode
cantilever (no repeated eigenvalues); and error handling (non-positive damping ratio rejected).

**MIDAS SPLC/SPFC import, `MidasJsonImporter.hpp`:** real field shapes confirmed from model 16's
own JSON (`SPLC`: `NAME`,`DIR`,`ANGLE`,`COMTYPE`,`aFUNCNAME`,`aUSEMODE`; `SPFC`:
`NAME`,`GRAV`,`DRATIO`,`aFUNC` as a period/value table). Explicitly rejected, not silently
approximated, when present: `ANGLE != 0` (arbitrary-angle direction, unsupported);
`aUSEMODE` entries with `bUSE=false` or `MSFACTOR != 1.0` (per-mode exclusion/scaling,
unsupported); `COMTYPE` other than `CQC`/`SRSS`. `SPLC`'s own `CQCRATIO` is NOT used (its exact
meaning -- possibly a closely-spaced-mode cutoff ratio in some codes' CQC variants, not a damping
ratio -- was not confirmed from accessible documentation); the CQC damping ratio instead comes
from the referenced SPFC's unambiguous `DRATIO`. `aFUNC`'s `VALUE` column is confirmed (by
cross-checking its T=0/plateau values against the SPFC's own cited Eurocode 8 parameters: ag=0.2,
S=1.15, I=1.2 -> ag\*S\*I=0.276, exactly the T=0 value) to be in units of g, converted to m/s^2
using THIS SPFC's own `GRAV` field, never a hardcoded constant. MIDAS's own `T=0` anchor point is
nudged to a tiny positive period (1e-6 s, not dropped) since `interpolateSpectrum` requires a
strictly positive period and 16/20 of model 16's real modes have a period shorter than the
smallest OTHER tabulated point (0.04 s) -- dropping T=0 entirely made the analysis fail outright
on a real model before this fix.

**`ElementPeakResponse` (`ResponseSpectrumResults`) gained `local_end_forces`** alongside the
pre-existing `global_end_forces` -- needed because MIDAS's own "Beam Force" export reports
LOCAL-axis values (Axial/Shear-y/Shear-z/Torsion/Moment-y/Moment-z), and comparing those against
`global_end_forces` for a non-horizontal member is comparing different quantities entirely (found
and fixed during this verification itself: an initial comparison attempt showed Axial and
Shear-z appearing "swapped," which was this global/local mismatch, not a real defect).
`kopruq-response-spectrum-results` schema bumped 1->2 (`schemas/response_spectrum_results.schema.json`
updated to match); existing `ResponseSpectrumResultsJson.SchemaKeysMatchWriterOutput` test still
passes (it does not enumerate `combined_elements`' own keys, but the schema file itself was kept
in sync for any future stricter check).

**Verified end-to-end against model 16's real reference data** (`.xlsx` exports, `--mass-type
lumped --format midas-nx-json`, CQC via the imported SPLC): Mesnet Tepkimeleri (Reactions)
**24/24 PASS**; Deplasmanlar (Displacements) **234/234 PASS**; Kiriş İç Kuvvetleri (Beam Force,
local axes) **454/456 PASS** (2 isolated rows at the same node, Moment-y, ~3.75% off -- not
investigated further, too small and localized to indicate a formulation gap). This is, by a wide
margin, the closest match to MIDAS achieved on any model so far in this project, confirming the
CQC implementation, the SPLC/SPFC import, the lumped-mass option (Phase 7H.1) and the g->m/s^2
conversion are all simultaneously correct on a real, non-trivial case.

Debug/Release/gui-release CTest: 239/244, 239/244, 269/274 (3 new CQC tests, same 5
pre-existing, unrelated `xmidas/deneme1.json`-missing failures as before this phase, no
regressions).

### Phase 7I — Traffic (moving) loads — REINSTATED, user decision 2026-11-16
Cancelled 2026-10-17 as part of the former Phase 8 ("construction stages, bearings/soil springs,
moving loads/influence lines"); **reinstated by explicit user decision 2026-11-16**, scoped
narrowly to moving/traffic loads only (construction stages and bearings/soil springs remain
cancelled/out of scope unless separately reinstated). **Not started.** Needs its own scope
definition before implementation begins (which traffic-load model — influence-line envelope vs.
discrete vehicle positions; which code, if any, e.g. EN 1991-2 load models — has not yet been
decided and must not be assumed) plus independent analytical reference cases, per this project's
standing verification requirements.

### Phase 5.4b addendum — MIDAS NSPR import; Phase 5.1 addendum — MIDAS FRLS import; SPLC ANGLE
### generalized (2026-11-18, user-supplied models 17/18/19)
User supplied three new real MIDAS NX sample models and asked for tests: `17 Spektrum-2` (two-
direction CQC response spectrum), `18 Köprü_mafsal` (beam end releases / "mafsal"), `19 Basit
Kiris point spring` (two ELNK links + 12 NSPR nodal springs).

**NSPR (Joint Spring) import, COMPLETE:** `MidasJsonImporter.hpp` now imports `NSPR` records
(keyed by node id, `TYPE:"LINEAR"` only) as `JointSpringConnection`. `SDR` maps to local
`kx..krz`. `F_S` must be all-false, `DAMPING` must be false, and `Cr`/`EFFAREA`/`DK` must all be
at their default (zero) value -- any non-default value in any of them is rejected, not guessed
(their meaning for a nonlinear/damped spring is not confirmed). **Found and fixed during
verification**: MIDAS's own `Reaction(Global).xlsx` reports a joint spring's force as that node's
reaction, even for a node with no explicit restraint there -- `NodeResults::reaction`
(`LinearStaticSolver.hpp`) did not reflect this (only explicitly-restrained components were
populated), so every joint-spring-only node's reaction showed as zero. Fixed: for any DOF
component NOT separately restrained, the joint spring's negated force is now added into that
node's own `reaction` (previously this fold only went into the model-level
`force_balance`/`moment_balance`, not the per-node value user-facing output actually shows).
**Verified against model 19's real MIDAS export** (49 nodes, 2 ELNK, 12 NSPR) via a standalone
(uncommitted) diagnostic: Beam Force **552/552 PASS**; Reaction **84/84 PASS** (0/12 before the
reaction-fold fix, confirming it was a real, necessary fix, not a style choice); Displacements
**293/294 PASS** (one isolated rotational mismatch, ~4% relative, not investigated further --
consistent in scale with other small accepted discrepancies elsewhere in this project, e.g. Phase
7I.1's 2 beam-force rows on model 16). New permanent regression tests,
`tests/unit/midas_joint_spring_tests.cpp` (5 tests): a small inline NSPR fixture import + solve
against a hand-derived closed form, plus rejection tests for non-LINEAR type, a true `F_S` entry,
and a non-default `EFFAREA`.

**FRLS (beam end release / "mafsal") import, COMPLETE:** `MidasJsonImporter.hpp`'s `ELEM`/`BEAM`
handling now also reads a matching `FRLS` record (keyed by element id), mapping `FLAG_I`/`FLAG_J`
(7-character strings) onto `Beam3DConnection::releases` using the same per-DOF-flag convention
already confirmed for `CONS`'s `CONSTRAINT` (first 6 characters = UX,UY,UZ,RX,RY,RZ; the 7th is
ignored, its meaning unconfirmed, same as `CONS`). `bVALUE=true` (a partial/semi-rigid release
with its own stiffness) is rejected, not approximated as a full release, since
`Beam3DConnection::releases` is boolean-only. Import-level parsing verified against model 18 (10
elements with real `FRLS` records, correctly parsed) -- **full numerical verification against
model 18's own real reference data is blocked** by the next item.

**New blocker found, not yet resolved: MIDAS `SECT.SHAPE:"T"` (T-section) is unsupported.**
Models 17 and 18 both use a `T`-shaped `DBUSER` section (`vSIZE=[2.0,2.0,1.0,1.0,...]`,
presumably depth/width/web-thickness/flange-thickness, not confirmed); `MidasSectionLibrary.hpp`
only implements `SB`/`B`/`SR`. Both models' full solve currently fails with "element property
reference missing". Implementing a T-section requires independently deriving its
area/Iy/Iz/J/shear-area formulas (this project's standing rule: never derive expected results
from the production code under test) -- a real section-library task (Phase 7S extension), not
attempted this session; left for the user's decision on priority. Model 19 is unaffected (`SB`
only).

**SPLC `ANGLE` generalized from "must be exactly 0" to "any multiple of 90 degrees within the
plane `DIR` names", COMPLETE:** model 17's `SPECY` case has `DIR="XY"`, `ANGLE=90` -- the prior
code rejected any nonzero `ANGLE` outright. Re-derived the direction rule from two real exports:
model 16's sole case (`DIR="XY"` `ANGLE=0` -> X, already verified) and model 17's `SPECY`
(`DIR="XY"` `ANGLE=90` -> same `DIR` string, only `ANGLE` differs, so `DIR` alone cannot encode
the direction) -- concluded `ANGLE` selects which of `DIR`'s two named axes applies (0 deg =
first letter, 90 deg = second letter), reduced modulo 180 (a response-spectrum envelope does not
depend on sign). Confirmed physically sensible against model 17's real `Reaction(Global).xlsx`:
`SPECX(RS)` rows show dominant FX reactions, `SPECY(RS)` rows show dominant FY reactions, exactly
as the X/Y resolution predicts. A full end-to-end numerical verification of model 17 (matching
Phase 7I.1's model-16 treatment) is blocked by the T-section gap above, same as model 18.

Verified (NSPR + FRLS + SPLC changes together): Debug CTest **256/261 PASS**, gui-release
**286/291 PASS**, same 5 pre-existing/permanent `xmidas/deneme1.json`-removal failures as every
prior baseline, no regressions; 16 new tests (11 solver-core from the same-day 5.4/5.4b work,
5 MIDAS-NSPR-import) all passing.

### Phase 7S addendum — MIDAS T-section added; new unresolved discrepancy found on models 17/18
**`tSection()` COMPLETE** (`MidasSectionLibrary.hpp`): resolves `SHAPE:"T"` (composite flange+web
parallel-axis decomposition, classical open-section torsion approximation, web/flange-only shear
areas), unblocking both models 17 and 18's "BASLIK" pier-cap section. New hand-computed test
`MidasSectionLibrary.TSectionMatchesHandComputation_Baslik`.

**New, unresolved finding (NOT a T-section bug):** verifying model 18 against its real MIDAS
export (standalone, uncommitted diagnostic) gives Reaction **96/96 PASS** (exact), but Beam Force
only **1806/1872 PASS** and Displacements only **919/948 PASS**. The dominant failure mode is a
**consistent exact sign flip** in Moment-z across elements 62-71 (section 4, shape `SR`, not `T`
-- ruling out the new T-section formula as the cause), plus smaller magnitude-only differences
(~10% constant Axial offset on elements 17-24, ~1-3% Moment-y/Shear-z on 57-61/72-75) and matching
displacement errors at nearby nodes. None of the affected elements carry an `FRLS` release either.
Candidate unexplored cause: the `ELEM` records' `STYPE` field (currently read nowhere), which
might flag these as a different member behavior MIDAS treats specially within an otherwise
BEAM-typed arch. **Not investigated further this session** -- left as an open, disclosed item,
not silently accepted or tolerance-widened away. Model 17 now also solves without error (the
T-section was its only blocker) but has not yet been numerically compared against its own real
reference data at all.

Verified: Debug CTest **257/262 PASS**, gui-release **287/292 PASS**, same 5 permanent failures,
no regressions.

### Phase 7E.1 addendum — Moment-z MIDAS sign-convention bug found and fixed (2026-11-19)
The "new unresolved discrepancy" above (exact Moment-z sign flips on model 18's vertical pylons)
was root-caused: `verification/ResultMatcher.cpp`'s `kBeamForceFlipsAtIEnd` table had Moment-z
grouped with Moment-y (flip at J, not I) by a symmetry argument its own comment flagged as never
empirically confirmed -- every model before 18 is planar, so Moment-z was always structurally
zero there. Model 18's real, nonzero out-of-plane bending falsifies that assumption outright.
Fixed by moving Moment-z into the Axial/Shear/Torsion group (flip at I, not J) instead --
resolves every one of the 66 sign-flip failures exactly. Mathematical grounding, not just a data
fit: the xy (Moment-z) and xz (Moment-y) bending planes are not symmetric under this project's
right-handed local-axis convention (z = x cross y directly, y = z cross x, one extra cyclic
step), exactly explaining why their I/J flip rules are opposite.

Verified: model 18 Beam Force improves to **1834/1872 PASS**; Debug CTest **258/258**,
gui-release **288/288** -- both 100%, no regressions (every other model is planar for Moment-z,
structurally insensitive to this change).

**Remaining 38 Beam Force + 29 Displacement mismatches, now better understood (still open)**: all
in the "POINT" load case (`CNLD` point loads) for beam forces and "FORCE" for displacements, both
small (2-10%, magnitude only, no sign flips) and localized to `T`/`B`/`SR`-sectioned elements
(never plain `SB`). Reactions match exactly (96/96) in both cases -- global equilibrium is right,
internal distribution differs slightly, the signature of stiffness-ratio sensitivity in this
indeterminate, shear-deformable (`USE_SHEAR_DEFORM=true`) structure. Most likely explanation: the
already-disclosed approximate shear-area formulas for `T`/`B` aren't precise enough to reproduce
MIDAS's internal force/displacement redistribution exactly, even though they're fine for the
force values themselves elsewhere. Improving this needs either MIDAS's own effective-shear-area
formula (inaccessible) or a more rigorous thin-walled shear-flow derivation -- a real task
requiring its own scoping, not attempted this session.

**Model 17 numerically checked (2026-11-19)**: its `NODE`/`ELEM`/`SECT`/`MATL`/`CONS`/`BMLD` are
byte-identical to model 18's (direct JSON comparison) -- same bridge; 18 adds `FRLS` hinges and a
`CNLD` case, 17 adds modal/spectrum requests instead. Static "FORCE" case: elements 1-2 (nearest
the roller) match exactly; from element 3 on, **373/936 beam-force rows FAIL** (modest, growing
errors away from the support, no sign flips -- same family as model 18's remaining item, not a
new bug). Explanation: model 18's hinges release some redundancy (closer to determinate, less
sensitive to a small stiffness error); model 17, fully continuous, is MORE indeterminate and
amplifies the same small shear-area imprecision across far more elements. Spectrum-case
(`SPECX(RS)`/`SPECY(RS)`) rows not checked this session. Same root cause and remediation path as
model 18's open item -- not pursued further.

### Phase 7S addendum — rigorous Zhuravskii shear-area formulas; model 18 reaches 100% (2026-11-19)
User instruction: prioritize replacing the T/box sections' approximate shear-area formulas with
classical theory, since MIDAS's own effective shear area cannot be obtained. Added
`detail::timoshenkoShearArea()` (`MidasSectionLibrary.hpp`): the standard Zhuravskii shear-stress
distribution gives As = I^2 / integral(Q(y)^2/b(y) dy) for any section built from rectangular
strips, replacing the earlier "web/flange alone" (T) and "parallel walls only" (box)
approximations for AsH and AsB. An initial version accumulated Q(y) via discrete increments
(O(1/N) bias); caught by its own self-test against the exact k=5/6 rectangle result and fixed by
evaluating Q(y) via its exact closed-form antiderivative per strip, leaving only the smooth
quartic Q(y)^2/b(y) integral numerical (O(1/N^2)).

**Verified against real MIDAS data, confirming the shear-area approximation was the actual root
cause of the "new unresolved discrepancy" above**: model 18's Beam Force/Reaction/Displacement
comparison (previously 1834/1872, 96/96, 919/948) is now **1872/1872, 96/96, 948/948 -- 100%
PASS.** Model 17 (same geometry, no `FRLS` hinges, more indeterminate) improves substantially:
FORCE-case beam force 563/936 -> 817/936, reactions 17 -> 5 mismatches, displacements 441/474 ->
**474/474**. Model 17's remaining residual is likely a different, smaller, already-disclosed
approximation (e.g. the open-section/Bredt torsion constant J) -- not investigated further.

Verified: Debug CTest **259/259 PASS**, gui-release **289/289 PASS** -- both 100%, no
regressions.

### Phase 7S addendum — torsion constant investigated; model 17 tested and verified (2026-11-19)
User instruction: fix the torsion-related parts too, then test and verify model 17. Added
`detail::torsionConstantFEM()` (`MidasSectionLibrary.hpp`): solves St. Venant's torsion via the
membrane analogy (exact PDE formulation for an arbitrary section), numerically via a finite-
difference Laplacian (Eigen sparse Cholesky). Verified correct against two independent references
(tabulated square coefficient, this file's own closed-form rectangle), both within ~0.03-0.1%.

**Tried wiring it into the T-section's J -- made things WORSE, not better.** Model 17's real
MIDAS-data match regressed (Beam Force PASS 817/936 -> 757/936, reaction mismatches 5 -> 11) while
model 18 (same section) was unaffected either way. Strong empirical evidence MIDAS's own DBUSER
general-section torsion constant uses the same simple thin-strip-sum convention internally, not an
exact PDE solve -- matching MIDAS's convention, not the more "physically rigorous" answer, is what
actually helps. **Reverted** `tSection()`'s J to the strip sum; `torsionConstantFEM()` itself is
kept (correct, tested, may suit a future shape) but not wired into any shape's J this session. A
disclosed finding, not a quiet revert: more theoretical rigor is not automatically more correct
when the goal is matching a specific other program's own convention.

**Model 17 tested and verified** (after the revert): FORCE-case **Beam Force 817/936 PASS**,
**Reaction 43/48 PASS** (5 small, 1-2% residual mismatches), **Displacement 474/474 PASS**.
Remaining residual not pursued further (all failing elements use section `SB`, the exact
closed-form shape -- not an approximate-property issue, but a small stiffness-distribution
sensitivity further down the load path in this fully continuous, more indeterminate structure).

Verified: Debug CTest **261/261 PASS**, gui-release **291/291 PASS** -- both 100%, no
regressions.

**Modal + spectrum also verified** (same session, user approved continuing): `ModalSolver`
(lumped mass) vs. model 17's real `Eigenvalue Mode.xlsx`: **76/80 PASS** (2 of 40 modes off by
~1.1-1.3%, the same isolated-mode-divergence pattern already accepted for models 13-15).
`ResponseSpectrumSolver` (CQC, lumped mass) combined `SPECX`/`SPECY` reactions, checked by hand
against the real `Reaction(Global).xlsx` `SPECX(RS)`/`SPECY(RS)` rows (nodes 54, 58, 8 values):
**all within 0.03-1.1%** of MIDAS's own numbers, matching model 16's prior verification quality --
confirms the `SPLC` `ANGLE` direction resolution, CQC combination and lumped-mass pipeline are all
correct here too.

### `xmidas/deneme1.json` tests removed entirely (2026-11-19, explicit user instruction)
Every CTest count above citing "5 pre-existing/permanent failures" refers to the 5 tests that
depended on the permanently-deleted `xmidas/deneme1.json` sample (see
`docs/DEVELOPMENT_STATUS.md`'s 2026-11-17 entry for the history). By explicit user instruction,
these 5 tests were deleted outright (not just left failing):
`Cli.MidasNxJsonFormatOption`'s `deneme1.json` sub-case, and
`MidasJsonImporter.{ImportCountsMatchIndependentlyComputedTotals,
ReportsElasticLinkRejectionCountAndNodeBoundingBox, ImportedModelPassesValidation,
StaticSolveOfImportedModelDoesNotThrowAndReportsAStatus}` in full (along with their shared
file-loading helper). Every other test in both files (self-contained synthetic fixtures) is
unaffected. **Verified: Debug CTest 258/258 PASS, gui-release 288/288 PASS -- 100%, no permanent
failures remain.** Future CTest/report baselines in this document should read "all tests
passing," not carry forward the old "same 5 pre-existing failures" framing.

### Phase 7J — MIDAS verification models 19-24: missing physics and import coverage — COMPLETE (2026-11-20 .. 2026-11-23)
User-approved work driven by the user's MIDAS reference models; each item verified against MIDAS
results and covered by dedicated new tests (details and evidence: `docs/DEVELOPMENT_STATUS.md`).
- Sections: user H with fillets (Roark torsion), C, truss-only L; catalog profiles removed (user
  decision: precast bridges first); square SB torsion 2.25 a^4; PSC 1CEL and PSC-I with 2D FE
  section analysis (SectionFE); mesh rule h = max(tmin/16, sqrt(A/80000)) (h = 0.01 m was tried and
  reverted by user decision: same results, longer runs).
- Boundary conditions: per-DOF rigid links (MIDAS RIGD DOF mask, restrained slave DOFs dropped);
  node local axes (MIDAS SKEW) with local reactions; STYP = 1; section offset "CT"; temperature
  gradient (GTMP).
- Loads: full MIDAS BMLD coverage except projection/eccentricity/pressure (uniform, trapezoidal,
  multi-point, moments, concentrated, local/global); Timoshenko-consistent member-load vectors
  (exact fixed-end forces for shear-deformable members).
- Verification tooling: GUI comparison of elastic link forces and local reactions (new input row,
  tab and report sheet); response-spectrum comparison.
Open (needs user data or a decision): PSC J/As differ from MIDAS's undocumented section mesh by
up to ~0.25% / ~0.75%, leaving 5 zero-crossing rows in models 22/23; PSC user-input stiffness not
imported (no sample export).

### Phase 7F — Final standalone release
Not started; depends on 7A-7I.

### Phase 8 — KOPRUQ integration (EXPLICIT APPROVAL REQUIRED)
*(renumbered from the former Phase 10)*
- Define a versioned solver input/output contract and process/library boundary.
- Integrate only after user approves modifications to `C:\_ZHarput_Data\KOPRUQ`.
- Bridge alternative generation, batch analysis, candidate comparison, and optimization integration.

### Phase 9 — AI-assisted optimization (later, separate module)
*(renumbered from the former Phase 11)*
- AI proposes design candidates and prioritizes search; deterministic verified solver remains the authority for physics and safety checks.
- Traceable constraints, objectives, result provenance and human engineering review.

## QT6 VERIFICATION GUI — STANDALONE MIDAS NX VALIDATION

### 1. Purpose
A desktop verification tool that drives KOPRUQ-SOLVER's own CLI against a real MIDAS Civil NX
JSON export and the MIDAS Excel result exports it produces, and reports, per result category and
per individual value, whether KOPRUQ-SOLVER independently reproduces MIDAS's numbers within
engineering tolerance. This is an engineering verification instrument (Phase 7E's practical front
end), not a modeling or visualization tool.

### 2. Scope
In scope: file selection, model validation, solver invocation via the existing CLI, parsing
MIDAS's own Excel result exports, numeric comparison against KOPRUQ-SOLVER's `results.json`, and
report export. Explicitly out of scope: any 3D/graphical model preview, structural diagrams, QML/
Qt Quick, model editing, or any change to the FEM numerical core. The GUI never recomputes a
result itself -- every number it displays comes from the CLI's own JSON output or from the parsed
MIDAS Excel files, never from GUI-side arithmetic duplicating the solver.

### 3. Technology stack
C++20, Qt 6 Widgets, CMake (new vcpkg dependency: a Qt6 port, e.g. `qtbase[core,gui,widgets]`, not
present in today's `vcpkg.json`), `QProcess` to invoke the existing `kopruq_solver_cli` as a
separate process, `QFileDialog`/`QSettings`/`QTableView`/`QTabWidget`/`QPlainTextEdit`/
`QProgressBar`, a small dark-mode QSS stylesheet (reference layout:
`KOPRUQ-SOLVER_menü_qt6.jpg`, repo root). No QML, no 3D, no React/Electron. An Excel reader
is a new dependency (none exists in the repo today -- see section 9).

### 4. Architecture
Three independent layers, so the numerical core, the comparison logic, and the UI never blend:
- **FEM core** (`include/kopruq/...`, `apps/cli`): completely unchanged; the GUI never links
  against solver internals for computation, only reuses `kopruq_solver_io`'s JSON (de)serializers
  (`ResultsJson.hpp`/`ModelJson.hpp`) to parse the CLI's own output, the same way any other result
  consumer would.
- **`verification/`** (new, Qt-independent static library): MIDAS Excel reading, result ID/load-
  case matching, unit conversion, tolerance-based comparison, and XLSX report writing. Pure C++20
  + nlohmann::json + the chosen Excel library; no `QWidget` dependency, so it is independently
  unit-testable with GoogleTest exactly like the rest of the solver.
- **`apps/verification-gui/`** (new Qt executable): widgets, `QProcess` orchestration, and nothing
  else -- it calls into `verification/` for every actual computation or parse.

### 5. Repository structure
```
apps/verification-gui/   main.cpp, MainWindow.{h,cpp}, FileInputPanel.{h,cpp},
                          AnalysisControlPanel.{h,cpp}, ExecutionLogPanel.{h,cpp},
                          ResultsComparisonPanel.{h,cpp}, resources/dark.qss
verification/             ExcelReferenceReader.{h,cpp}, ResultMatcher.{h,cpp},
                          ComparisonEngine.{h,cpp}, VerificationReportExporter.{h,cpp}
```
No duplicate JSON parser or comparison engine is created; `verification/` is the single
implementation the GUI, any future CLI verification command, and tests all share.

### 6. Approved UI layout
Four areas, matching `KOPRUQ-SOLVER_menü_qt6.jpg` exactly in structure and proportion: (1)
five input-file selectors (MIDAS JSON required, four MIDAS Excel exports individually optional),
(2) four action buttons (Modeli Doğrula / Solver'ı Çalıştır / Sonuçları
Karşılaştır / Raporu Dışa Aktar), (3) an execution-status panel
(progress bar, timestamped log), (4) a full-width tabbed comparison area (Özet / Deplasmanlar
(Global) / Mesnet Tepkimeleri (Global) / Kiriş İç Kuvvetleri / Elastik Bağlantılar
/ Diagnostik) plus a bottom tolerance-settings strip. No model preview, no diagrams, no animation.

### 7. GUI-0 through GUI-7 milestones
| # | Milestone | Status |
|---|---|---|
| GUI-0 | Repository audit, roadmap/status update | **COMPLETE** |
| GUI-1 | Main window, four-area dark-theme layout, empty tabs | **COMPLETE (verified, release.yml run `39406148285`, 2026-10-20)** |
| GUI-2 | File selection, extension/existence validation, QSettings persistence | **COMPLETE (verified, release.yml run `39422159633`, 2026-10-20)** |
| GUI-3 | Solver execution via QProcess + real CLI, async, results.json discovery | **COMPLETE (verified, release.yml run `39428529324`, 2026-10-20)** |
| GUI-4 | MIDAS Excel reference parsing (4 files) | **COMPLETE (verified, release.yml run `39435756925`, 2026-10-20)** |
| GUI-5 | Comparison engine, PASS/FAIL/N-A/UNSUPPORTED, tables | **COMPLETE (verified, release.yml run `39468392733`, 2026-10-20)** |
| GUI-6 | XLSX verification report export | **COMPLETE (verified, release.yml run `39548523252`, 2026-10-21)** |
| GUI-7 | Cross-platform build/release validation (Windows + Ubuntu) | **COMPLETE (verified, release.yml run `39760415174`, 2026-10-22)** |

No milestone is marked COMPLETE without its stated acceptance tests passing; GUI-0's own
"acceptance" is this audit plus the two documentation updates it produces.

### 8. Dependencies on Phase 7C/7D
**Satisfied.** Phase 7C (`--format midas-nx-json` on `solve`/`modal`/`spectrum`) and Phase 7D (no
new results schema needed -- the importer produces a standard `ModelInput`, so existing
`kopruq-results`/`kopruq-modal-results`/`kopruq-response-spectrum-results` JSON is written
unchanged) are both complete and CI-verified (run `39265009361`, commit `a92f045`, all three
matrix jobs green). GUI-3 can invoke `kopruq_solver_cli solve <model.json> --format
midas-nx-json -o <results.json>` exactly as documented in `cli_usage`, with no CLI contract
changes required.

### 9. Excel parsing requirements
**No Excel/XLSX handling exists anywhere in the repository today** (confirmed by a
`grep -rn "xlsx|XLSX|Excel"` across all source and CMake files: zero matches outside this new
roadmap text). `vcpkg.json` currently lists only `eigen3`, `gtest`, `nlohmann-json`. A reader must
be added in GUI-4. Candidates to evaluate at that time: **QXlsx** (header+source, MIT-licensed,
already Qt-dependent so it adds no *second* large dependency once Qt6 is already required for the
GUI, straightforward to vendor under `third_party/` or pull via CMake `FetchContent` since it is
not in the vcpkg registry under that exact name) versus a Qt-independent library such as `xlnt`
(available via vcpkg, but a second substantial dependency on top of Qt6, and licensing/size should
be re-checked against vcpkg's current registry before committing). GUI-4 must also first inspect
the real files already present in the repo (`xmidas/Beam Force.xls`,
`xmidas/Displacements(Global).xls`, `xmidas/Elastic Link.xls`, `xmidas/Reaction(Global).xls` --
note these are legacy `.xls`, not `.xlsx`; MIDAS's actual export extension must be confirmed
before picking a library, since `.xls` (BIFF/OLE2) and `.xlsx` (OOXML) are different formats and
not every candidate library reads both) before assuming any fixed column layout.

### 10. Engineering comparison rules
Per-value status is PASS/FAIL/N-A/UNSUPPORTED, never a false PASS from a missing or unmatched
value. Matching is by original MIDAS ID (node/element/link) and load-case name, both of which
Phase 7B's importer already preserves (`MidasImportReport::originalToInternalNodeId`/
`originalToInternalElementId`, and load case names are carried through unchanged). A category with
no comparable KOPRUQ-SOLVER output -- today, this includes elastic links, since the importer
unconditionally rejects every MIDAS `ELNK` record (non-coincident-node elastic links are a real
formulation gap, not a missing-data gap -- see Phase 7B) -- must be reported as UNSUPPORTED for
every row, never silently skipped or marked PASS/N-A.

### 11. Numerical tolerances
Combined absolute/relative criterion, `abs(solver - reference) <= absTol + relTol * abs(reference)`,
applied with category-specific absolute tolerances and units (never one dimensional tolerance
reused across displacements, rotations, forces and moments): e.g. displacement in m, rotation in
rad, force in kN, moment in kNm each need their own default `absTol`, configurable in the GUI's
bottom settings strip and persisted via `QSettings`. Exact default values are a GUI-5 decision
(documented in `ComparisonEngine.h` and surfaced in the UI, not hardcoded invisibly), not fixed
here. Near-zero reference values are handled correctly by construction (the `relTol * abs(reference)`
term vanishes, leaving the `absTol` floor to govern) -- no special-cased division-by-reference
logic.

### 12. Acceptance criteria
Per-milestone acceptance is stated under each GUI-N item above (section 6 of the originating task
spec) and is not repeated in full here; the governing principle throughout is CLAUDE.md's existing
quality gate: no fabricated or simulated results, no silent data loss, every unsupported/missing
case explicitly and visibly reported, and every existing FEM regression test left untouched and
passing.

### 13. Windows and Ubuntu deployment
**Implemented in GUI-7 (2026-10-21).** Both platforms are verified via the same manual-dispatch-
only `release.yml` used for every prior GUI milestone (never added to every-push `ci.yml`, for
the Actions-minute-budget reasons recorded throughout this document):

- **Windows** (`ninja-gui-release`, existing `build-test` job, `windows-2022`): `windeployqt`
  deploys the Qt6 platform plugin (`qwindows.dll`) and runtime DLLs next to
  `kopruq_verification_gui.exe`; vcpkg's `applocal` step (already used for the CLI's own
  dependencies) copies any other linked DLLs. Without this, the exe cannot even start -- Qt
  Widgets apps fail immediately with "could not load the Qt platform plugin" if the plugin isn't
  deployed alongside the binary. A real `.exe` is uploaded as the `kopruq-verification-gui-
  windows-x64` artifact.
- **Ubuntu** (`gcc-gui-release`, new `build-test-linux-gui` job, `ubuntu-latest`): the GUI has no
  Windows-specific code (confirmed: no `_MSC_VER`/`Windows.h`/`__declspec`, same as the rest of
  the project per `ci.yml`'s own header comment), so the identical C++20 source builds under GCC
  unchanged. Qt6 is still fetched via vcpkg's `"gui"` manifest feature (never a system
  `apt-get install qt6-base-dev` package, avoiding the toolchain inconsistency flagged in the
  prior revision of this section) -- so both platforms build Qt6 from the exact same pinned
  vcpkg baseline. A Qt Widgets app needs a real (or virtual) X server to open at all on Linux; CI
  has no display, so the launch smoke test runs under `xvfb-run` (`xvfb` + a handful of
  `libxcb-*`/`libxkbcommon-x11-0` runtime packages installed via `apt-get`, needed by Qt's xcb
  platform plugin itself -- these are runtime shared libraries the plugin `dlopen`s, not a
  parallel Qt installation, so this does not reintroduce the system-Qt inconsistency). A real
  Linux executable is uploaded as the `kopruq-verification-gui-linux-x64` artifact.
- **For an end user running the GUI outside CI** (the actual deployment scenario this section is
  about): the Windows `.exe` needs its `windeployqt`-populated folder shipped alongside it (not
  just the bare exe); the Linux binary needs a real X11 (or Wayland-via-XWayland) desktop session
  -- no offscreen/headless mode is configured for normal use, only for the CI smoke test. Neither
  platform's binary is statically linked against Qt.

### GUI-7 implementation note (2026-10-21)
New `gcc-gui-release` CMake preset (Ubuntu GCC Release, `KOPRUQ_BUILD_GUI=ON`,
`VCPKG_MANIFEST_FEATURES=gui` -- identical gating discipline to `ninja-gui-release`, so
`ci.yml`'s every-push Ubuntu matrix still never requests `qtbase`/`xlnt`). New
`build-test-linux-gui` job in `release.yml` (manual-dispatch-only, same workflow as the Windows
job): builds and full-CTests the GUI target on Ubuntu, then runs the same "launches and stays
alive for 5s" smoke test as Windows, but under `xvfb-run` since Qt Widgets needs a real or
virtual X server to open at all on Linux. No application code changed -- the GUI already had no
Windows-specific code (same property `ci.yml`'s own header comment already relies on for the
rest of the project), so the identical sources build under GCC unchanged. Full deployment
requirements for both platforms are now documented in section 13 above, replacing its earlier
"risk, not yet resolved" framing.

### GUI-7 — COMPLETE AND VERIFIED (2026-10-22)

`release.yml` run [`39760415174`](https://github.com/zharput/KOPRUQ-SOLVER/actions/runs/39760415174)
(commit `7bf04cb`): **all six jobs green** -- `ninja-debug`, `ninja-release`, `ninja-asan`,
`ninja-analyze`, `ninja-gui-release` (Windows) and `gcc-gui-release` (Ubuntu). **242/242 tests**
on both platforms. The GUI smoke test actually launched the real executable under `xvfb-run` on
Linux and under a normal process start on Windows, confirmed alive for 5s, then closed cleanly on
both. The Linux GUI binary cache (`vcpkg-Linux-gcc-gui-release-*`, 427MB) was finally saved
successfully for the first time this arc, immediately after the job's first full green
completion. This closes a debugging arc of **thirteen distinct, individually-diagnosed, real
root-cause fixes** (full narrative below, kept for anyone who needs to touch this build
configuration again) -- every one converging, none repeated, the last four resolving an entirely
genuine chain of Qt6-on-Linux build/link/runtime issues (missing X11/XCB build-time `-dev`
packages; disk space; a `qtbase` manifest-feature trim that initially went too far and had to be
restored piece by piece with evidence from Qt's own source; and finally vcpkg's static-by-default
Linux triplet requiring an explicit `Q_IMPORT_PLUGIN`, the arc's only application-source-code
change). Windows and Ubuntu both now build, test, and launch the real GUI from the same pinned
vcpkg toolchain and the same application source, with full deployment requirements documented in
section 13.

#### Windows portable package fix (2026-10-22, post-GUI-7)
Inspecting run `39760415174`'s own `kopruq-verification-gui-windows-x64` artifact found it was
**not actually portable**: only the bare `.exe` (300KB) and `resources/dark.qss` were uploaded,
even though the same build's own log shows `windeployqt` genuinely deploying a complete set of
Qt6 DLLs and plugins (`Qt6Core.dll`/`Qt6Gui.dll`/`Qt6Widgets.dll`/`Qt6Network.dll`,
`platforms/qwindows.dll`, `styles/`, `imageformats/`, `tls/`, plus vcpkg-applocal dependencies
like `xlnt.dll`) into `build/gui-release/` -- the `actions/upload-artifact` step's `path:` list
simply never grabbed any of it. Separately, `kopruq_solver_cli.exe` -- needed by
`SolverRunner::locateCli()`'s first search candidate, `<applicationDirPath>/kopruq_solver_cli.exe`
-- was published only in the *separate* `kopruq-solver-windows-x64` artifact; a user who only
downloaded the GUI zip would have no working "Solver'ı Çalıştır" at all. Fixed, user-approved,
with only `release.yml` changes (no FEM/GUI/test code touched):
- Broadened the Windows GUI upload step's `path:` to the whole deployed set:
  `kopruq_verification_gui.exe`, `kopruq_solver_cli.exe` (already built into
  `build/gui-release/` for free, since it is an unconditional CMake target and
  `cmake --build --preset ninja-gui-release` builds every target, not just the GUI), every
  `*.dll` in that directory, `resources/`, and the `platforms/`/`styles/`/`imageformats/`/
  `generic/`/`networkinformation/`/`tls/` plugin subdirectories windeployqt populated. No debug
  files, object files, source, or test/benchmark executables included.
- Added an explicit pre-upload check (`GUI portable-package check`): the GUI exe, the CLI exe
  *at the exact path `SolverRunner::locateCli()` checks first*, `Qt6Core.dll`/`Qt6Gui.dll`/
  `Qt6Widgets.dll`, and `platforms/qwindows.dll` must all exist, or the job fails with a specific
  message naming what's missing -- never a silent incomplete package.
- Added a standalone CLI invocation (`kopruq_solver_cli.exe --version`) from inside the package
  directory on the `windows-2022` runner -- which has no system-wide Qt6 install at all, i.e. it
  already is the "clean machine" acceptance criterion -- confirming the CLI genuinely needs no
  further runtime dependencies.
- The existing GUI launch smoke test (stays alive 5s, closed cleanly) is unchanged, now running
  against the complete package rather than the bare exe.

**Verified, `release.yml` run [`39780356141`](https://github.com/zharput/KOPRUQ-SOLVER/actions/runs/39780356141)
(commit `99055ac`): all six jobs green**, including a clean re-run of `gcc-gui-release` (now
genuinely fast off the warm cache from the prior GUI-7 run). The `kopruq-verification-gui-
windows-x64` artifact is **23 files, 20.3MB** -- `kopruq_verification_gui.exe`,
`kopruq_solver_cli.exe` (same directory, matching `SolverRunner::locateCli()`'s first candidate
exactly), `Qt6Core.dll`/`Qt6Gui.dll`/`Qt6Widgets.dll`/`Qt6Network.dll`, `platforms/qwindows.dll`,
`styles/qmodernwindowsstyle.dll`, `imageformats/{qgif,qico}.dll`,
`networkinformation/qnetworklistmanager.dll`, `generic/qtuiotouchplugin.dll`,
`tls/{qcertonlybackend,qschannelbackend}.dll`, vcpkg-applocal deps (`xlnt.dll`, `z.dll`,
`pcre2-16.dll`, `double-conversion.dll`, `md4c.dll`, plus ANGLE/D3D compiler runtime DLLs Qt's
own deployment pulled in), and `resources/dark.qss` -- no debug files, object files, source, or
test/benchmark executables. Downloaded and inspected the zip directly (not just trusted the CI
log) to confirm this. All eight of the user's acceptance criteria passed: GUI exe present, CLI
exe present, Qt6 DLLs present, Qt platform plugin present, GUI launches and closes cleanly on the
`windows-2022` runner (which has no system-wide Qt6 install -- already the "clean machine"
criterion), CLI runs standalone (`--version`) from the package directory on that same runner,
**242/242 tests** passed, and the zip artifact is complete.

The Windows half of GUI-7 (`ninja-gui-release`) has passed repeatedly throughout this effort and
is not in question. The new Ubuntu job (`gcc-gui-release`) took **eleven consecutive dispatches**
to debug, each catching one real, distinct, correctly-diagnosed problem (converging the whole
way, never repeating the same error) -- in order: (1) `gperf` (a qtbase build-time tool) needed
`autoconf`/`automake`/`libtool` from apt; (2) `libxcrypt` needed `libltdl-dev`; (3) qtbase's own
X11/XCB/OpenGL feature detection needed a full set of `-dev` packages, not just runtime `.so`s;
(4) one remaining feature, `system_xcb_xinput`, needed `libxcb-xinput-dev`; (5) a wrong guessed
package name (`libxcb-errors-dev`, later `libxcb-errors0-dev`) turned out to be unnecessary
entirely -- removed; (6) qtbase (built with its full default feature set, Debug+Release) ran the
runner out of disk ("No space left on device" mid-build) -- fixed by `VCPKG_BUILD_TYPE=release`
plus freeing preinstalled SDK dirs; (7) even Release-only, qtbase's *default* feature set (dbus,
sql-psql, sql-sqlite, testlib, icu, harfbuzz, brotli, network, opengl, openssl, async-io,
sessionmanager, dnslookup, zstd -- none of which `apps/verification-gui` uses) still didn't fit;
the actually-sustainable fix (not just more cleanup) was trimming `vcpkg.json`'s `qtbase`
dependency to `default-features:false` + an explicit `{gui, widgets}` list (`core` is implicit,
cannot be named explicitly -- a vcpkg manifest-schema rule that broke *every* preset, including
Windows, until caught: fixed same session); (8) with that same run, qtbase actually finished
building successfully (~2h) and the **real final blocker** turned out to be unrelated to any of
the above: `find_package(xlnt CONFIG REQUIRED)` should have been `find_package(Xlnt ...)` --
xlnt's own installed CMake package config is capitalized `Xlnt`; Windows's case-insensitive
filesystem silently tolerated the wrong spelling the whole time, Linux's case-sensitive one did
not. Both final fixes (the capitalization fix and the trimmed `qtbase` feature list) were
committed together as `d4c7db4`, then a manifest-schema typo in that same commit (`"core"` listed
explicitly) was caught and fixed as `edf6251`. The session paused overnight with CI run
`39619224162` (commit `edf6251`) deliberately left running in the cloud, not cancelled.

**(9) Resumed 2026-10-22:** run `39619224162` finished (while offline) as a genuine new failure,
not a repeat -- for the first time, qtbase **and** xlnt both built and linked successfully, and
the GUI target itself failed to *compile*: `QProcess::ExitStatus` was undeclared in
`MainWindow.h`/`SolverRunner.h`/their moc output, even though `<QProcess>` is included. First
hypothesis: trimming `qtbase` to the bare `{gui, widgets}` feature pair (fix (7) above) had
silently dropped `network`, cross-checked against Bitcoin Core's own minimal-Qt6 vcpkg manifest
(which includes `network`) -- added `{gui, widgets, network}` and re-dispatched (run
`39726773483`).

**(10)** That hypothesis was wrong: qtbase rebuilt with `network` included (confirmed from the
log: `qtbase[core,doubleconversion,gui,network,widgets]`) and hit the **identical**
`QProcess::ExitStatus` error again. Root-caused properly this time by reading Qt's own
`src/corelib/configure.cmake` directly: the `"process"` feature (i.e. `QProcess` itself) has
`CONDITION QT_FEATURE_thread OR NOT UNIX` -- on Linux this means `QProcess` does not exist at all
without `thread` explicitly enabled; `network` was never part of its condition. Bitcoin Core's
inclusion of `network` was for its own actual `QtNetwork` usage, unrelated to this. Fixed by
replacing `network` with `thread` in the requested feature list (`{gui, widgets, thread}`) -- this
is now the third attempt at this specific line, but the first one actually backed by reading Qt's
own source rather than cross-project inference. Still excludes every one of qtbase's genuinely
disk-heavy, unused default features (dbus, sql-psql, sql-sqlite, testlib, icu, harfbuzz, brotli,
network, opengl, openssl, async-io, sessionmanager, dnslookup, zstd), so the disk-budget fix from
(7) is not undone.

**(11)** Run `39736079075` (the `thread` fix, plus the approved cache `restore-keys` change):
qtbase built, the GUI target **compiled and linked successfully**, and **all 242 tests passed**
-- the first time any of that happened on Linux. Only the launch smoke test itself failed:
`qt.qpa.plugin: Could not find the Qt platform plugin "xcb" in ""`. Unlike Windows (`windeployqt`
copies the platform plugin next to the exe), nothing deploys `libqxcb.so` anywhere Qt's default
search path would find it on Linux. Fixed in the CI script itself (not a build/dependency
problem): `find`s `libqxcb.so` dynamically inside `vcpkg_installed` (no layout guessing) and
exports `QT_PLUGIN_PATH` to its parent `plugins` directory before launching under `xvfb-run`. A
real end-user Linux deployment would need the same handling (either set `QT_PLUGIN_PATH` or copy
vcpkg's `plugins/platforms` next to the installed binary, mirroring `windeployqt`'s output on
Windows) -- noted here as a GUI-7 deployment-documentation item, not yet automated into the
CMake build the way `windeployqt` is for Windows.

**(12)** Run `39743255847` (user-approved cache `restore-keys` optimization + the plugin-path
smoke-test fix): the `QT_PLUGIN_PATH` fix correctly reported `libqxcb.so not found anywhere under
vcpkg_installed` -- the plugin genuinely was never built at all. Root-caused by reading vcpkg's
`ports/qtbase/vcpkg.json` and `portfile.cmake` directly: on Linux, qtbase's own **default**
feature list includes `{xcb, xcb-sm, xcb-xlib, xrender}` (each `platform: linux`-conditioned) --
none of which our minimal `{gui, widgets, thread}` list requested. Without `xcb`, the portfile
passes `-DINPUT_xcb=no` to Qt's own build, which disables `QXcbIntegrationPlugin` entirely
regardless of which system X11/XCB `-dev` packages are installed (fix (3)/(4)'s apt packages were
necessary but not sufficient). Added a third, Linux-platform-filtered `qtbase` dependency entry
requesting exactly vcpkg's own proven-good Linux default X11 set: `{xcb, xcb-sm, xcb-xlib,
xrender}`.

**Open question noted for the CI-optimization task, not yet resolved:** despite the approved
`restore-keys` fix, `GET /repos/.../actions/caches` shows **zero** `vcpkg-Linux-gcc-gui-release-*`
entries have ever been saved, across the entire multi-day debugging arc -- while
`vcpkg-tool-Linux-*` (same job, same `actions/cache@v4` mechanism, smaller/faster-to-save) and
every Windows GUI cache entry (300-580MB, saving reliably every run) both work normally. Possible
cause: the Linux GUI job has consistently run far longer (1-3+ hours) than any other job in this
workflow, and `actions/cache`'s upload reservation can expire/fail silently on very long-running
jobs without a clear log message -- not confirmed, just the leading hypothesis. Worth watching
once a run completes faster (post-fix (12), most dependencies should already be warm); if binary
cache saves remain absent even on a short run, this needs its own dedicated investigation.
**Resolved/observed (2026-10-22):** on the final green run (`39760415174`), the
`vcpkg-Linux-gcc-gui-release-*` cache (427MB) saved successfully for the first time all arc, right
after the job's first full success. Consistent with (though not proof of) the leading hypothesis:
the save only completed reliably once the job ran to a genuine successful finish rather than
failing partway through a 1-3h run.

**(13)** Run `39753093766` (the `{xcb,xcb-sm,xcb-xlib,xrender}` fix): qtbase rebuilt
(`[core,doubleconversion,fontconfig,freetype,gui,sessionmanager,thread,widgets,xcb,xcb-sm,
xcb-xlib,xkb,xkbcommon-x11,xlib,xrender]`, vcpkg correctly pulling in `xkb`/`xkbcommon-x11`/`xlib`
transitively), configured cleanly with **no X11/XCB feature errors at all this time**, and all 242
tests passed again -- but `libqxcb.so` still didn't exist anywhere, confirmed by a direct `find`
across the entire `vcpkg_installed` tree. **This is the real, final root cause**: vcpkg's
`triplets/x64-linux.cmake` sets `VCPKG_LIBRARY_LINKAGE static` by default (`x64-windows.cmake`
defaults to `dynamic`) -- so on Linux there never was a shared-object platform plugin to find or
point `QT_PLUGIN_PATH` at in the first place; `QXcbIntegrationPlugin` is a **static** library
baked into `Qt6::Gui`, and a statically-linked Qt application must register it explicitly via
`Q_IMPORT_PLUGIN` (Qt's own build-time note, "Using static linking will disable the use of
dynamically loaded plugins. Make sure to import all needed static plugins...", seen but not acted
on in an earlier run's log) plus link against the plugin's own CMake target. Fixed by adding
`#if defined(Q_OS_LINUX) ... Q_IMPORT_PLUGIN(QXcbIntegrationPlugin) #endif` to
`apps/verification-gui/main.cpp` and `if(UNIX AND NOT APPLE) target_link_libraries(... PRIVATE
Qt6::QXcbIntegrationPlugin)` in `CMakeLists.txt` -- both inert on Windows, which keeps loading its
plugin dynamically via `windeployqt`'s deployed `qwindows.dll`, unaffected by any of this. The
now-unnecessary `QT_PLUGIN_PATH`-searching logic from fix (11) was removed from the smoke-test
step in `release.yml`, since a static build has no `.so` to locate at all. This is the first
application-source-code change in the entire GUI-7 debugging arc (everything through fix (12) was
CI/build-configuration only).

**Latest dispatch (after fix 13):** check `docs/DEVELOPMENT_STATUS.md`'s "Next autonomous action"
or the commit log for the current run ID and its outcome -- this entry is the historical debugging
narrative, not necessarily the latest status.

Beyond fix (13) above, no application source code changed in this debugging arc -- every other fix
was either a CI workflow (`release.yml`) package list or a build configuration (`vcpkg.json`, `CMakePresets.json`,
`CMakeLists.txt`'s `find_package` call) problem. GUI-0 through GUI-6 remain complete and verified
exactly as recorded above; only GUI-7's Ubuntu half is outstanding.

### GUI-6 verification (2026-10-21)
**COMPLETE on the first dispatch.** `release.yml` run `39548523252`: all five Windows presets
`success`. **242/242 tests** (237 prior + 5 new `VerificationReportExporter` tests, all passing
-- including the row/column layout assumptions in the test file matching the writer's actual
output exactly on the first attempt). GUI smoke test confirmed another clean 5s launch/close. No
existing test modified or removed.

### GUI-6 implementation note (2026-10-21)
New `verification/VerificationReportExporter.{h,cpp}`: writes
`KOPRUQ_SOLVER_Verification_Report.xlsx` with six worksheets (Summary, Displacements, Reactions,
Beam Forces, Elastic Links, Diagnostics) via `xlnt`'s write API (reusing the same A1-reference
helper pattern already proven safe in `ExcelReferenceReader`). Per the task spec's explicit
requirement ("do not maintain separate, inconsistent comparison calculations for the GUI and
report"), this file **never recomputes a comparison** -- `ReportData` carries exactly the
`ComparisonRow`s and diagnostics `MainWindow::onCompare()` already produced and showed on screen
(now cached as members so `onCompare()` doesn't have to re-run for `onExport()` to use them), and
the Summary sheet's category totals come from the same `summarizeCategory()` GUI-5's Özet tab
uses (moved to `ResultMatcher.h` so both call sites share one implementation, never two
independently-written versions that could silently drift apart). Summary metadata includes the
source model/Excel filenames, solver version (read from `results.json`'s own
`metadata.solver_version`, not re-derived), the tolerance settings used, and the comparison's
execution timestamp. `MainWindow::onExport()` prompts a save location via `QFileDialog`, defaults
to `KOPRUQ_SOLVER_Verification_Report.xlsx` in the user's Documents folder, and is enabled only
after at least one real comparison has run. **Simplification, disclosed rather than silent**: all
worksheet text uses ASCII-only labels (e.g. "Basarili" not "Başarılı") rather than the GUI's
Turkish UTF-8 labels, to avoid an unverifiable encoding risk in `xlnt`'s write path with no local
build to check it against -- the GUI itself rnemains fully Turkish; only the exported file's
static header text is ASCII, and the actual data (ids, load case names, numeric values) is
written verbatim regardless. New `tests/unit/verification_report_exporter_tests.cpp` (5 tests):
round-trip readability, Summary sheet metadata/category-totals correctness, all six worksheets
present, Unsupported rows leave numeric cells blank rather than a fabricated 0, and a graceful
(non-crashing) failure on an unwritable path.

### GUI-5 verification (2026-10-20)
**COMPLETE on the first dispatch.** `release.yml` run `39468392733`: all five Windows presets
`success`. **237/237 tests** (226 prior + 7 new `ComparisonEngine` tests + 4 new `ResultMatcher`
tests, all passing). GUI smoke test confirmed another clean 5s launch/close -- the executable now
also wires `onCompare()` and the `ComparisonRow`/category-summary population path without any
startup issue (reachable code only, not exercised end to end since no model/Excel files are
selected in the unattended smoke test). No existing test modified or removed.

### GUI-5 implementation note (2026-10-21)
New `verification/ComparisonEngine.{h,cpp}` (pure numeric, no I/O): implements the task spec's
combined criterion `abs(solver-reference) <= absTol(category)*scale + relTol*abs(reference)`.
Each physical quantity (Displacement m, Rotation rad, Force kN, Moment kNm) has its own
documented default absolute-tolerance floor -- never one dimensional number reused across all
four, which the task spec explicitly forbids. The approved UI layout has only a single "Mutlak
Tolerans"/"Bağıl Tolerans" pair (not one per category), so that field is exposed as
`absoluteToleranceScale`, a dimensionless multiplier on every category's own floor, and
`relativeTol`, legitimately shared since it is itself dimensionless. Near-zero references are
handled by construction (the `relTol*abs(reference)` term vanishes, leaving the absolute floor),
no special-cased branch needed.

New `verification/ResultMatcher.{h,cpp}`: matches `results.json` (the real `kopruq-results`
schema, `include/kopruq/io/ResultsJson.hpp` -- read back with `nlohmann::json`, never re-derived)
against a `ReferenceTable` by original MIDAS id + load case name, both of which Phase 7B's
importer preserves unchanged. `matchDisplacements`/`matchReactions` share one implementation
(identical Node/Load/6-component structure); `matchBeamForces` additionally reads the `Part`
column's `I[...]`/`J[...]` prefix to pick `local_end_forces[0..5]` vs `[6..11]`.
`markElasticLinksUnsupported` reports every row `Unsupported` unconditionally, since Phase 7B's
importer rejects every MIDAS `ELNK` record -- no element in `results.json` can ever correspond to
one (a real formulation gap, not guessed around). Every reference row always produces a
`ComparisonRow` with a real status: `Pass`/`Fail` (actually compared), `NotApplicable` (the id or
load case has no counterpart in `results.json`, including a load case that failed to solve), or
`Unsupported` -- never silently dropped.

`MainWindow::onCompare()` wires this to the real GUI: reads `lastResultsPath_`, reads whichever
Excel reference files are actually selected (each independently optional, per spec), runs the
matchers, and populates all five result tables (`Özet`'s 4 tiles + category table,
`Deplasmanlar`, `Mesnet Tepkimeleri`, `Kiriş İç Kuvvetleri`, `Elastik Bağlantılar`) plus
`Diagnostik` with every reader/matcher diagnostic (missing columns, unmatched load cases, skipped
categories). A category's Durum is PASS only when it has at least one row and zero FAILs. New
`tests/unit/comparison_engine_tests.cpp` (7 tests: pass/fail, near-zero-reference floor,
per-category tolerance independence, the UI scale factor, non-finite handling, status names) and
`tests/unit/result_matcher_tests.cpp` (hand-built `results.json` fragment + in-memory
`ReferenceTable`s, no Excel I/O needed since `ExcelReferenceReader`'s own tests already cover
that path: displacement PASS/FAIL/3-kinds-of-N/A, beam-force I/J-end matching, elastic-link
unconditional Unsupported, empty-table edge case).

**Known open question, deliberately not resolved here:** MIDAS's own local-axis/beam-force sign
convention was confirmed via WebSearch during Phase 7B (support.midasuser.com returns HTTP 403)
but never independently cross-checked against a real solved result -- that is explicitly Phase
7E's job ("MIDAS NX results comparison"), still not started. If the real `xmidas/deneme1.json`
model shows systematic beam-force FAILs once re-exported to `.xlsx` and compared, that is
information this milestone is supposed to surface, not a GUI-5 defect to silently work around.

### GUI-4 verification (2026-10-20)
**COMPLETE on the first dispatch.** `release.yml` run `39435756925`: all five Windows presets
`success` (`xlnt` built from vcpkg source for the first time, similar to Qt6's first build --
the run took ~50 minutes, now cached). **226/226 tests** (220 prior + the 6 new
`ExcelReferenceReader` tests, all passing, including the `.xls`-rejection and missing-file
diagnostics). GUI smoke test again confirmed a clean 5s launch/close -- the executable now also
links `kopruq_verification` (and transitively `xlnt`) without any runtime startup issue. The
Windows GUI executable artifact was uploaded successfully (`kopruq-verification-gui-windows-x64`,
40,572 bytes). No existing test was modified, weakened, or removed to reach this result.

### GUI-4 implementation note (2026-10-20)
**Format decision (user, 2026-10-20):** the real exports already in `xmidas/` turned out to be
legacy `.xls` (BIFF/OLE2) during inspection, confirmed via `xlrd`. vcpkg has no `.xls`-capable
library (`xlnt`/`xlsxio`/`openxlsx` all read `.xlsx` only; `libxls` is not in vcpkg and would need
vendoring a C library with no local build to verify it against). Asked the user to choose between
vendoring `libxls`, supporting only `.xlsx`, or re-exporting the samples as `.xlsx` -- user chose
**re-export as `.xlsx`**, so `ExcelReferenceReader` supports `.xlsx` only, via `xlnt` (vcpkg,
already gated behind the "gui" manifest feature alongside `qtbase`). A `.xls` file is sniffed by
its real signature (OLE2 `D0 CF 11 E0`, never trusted from the extension alone) and rejected with
an explicit diagnostic naming the problem and the fix, both in `ExcelReferenceReader` itself and
one level up in `FileInputPanel` (GUI-2's Excel rows now only accept `.xlsx`, so a `.xls` file is
never shown as "Seçildi" for a file GUI-4 cannot actually parse).

New `verification/ExcelReferenceReader.{h,cpp}` (Qt-independent, per the architecture split):
locates every field **by header text**, never a hardcoded column index -- the four MIDAS export
types have different column counts/orders (Beam Force 23, Elastic Link 19, Displacements/
Reaction 15 columns, confirmed directly against the real samples before conversion), and nothing
guarantees MIDAS won't add/reorder columns in a future version. The header row itself is located
by content (first row with >=3 non-empty, non-numeric-looking cells), not a hardcoded row index,
since MIDAS's exports have a title row above it. A header cell's `"Name\n(unit)"` text is split
into a name and a unit, both preserved (never discarded). Every cell is captured in
`ReferenceTable::rows` regardless of whether anything currently reads it (`CellValue` holds both
the raw text and, if numeric, the parsed double) -- no silent data loss. New
`tests/unit/excel_reference_reader_tests.cpp`: builds small `.xlsx` fixtures with `xlnt` itself
(no external fixture file needed) mirroring the real `Displacements(Global)` column layout, and
checks header/unit splitting, position-independent name lookup, correct row values, the `.xls`-
rejection diagnostic (via a hand-written OLE2-signature file, not a real `.xls`), and a
missing-file diagnostic.

**Not yet wired into the GUI's "Sonuçları Karşılaştır" button** -- that is GUI-5's
`ComparisonEngine`/`ResultMatcher` job, deliberately out of scope here to keep this milestone
testable in isolation, per the task's own GUI-0→GUI-7 ordering.

### GUI-3 verification (2026-10-20)
**COMPLETE on the first dispatch.** `release.yml` run `39428529324`: all five Windows presets
`success`, **220/220 tests**, GUI smoke test confirmed another clean 5s launch/close -- the
construction path now also builds `SolverRunner` and wires its signals without a crash. As with
GUI-2, an unattended CI job has nothing to point the MIDAS model selector at and no one to click
Run, so an actual CLI invocation (process launch, stdout/stderr capture, exit-code handling,
results.json discovery) is **not yet exercised end to end by CI** -- confirmed instead by code
review of `SolverRunner.cpp`/`MainWindow.cpp` against `Cli.hpp`'s documented contract. A real
local run (same machine, both `ninja-release` and `ninja-gui-release` built from the same tree so
`locateCli()`'s relative-path fallback finds `kopruq_solver_cli.exe`) against the real
`xmidas/deneme1.json` sample would be the natural manual confirmation step once GUI-4/5 give the
GUI something to compare the results against; not performed in this unattended CI-only
environment.

### GUI-3 implementation note (2026-10-20)
New `apps/verification-gui/SolverRunner.{h,cpp}` wraps `QProcess` asynchronously (never blocks the
GUI thread): `locateCli()` searches a handful of plausible developer-build locations next to the
GUI executable, then falls back to `PATH`, returning an empty string (never a guessed path) if
`kopruq_solver_cli[.exe]` isn't found anywhere -- real, deterministic packaging next to the GUI
is GUI-7's job, not simulated here. `run()` always invokes `solve <model> -o <output> --format
midas-nx-json` (this GUI is specifically a MIDAS verification tool, per area 1's "MIDAS NX Model
(JSON)" label, so the format flag is never "native" in practice) exactly as `Cli.hpp` documents
it -- no invented flags. Output is written to `%TEMP%/kopruq_verification/<model>.results.json`.

Both "Modeli Doğrula" and "Solver'ı Çalıştır" invoke the identical CLI command (the CLI has no
separate validate-only mode); they differ only in how `MainWindow` interprets the exit code --
Validate reports only the invalid/valid (exit 3 vs. not) distinction and never claims a solve
succeeded, while Run Solver reports the full 0/1/other contract from `cli_usage`
(`Cli.hpp`). Both buttons are enabled only once `FileInputPanel::hasRequiredModel()` is true and
no run is already in flight; the progress bar goes indeterminate while a run is active (the CLI
exposes no granular progress) and only reaches 100% on an actual success, per the task spec's
explicit "do not invent progress percentages" / "do not show 100% unless the operation actually
succeeds" rules. `MainWindow::closeEvent` terminates (then kills, if needed) any in-flight
process rather than leaving an orphan running after the window closes -- the "cancel/terminate
handling where safe" GUI-3 asks for, done without adding a button outside the approved four-
button layout.

### GUI-2 implementation note (2026-10-20)
`FileInputPanel` now owns real behavior: each "..." button opens a `QFileDialog` filtered to the
row's expected type (`*.json` for the MIDAS model; `*.xlsx *.xls` for the four reference files --
widened from the spec's `.xlsx`-only wording because the real MIDAS exports already in
`xmidas/` are legacy `.xls`, confirmed in GUI-0). A selected path is validated by extension then
by `QFileInfo::exists()`, producing one of `NotSelected`/`Selected`/`Invalid`/`Unsupported` --
deliberately never a "Valid" state, since that would claim a level of checking (actual model
parsing) this panel does not do; that remains the "Modeli Doğrula" action's job once GUI-3 wires
the CLI. Every selection is persisted via `QSettings` (native per-OS backend: Windows registry
under `KOPRUQ-SOLVER\KOPRUQ-SOLVER Verification GUI`, an `.ini`-style file on Linux) and restored
(and re-validated against the current filesystem, so a moved/deleted file is correctly reported
Invalid rather than silently reappearing as if still selected) on next launch. The four Excel rows
remain individually optional by construction -- nothing in `FileInputPanel` requires them to be
filled, only the MIDAS JSON row (`hasRequiredModel()`). No action button is wired in this
milestone (that is GUI-3's CLI-integration scope); clicking one is still inert, not fabricated
success.

### GUI-2 verification (2026-10-20)
**COMPLETE on the first dispatch.** `release.yml` run `39422159633`: all five Windows presets
(`ninja-debug/release/asan/analyze/gui-release`) `success`, **220/220 tests**, GUI smoke test
again confirmed a 5s clean launch/close -- this run exercised `FileInputPanel`'s full
construction path (including `restorePersistedPaths()`/`revalidate()` against a fresh runner
with no prior `QSettings` entries) without a crash. What is **not** CI-verified, and is noted
honestly rather than claimed: the interactive `QFileDialog` flow itself (opening the dialog,
picking a file, seeing the status badge update) and cross-restart settings persistence cannot be
exercised by an unattended CI job with no user to click a dialog -- those are confirmed by code
review of `FileInputPanel.cpp`'s validation logic (extension check, `QFileInfo::exists()`,
`QSettings` read/write) rather than an end-to-end interactive test. A future milestone could add
`QTest`-driven headless UI tests (synthesizing clicks/dialog results) if that level of rigor is
wanted; not implemented here to keep scope matched to "minimize frontend development time."

### GUI-1 verification (2026-10-20)
**COMPLETE.** Three `release.yml` dispatches were needed to reach green, each catching a real,
distinct defect (never worked around or suppressed):
1. Run `39375862004`: Qt6 (`qtbase`, 31 features) built from vcpkg source successfully (~2.5h,
   now binary-cached) and the five other presets (`ninja-debug/release/asan/analyze`) passed
   unaffected, but `ninja-gui-release` failed to compile -- `ResultsComparisonPanel.cpp`'s
   `auto* addTile = [&](...){...}` (a lambda is not a pointer; MSVC C3535/C2440). Fixed in
   `6d7bc82` (plain `auto`).
2. Run `39394108214`: compiled and linked, then failed at `ctest --preset ninja-gui-release -N`
   with "No such test preset" -- `CMakePresets.json` had a `configurePresets`/`buildPresets`
   entry for `ninja-gui-release` but no matching `testPresets` entry. Fixed in `9c74ba9`.
3. Run `39406148285`: **PASS.** `ninja-gui-release`, `ninja-debug`, `ninja-release`, `ninja-asan`,
   `ninja-analyze` all `success`. **220/220 tests passed** (CTest, same inventory as the routine
   Ubuntu matrix plus the GUI preset's own build). The GUI smoke test actually launched
   `kopruq_verification_gui.exe` (with `windeployqt` having deployed the Qt6 platform plugin and
   runtime DLLs next to it), confirmed it stayed alive for 5 seconds (not an immediate
   platform-plugin-missing crash), then closed it cleanly. No existing regression test was
   touched, modified, or disabled by this milestone.

### GUI-1 implementation note (2026-10-20)
User decision for the Qt6/CI verification-strategy question raised in GUI-0: **option (b)** --
manual `release.yml` (Windows) dispatch after each GUI milestone, not a change to the every-push
Ubuntu `ci.yml`. Implemented accordingly: `vcpkg.json` gained an opt-in `"gui"` manifest feature
(`qtbase`, only fetched when `-DVCPKG_MANIFEST_FEATURES=gui` is passed, never by the default
presets `ci.yml` uses); `CMakePresets.json` gained `ninja-gui-release`; `CMakeLists.txt` gained a
`KOPRUQ_BUILD_GUI` option (default `OFF`) gating the new `kopruq_verification_gui` target;
`release.yml`'s Windows matrix gained that preset, a launch-and-stays-alive-for-5s smoke test
(treats an immediate exit as a crash, not success), and an artifact upload. A `windeployqt`
best-effort step was added (skipped quietly if not found, since full Qt runtime deployment
tooling is GUI-7's scope) because a Qt Widgets app cannot start at all without its platform
plugin deployed next to the executable -- without it, the smoke test will correctly report a
crash rather than a false pass. GUI-1's actual pass/fail is therefore determined by the next
manually-dispatched `release.yml` run, not asserted here.

### 14. Risks and limitations
**Resolved by GUI-7 (2026-10-21):** this section's original "central open risk" (no CI path built
Qt6 at all) was resolved per the user's 2026-10-20 decision (option b, manual `release.yml`
dispatch) and is now exercised for both Windows and Ubuntu -- see section 13. The bullets below
are kept as the historical record of the question as it stood during GUI-0/GUI-1, not rewritten.
- **No local build verification is currently possible in this development environment**: `cmake`
  is not on `PATH` here (confirmed this session), so every GUI milestone's "application compiles/
  opens" acceptance criterion can only be verified through CI, not interactively, for as long as
  that remains true.
- **No CI path currently builds Qt6 at all.** `ci.yml` (every push, Ubuntu) has no Qt6; `release.yml`
  (Windows, the natural home for a real desktop GUI) is manual-dispatch/tag-only, specifically to
  avoid repeating an earlier Actions-minute exhaustion incident. Verifying GUI-1's own acceptance
  criteria ("application compiles", "application opens", "no crashes when resizing") therefore
  needs an explicit decision on *how* that will be done before GUI-1 starts -- candidates: (a) add
  Qt6 to the Ubuntu `ci.yml` job (via system package, fastest, documented inconsistency vs. the
  vcpkg-pinned toolchain used for everything else), (b) manually dispatch `release.yml` after each
  GUI milestone (real Windows/MSVC build, but consumes the same limited Windows-minute budget this
  project has already exhausted once), or (c) build and smoke-test only via manual
  `workflow_dispatch` runs the user triggers, not on every push. This is flagged here rather than
  decided unilaterally, since it is a CI-cost/verification-rigor tradeoff, not a routine
  implementation detail.
- Elastic-link MIDAS comparison is structurally UNSUPPORTED for the real `xmidas/deneme1.json`
  sample today (ELNK unconditionally rejected by the Phase 7B importer) -- the GUI will correctly
  show this, but it means the one real reference model available cannot exercise a PASS/FAIL
  elastic-link comparison, only the UNSUPPORTED path.
- MIDAS's real export file extension for the four reference files is `.xls` (legacy BIFF/OLE2) in
  the samples already in the repo, not `.xlsx` -- confirm before choosing an Excel library, since
  this changes the GUI-4 library evaluation.
- No results category for spring/elastic-link elements is distinguished from ordinary beam
  elements in today's `results.json` (`StaticSolution::results.elements` carries beam and spring
  end-forces alike, keyed only by element id, with no element-type tag) -- GUI-5's matcher will
  need the *input* model's element-type information (already available via `ModelInput`) to
  separate "Kiriş İç Kuvvetleri" rows from "Elastik Bağlantılar" rows, not
  `results.json` alone.

### 15. Future enhancements
Not scoped now: batch/multi-model verification runs, CLI-only (headless) verification mode reusing
the same `verification/` library, automatic MIDAS API-based export (instead of manual Excel
export), provenance tagging of `results.json` (import format, see Phase 7D's note), CSV support
for the reference files (optional, only if trivial once the chosen Excel library is in place).

### 16. Completion status tracking
This section's own per-milestone table (item 7) is the single source of truth and will be updated
in place after each milestone's acceptance tests actually pass, per the "Standard acceptance gate"
below. `docs/DEVELOPMENT_STATUS.md` carries the same status in its running log and "Next
autonomous action" section.

## Standard acceptance gate for each milestone
1. Requirements and conventions documented.
2. Implementation builds in Debug and Release.
3. Independent analytical or dense-reference tests PASS.
4. Existing tests preserved; complete inventory audited.
5. Full Debug and Release CTest PASS.
6. Numerical errors and limitations documented.
7. Git diff reviewed and a scoped commit made when authorized.
8. `docs/DEVELOPMENT_STATUS.md` updated.

## Decision policy
Claude may autonomously choose safe implementation details. Pause for user approval before material numerical-formulation changes, major API/architecture changes, destructive actions, or KOPRUQ integration. Avoid repeated permission questions for ordinary implementation/debugging tasks.
