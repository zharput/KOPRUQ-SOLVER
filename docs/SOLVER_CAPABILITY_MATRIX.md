# Solver Capability vs. MIDAS NX Sample — Gap Matrix (Phase 7A)

Current solver capability per `docs/SOLVER_ROADMAP.md`/`docs/DEVELOPMENT_STATUS.md` (git history
is the source of truth over either document), compared against what `xmidas/deneme1.json`
actually requires (`docs/MIDAS_JSON_DATA_AUDIT.md`). "Needed by sample" marks only what this one
real model actually exercises; KOPRUQ-SOLVER's own scope is broader or narrower in places.

| Capability | Solver status | Needed by sample? | Gap |
|---|---|---|---|
| 3D beam element, stiffness/mass | COMPLETE (Phase 3.2R1, 6.1) | Yes (94 elements) | None |
| Euler-Bernoulli + Timoshenko shear | COMPLETE (Phase 3.2R1, 5.6) | Possibly (sections have `USE_SHEAR_DEFORM:true`) | Shear area not in export (see material/section gap); would need a shape-formula shear-area term too |
| Sparse assembly, DOF numbering, non-consecutive node IDs | COMPLETE (Phase 3.2R2) | Yes (IDs up to 109, 106 used) | None |
| Linear static solve, multi-case/combination | COMPLETE (Phase 3.2R3, 4.1) | Yes (5 cases, SW self-weight factor LTOM-equivalent) | None for mechanics; blocked on material/section data |
| Reaction/element-force recovery, equilibrium/energy checks | COMPLETE (Phase 3.2R4) | Yes | None |
| Member distributed loads (global axes, uniform) | COMPLETE (Phase 4.1) | Yes (`BMLD` UNILOAD, GZ) | `D`/`P` array 3rd/4th entries unconfirmed |
| Self-weight | COMPLETE (Phase 4.1) | Yes (`BODF`, LTOM) | Needs `Material::unit_weight`, itself a gap (§4.2 of audit) |
| JSON model I/O, CLI | COMPLETE (Phase 4.2) | Yes (import path itself) | A MIDAS-format adapter does not exist yet (Phase 7B) |
| Concentrated member forces/moments | COMPLETE (Phase 4.3) | No (`CNLD` empty, no beam point loads seen) | None |
| Validated C++ API | COMPLETE (Phase 4.4) | Yes (used by any importer) | None |
| Beam end releases | COMPLETE (Phase 5.1) | No (`FRLS` empty) | None |
| Truss elements | COMPLETE (Phase 5.2) | No (all 94 elements are `BEAM`, none are truss-like) | None |
| Rigid end offsets (eccentric, two-node) | COMPLETE (Phase 5.3) | No (`OFFS` empty) | None |
| Spring/link elements (bearings, isolators) | COMPLETE (Phase 5.4) — **coincident nodes only** | Yes, but **sample violates the constraint** | `ELNK` connects non-coincident nodes (audit §4.5) — real gap, not just missing data |
| General rigid links / multi-point constraints | COMPLETE (Phase 5.5) | Yes (`RIGD`, 5 records) | None for the static case; **ModalSolver/ResponseSpectrumSolver explicitly reject models with rigid links** — blocks this sample's dynamic cases specifically |
| Timoshenko beam (shear area input) | COMPLETE (Phase 5.6) | Possibly | Shear area value itself not derivable from export without a section-shape library |
| Modal (free-vibration) analysis | COMPLETE (Phase 6.1) | Yes (`EIGV`, Lanczos/20 modes) | Blocked by rigid-links rejection (above) and by material/section gaps |
| Response spectrum analysis | COMPLETE (Phase 6.2) | Yes (`SPLC`/`SPFC`, SPX/SPY, CQC) | Same blockers, plus: solver takes a caller-supplied spectrum (period, Sa) array — `SPFC`'s `aFUNC` table maps onto this directly, no gap there |
| Material: direct E/Poisson/unit-weight input | COMPLETE | Yes, but **sample never supplies these** | Grade-name-only material (`C35/45` etc.); needs code-formula derivation or explicit rejection (audit §4.2) |
| Section: direct area/Iy/Iz/J input | COMPLETE | Yes, but **sample never supplies these** | Shape-code + raw dimensions only (`SB`,`B`,`SR`); needs a section-property formula library (audit §4.3) |
| Thermal/temperature loads | **NOT IMPLEMENTED, any phase** | Yes (`TU` case, 40 `ETMP` records) | New capability, not yet scoped anywhere in the roadmap |
| Load combinations | COMPLETE (`LoadCombination`) | No (`LCOM-*` empty in sample) | None |
| MIDAS NX JSON import adapter | **does not exist** | Yes — this is the whole point of Phase 7B | To be built |
| Section-shape beta-angle (`ANGLE`) → `reference_y` | N/A (different conventions) | Yes (all 94 elements have an `ANGLE`) | Conversion rule needs deriving/verifying (audit §4.1) |

## Reading this matrix

Of the ~20 structural capabilities MIDAS exercises in this one sample, KOPRUQ-SOLVER's existing,
already-verified element/load/analysis machinery covers the *mechanics* of nearly all of them.
The real blocking gaps are not in the solver's structural formulations — they are in **translating
MIDAS's design-oriented input (grade names, shape codes) into the numeric engineering properties
our solver already knows how to use**, plus two smaller items: non-coincident elastic links
(§4.5) and the rigid-links-vs-dynamics restriction (§4.9 of the audit), plus thermal loads being
entirely out of scope today. None of these require touching previously-verified static stiffness/
mass/load/recovery code; they are new, additive work (a MIDAS adapter, a section-property
library, a material-grade table, and either a `SpringLink` generalization or explicit rejection of
non-coincident links) — per `CLAUDE.md`, routine implementation, not a numerical-formulation
change to existing elements.
