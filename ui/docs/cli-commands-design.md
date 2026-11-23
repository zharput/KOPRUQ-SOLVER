# New CLI commands: technical design (approved in principle 2026-11-23)

Constraint: the analysis core, the existing JSON schemas and the existing commands (`solve`, `modal`,
`spectrum`, flags, exit codes, output bytes) stay unchanged. Everything below is additive: new commands,
a new opt-in flag, new output schemas with their own names and versions. Implementation lives in the CLI
front end (`include/kopruq/io/Cli.hpp`, `apps/`), not in solvers or assembly.

## 1. `inspect` -- model conversion for the viewer
`kopruq_solver_cli inspect <model> [--format native|midas-nx-json] [-o view.json]`
Output schema `kopruq-model-view` v1 (new file `schemas/model_view.schema.json`):
`model` (the existing `kopruq-model` object, via the existing writer), `section_outlines`
(by section id: polygons, centroid at origin; the data the Qt viewer already uses), `import`
(`assumptions[]`, `rejected[]`, counts, bounding box -- today only free text on stderr), `restraints_local_axes`
is already inside `model.node_axes`. Exit codes as `solve` (3 invalid model, 4 I/O). Needs no solver run.

## 2. `verify` -- MIDAS NX comparison (logic stays in C++)
`kopruq_verify_cli compare --model <midas.json> --results <results.json> [--spectrum <s.json>]
 [--disp f.xlsx] [--reaction f.xlsx] [--reaction-local f.xlsx] [--beam f.xlsx] [--truss f.xlsx]
 [--link f.xlsx] [--eigen f.xlsx --modal <m.json>] [--abs-scale x] [--rel-tol x] -o verification.json`
Wraps the existing `kopruq_verification` library (ResultMatcher, ExcelReferenceReader,
ComparisonEngine) unchanged. Output schema `kopruq-verification` v1: per category `rows[]`
(load_case, id, location, component, unit, midas, solver, signed_diff, abs_diff, rel_diff, status =
PASS|FAIL|N/A|UNSUPPORTED), `summary` (the same CategoryStats the GUI shows), `diagnostics[]`. A separate
executable because it links xlnt (Excel); the solver CLI stays dependency-light. `report` sub-command
re-uses the existing xlsx report exporter.

## 3. Progress reporting
Opt-in flag `--progress json` on `solve|modal|spectrum`: newline-delimited JSON events on **stderr**
(`{"event":"stage","name":"import|validate|assemble|solve|recover|write","state":"start|done","t":sec}`,
`{"event":"case","name":...,"index":i,"of":n}`), final line `{"event":"done","exit":code}`. Without the
flag nothing changes (byte-identical stdout/stderr). Cancel = the host terminates the process.

## Open points for approval
- Where `kopruq_verify_cli` is built (needs xlnt: a new CMake option, default off like the GUI).
- UTF-8 paths already handled (`io::utf8Path`); the new commands must use it.
- Tests: each command gets golden-output tests on the existing models (100, 23, 24); the existing
  C++ and MIDAS verification suites must stay green.
