# MIDAS Civil NX JSON Export — Data Audit (Phase 7A)

Source: `C:\_ZHarput_Data\KOPRUQ-SOLVER\xmidas\deneme1.json` (48,976 bytes), inspected directly
(PowerShell `ConvertFrom-Json`) on 2026-10-18. Not modified. Sibling files in the same directory
(`deneme1.bak`, `deneme1.mcb`, `deneme1_*.log/.mec/.nfxp/.out/.rst`) are native MIDAS
binary/text working files and run logs, not JSON; they are catalogued in §6 as potential future
reference data, not parsed as model input.

## 1. Documentation access

Both official MIDAS pages named in the task returned **HTTP 403 Forbidden** (authenticated
support portal; WebFetch cannot bypass that):
- `support.midasuser.com/hc/ko/articles/33016922742937-MIDAS-API-Online-Manual`
- `support.midasuser.com/hc/en-us/articles/35994422273305-Export-to-Json`

Per the task's own fallback instruction, these are **not used**; every field meaning below is
derived from the actual exported data plus one corroborating public page found by search
(`support.midasuser.com` "Modify Concrete Materials": design-code concrete modulus is computed
from grade via `Ec = 9.5*(fck+8)^(1/3) * 10^3` for the `EN(RC)` family — consistent with what §3.2
shows is *missing* from the export). Field names not independently confirmed are marked
**(unverified)** below; none were guessed beyond what the data itself demonstrates.

## 2. File identity and top-level structure

Single root key: `"DBG"` (the whole file is one object `{"DBG": {...}}`). Under `DBG`, **over 150**
second-level keys exist — this is a **full MIDAS Civil NX database dump** (every record category
the application knows about, most empty for this model), not a minimal structural-analysis
subset. No MIDAS version/build metadata field was found at the top level.

## 3. Populated categories (non-zero entry count)

| Key | Count | Meaning (from data + task's category list) |
|---|---|---|
| `UNIT` | 1 | Units: `FORCE:"KN"`, `DIST:"M"`, `HEAT:"BTU"`, `TEMPER:"C"` |
| `STYP` | 1 | Structure type / mass source: `MASS:1` (from loads), `GRAV:9.806`, self-weight flag on |
| `PJCF` | 1 | Project config; empty object `{}` in this export |
| `MDGN` | 1 | Design-code defaults (concrete/steel code names), not analysis input |
| `NODE` | 106 | Node coordinates (X,Y,Z); IDs are sparse, up to 109, not 1..106 contiguous |
| `ELEM` | 94 | Elements — **all 94 are `TYPE:"BEAM"`** (no truss/plate/solid in this model) |
| `MATL` | 2 | Materials — both `TYPE:"CONC"`, identified by **grade name** (`C35/45`, `C45/55`) |
| `MATD` | 2 | Material design data — characteristic strength only (see §3.2, gap) |
| `SECT` | 5 | Sections — `SECTTYPE:"DBUSER"`, shape code + raw dimensions (see §3.3, gap) |
| `CONS` | 9 | Point restraints — all `"1111111"` (fully fixed; see §3.4 on the 7th character) |
| `ELNK` | 4 | Elastic links (`LINK:"GEN"`) between **non-coincident** node pairs (see §3.5, gap) |
| `RIGD` | 5 | Rigid links — one master node key, `S_NODE` list of slaves (maps directly onto our `RigidLink`, Phase 5.5) |
| `STLD` | 5 | Load cases: `SW` (D), `SDL-1`/`SDL-2`/`SDL-3` (DC), `TU` (T, temperature) |
| `BODF` | 1 | Self-weight: `FV:[0,0,-1]` (global −Z), case `SW` |
| `BMLD` | 40 (elements) | Uniform member loads (`TYPE:"UNILOAD"`, `DIRECTION:"GZ"`), several per element across `SDL-1/2/3` |
| `ETMP` | 40 (elements) | Element temperature loads, case `TU` — **not currently supported** (see §3.6) |
| `EIGV` | 1 | Modal control: Lanczos, 20 modes |
| `SPFC` | 1 | Response-spectrum function `"EURO2004 H-ELASTIC"`, full period/Sa table (Eurocode 8 elastic spectrum) |
| `SPLC` | 2 | Response-spectrum cases `SPX`/`SPY` (0°/90°), CQC combination |
| `LTOM` | 1 | Load-to-mass control: self-weight (`SW`, factor 1.0) converted to mass for dynamics |
| `CO_S`, `CO_M` | 5, 2 | **Display colors only** (RGB fields), not load combinations |

Categories named in the task's list that are **absent (0 entries)** in this export: `SKEW`
(no node local axes), `NSPR`/`GSPR`/`GSBG`/`GSTP` (no point/general springs), `FRLS` (no beam end
releases), `OFFS` (no beam end offsets), `CNLD` (**no nodal loads**), `NMAS` (no added mass),
`LCOM-*` (no load combinations defined, only load cases).

## 4. Supported vs. unsupported, with concrete gaps

### 4.1 Element type — supported
94/94 elements are `BEAM` (`MATL`, `SECT`, `NODE:[i,j,...]`, `ANGLE` beta, `STYPE`). This maps onto
our `Beam3DConnection`. **Open item:** MIDAS's `ANGLE` is a beta rotation in degrees about the
member's own local x-axis from a default orientation; our `reference_y` is a vector. The
conversion (beta angle → equivalent `reference_y`, given MIDAS's default-orientation rule) needs
to be derived and independently verified in 7B — not guessed.

### 4.2 Material — **real gap**
`MATL`/`MATD` give `TYPE:"CONC"` and a grade name (e.g. `"C35/45"`) plus characteristic strength
(`C_FC:35000` kN/m² = 35 MPa, matching the grade) — **no elastic modulus, Poisson's ratio, or unit
weight/density value anywhere in the export.** MIDAS computes `E` internally from the grade via a
design-code formula (confirmed by the one accessible public MIDAS page: `Ec = 9.5*(fck+8)^(1/3)*
10^3` for the `EN(RC)` family). Our `Material` struct requires `elastic_modulus`, `poisson_ratio`,
`unit_weight` directly. **This cannot be silently defaulted** (CLAUDE.md / this task's explicit
rule). 7B needs either (a) a small, explicitly-cited code-formula table (Eurocode concrete E-fck
relationship, Poisson ≈ 0.2, unit weight ≈ 25 kN/m³ for normal concrete — each a cited engineering
assumption, not a guess) or (b) rejection of any model whose material lacks a directly-supplied
`elastic_modulus`, with a clear diagnostic naming the missing value.

### 4.3 Section — **real gap**
Every section is `SECTTYPE:"DBUSER"` with a `SHAPE` code and a raw dimension array `vSIZE`, e.g.
`SB` (`[1.0,12.0]`), `B` (`[2.0,5.0,0.5,0.5]`), `SR` (`[1.2]`) — **no computed area/Iy/Iz/J
anywhere** (`SECP`, `SECF` are both empty). Our `Section` struct requires `area, Iy, Iz, J`
directly. 7B needs a small section-shape property library (closed-form area/I/J formulas for
solid rectangle, box/hollow rectangle, solid circle — the three shapes actually used here), with
each formula's source cited and independently checked against hand calculation, not merely
transcribed from memory. Shape codes beyond these three are out of scope until encountered.

### 4.4 Supports — mostly supported, one open question
All 9 `CONS` entries are `"1111111"` — **seven** characters, not six. Our `Restraints` is 6 flags
(UX,UY,UZ,RX,RY,RZ). The extra character's meaning (always `1` in this sample, so it cannot be
distinguished from "ignored/reserved" vs. a real 7th DOF here) is an open item for 7B, to be
resolved from documentation or, since the docs are inaccessible, by testing against a model that
varies it — not assumed to be benign.

### 4.5 Elastic links (`ELNK`) — **real formulation gap, not just missing data**
4 `GEN` elastic links connect **non-coincident** node pairs (e.g. node 26 at `(0,2,-2.5)` to node
11 at `(0,2,-2.0)`, 0.5 m apart) with independent translational springs (`SDR`) and no rotational
stiffness. Our existing `SpringLink` element (Phase 5.4) **requires coincident nodes** — this was
a deliberately enforced, CI-verified formulation limit (a decoupled per-DOF spring has no
lever-arm coupling to node separation, so it is only rigid-body consistent when nodes coincide;
see `SOLVER_ROADMAP.md` 5.4). MIDAS's `GEN` link evidently supports non-coincident nodes;
whether MIDAS adds the lever-arm coupling our `SpringLink` deliberately does not, or accepts the
same approximation for small offsets, cannot be confirmed from the export or the (inaccessible)
docs. **This model cannot be faithfully imported with today's `SpringLink` as-is.** 7B must either
extend the element (adding the rigid-arm coupling, a real formulation change requiring its own
independent verification) or reject these 4 records with a clear diagnostic — not silently import
them onto coincident-node assumptions that don't hold here.

### 4.6 Thermal loads (`ETMP`, case `TU`) — **unsupported, not implemented at all**
40 element temperature records exist for load case `TU` (`STLD` id 5, `TYPE:"T"`). KOPRUQ-SOLVER
has no thermal/temperature load capability in any phase to date. Must be explicitly rejected
(clear diagnostic) if `TU` is requested for analysis, not silently dropped.

### 4.7 Rigid links (`RIGD`) — supported
5 records, each one master node key with a `S_NODE` slave list (e.g. master node 1 →
slaves `[11,13]`; master node 5 → slaves `[12,14]`) and `DOF:111111` (all 6 DOFs rigid). This maps
directly onto `kopruq::RigidLink` (Phase 5.5, just completed and verified) — **one of the few
categories with no gap**, modulo the ID-mapping/one-master-per-slave validation already built in.

### 4.8 Loads — mostly supported, one gap
- `BODF` self-weight (−Z): matches our existing self-weight mechanism.
- `BMLD` uniform member loads (`UNILOAD`, global Z): matches `MemberLoad` (global axes, uniform
  w1=w2). `D`/`P` 4-element arrays carry `[start,end,?,?]` / `[w1,w2,?,?]`; the 3rd/4th entries'
  meaning is unconfirmed (always 0.0 in this sample) — needs resolving in 7B, not assumed benign.
- `CNLD` (nodal loads): **absent** in this model, so not exercised by it, but is a currently-
  supported category on our side regardless.
- `ETMP` (temperature): unsupported per §4.6.

### 4.9 Dynamic/seismic analysis — partially supported, partially not
`EIGV` (Lanczos, 20 modes) and `SPLC`/`SPFC` (2 response-spectrum cases, Eurocode 8 elastic
spectrum with a full period/Sa table) map onto our already-complete Phase 6.1 (modal) and 6.2
(response spectrum) solvers. `LTOM` (self-weight converted to mass, factor 1.0) corresponds to how
our solver already derives mass from `Material::unit_weight`/self-weight — compatible in
principle, pending the material-E/density gap in §4.2 being resolved (mass needs density too).
**Rigid links are present in this same model (§4.7), and ModalSolver/ResponseSpectrumSolver
currently reject any model with `rigid_links` (Phase 5.5's explicit, documented limitation)** — so
this model's modal/spectrum cases cannot be run end-to-end until that restriction is lifted or
the rigid links are excluded for those cases specifically. Flagged for 7B/7C planning, not
resolved here.

### 4.10 Load combinations — none to import
`CO_S`/`CO_M` are display colors, not combinations; no `LCOM-*` category has entries. Nothing to
map; our `LoadCombination` mechanism is simply unused for this particular model.

## 5. Model classification (task §3, item 1–4)

**Partial structural model for a subset of our current capability, full model for capabilities we
don't have yet.** It parses as valid JSON and contains complete geometry/connectivity/supports/
most loads, but is **not sufficient for direct analysis without additional engineering input**:
material `E`/Poisson/density and section `A/I/J` are not present as numbers (§4.2, §4.3) and must
be derived via explicit, cited formulas or the model must be rejected for those members — this is
a "database export requiring additional records/computation", not a self-contained analysis input,
for exactly the two categories that matter most (material and section stiffness).

## 6. MIDAS's own result files (potential future reference data, not parsed now)

Sibling files in `xmidas/` are MIDAS's own run artifacts for this same model:
- `deneme1_Eigenvalue.out` (plain text): contains MIDAS's own modal frequencies for all 20 modes
  (e.g. Mode 1: 1.8050 Hz, Mode 2: 1.9072 Hz, ... Mode 20) — **directly usable as an independent
  reference for a future Phase 7E modal comparison**, once the material/section gaps are closed
  enough to run our own modal analysis on this model.
- `deneme1_ST_SW.rst`, `..._ST_SDL-1/2/3.rst`, `..._ST_TU.rst`, `..._Eigenvalue_Eigen.rst`,
  `..._Spectrum_SPX/SPY.mec/.nfxp`: binary MIDAS result formats. No displacement/reaction/force
  numbers were extractable from the plain-text `.out`/`.log` files for the static or spectrum
  cases (those `.out` files are just run logs: subcase names + "JOB SUCCESSFULLY COMPLETED" +
  timing, no numeric results printed). Extracting numeric MIDAS reference results for static and
  spectrum cases will need either the MIDAS API (if available with proper authentication) or a
  results re-export from MIDAS in a parseable format — not attempted in this audit.

## 7. Summary for Phase 7B planning

Must resolve, in this order of blocking severity, before any analysis of this real model is
possible:
1. Material `E`/Poisson/unit-weight derivation from grade (§4.2) — blocks everything.
2. Section shape→property formulas for `SB`/`B`/`SR` (§4.3) — blocks everything.
3. `ELNK` non-coincident elastic links (§4.5) — blocks the 4 elements/joints they touch; the rest
   of the model could proceed without them if explicitly excluded with a diagnostic.
4. Rigid links vs. ModalSolver/ResponseSpectrumSolver's current rejection (§4.9) — blocks dynamic
   analysis of this model specifically, not static analysis.
5. `ETMP`/`TU` thermal loads (§4.6) — blocks only the `TU` load case, not `SW`/`SDL-1/2/3`.
6. `ANGLE` (beta) → `reference_y` conversion (§4.1) and the `CONS` 7-character string (§4.4) and
   `BMLD`'s `D`/`P` 3rd/4th array entries (§4.8) — smaller, likely-resolvable-by-inspection open
   items, not launch blockers but must be resolved with evidence, not assumption, before claiming
   a faithful import.
