# Example models

JSON models in the `kopruq-model` format, version 1 (`schemas/model.schema.json`).
Units are m, kN and rad.

| File | Content |
|---|---|
| `cantilever.json` | 4 m steel cantilever; self-weight, a global gravity line load and a tip load; one ULS combination |
| `portal_frame.json` | 6 m x 4 m concrete portal frame fixed at both column bases; self-weight, deck load, wind; two combinations |

Run:

```
kopruq_solver_cli solve examples/portal_frame.json -o results.json
```

The results follow `schemas/results.schema.json`.
