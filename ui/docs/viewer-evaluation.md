# 3D viewer evaluation: vtk.js vs Three.js (Phase 0, 2026-11-23)

Data: the real verification model 100 (94 beams, 106 nodes; native `kopruq-model` JSON written by the
existing importer) with its real `results.json` (case SDL-1 displacements), and the same model tiled
100x (9 400 elements, 10 600 nodes) for scale. Code: `ui/spikes/` (`vtk_bench.js`, `three_bench.js`,
shared `common.js`). Versions: current npm releases installed on 2026-11-23.

**Measurement limits (read before trusting the numbers):** headless Edge with software WebGL
(SwiftShader), no GPU; timer resolution 0.1 ms; each figure is a median of 10-20 runs on one machine.
Absolute values are therefore pessimistic and only the comparison between the two libraries and the
scaling with element count mean something. Frame time includes a 1-pixel `readPixels` to force the
GPU work to finish. Line elements only (no extruded sections yet).

| Metric | vtk.js 94 el. | Three.js 94 el. | vtk.js 9 400 el. | Three.js 9 400 el. |
|---|---|---|---|---|
| Scene build (ms) | 5 | 1 | 1.5 | 3 |
| Frame render (ms, median) | 2.5 | 2.5 | 4.7 | 4.0 |
| Deform + recolour + render (ms) | 3.3 | 2.4 | 5.6 | 5.6 |
| Pick: correct element | 39/40 | 40/40 | 1/40 (34/40 hit a line through the pixel) | 0/40 (40/40 hit a line through the pixel) |
| Pick time (ms) | 3.4 | <0.1 | 5.0 | 0.3 |

At 9 400 elements the tiled copies overlap on screen, so an exact element id is ambiguous; the
"line through the pixel" figure is the fair one there. Production bundle (gzip): vtk.js 231 kB
(Geometry profile), Three.js 132 kB.

Capabilities verified on the real data (screenshots `ui/spikes/vtk_view.png`, `three_view.png`): both draw
the frame, colour it by displacement magnitude, deform it by a scale factor (point rewrite, no rebuild)
and pick elements. Differences that matter:

- **Picking.** vtk.js `CellPicker` ignores line cells; picking needs the GPU hardware selector (works,
  ~3-5 ms, constant in model size, but returns only what is visible). Three.js raycasting is a CPU linear
  scan (0.3 ms at 9 400 elements, grows linearly, a spatial index or GPU id-pass would be needed beyond
  ~10^5 elements).
- **Scalar colouring.** vtk.js has colour transfer functions, scalar ranges and (to be added) scalar
  bars built in; Three.js needs our own colour mapping and legend (small).
- **Section solids.** Neither gives section extrusion for free; we generate the geometry ourselves in both
  (as already done in the Qt viewer).
- **Size/ecosystem.** vtk.js is ~75 % larger; it is the more natural fit for contours, cutting and later
  shell/solid results; Three.js is lighter and has the larger community.

**Recommendation:** vtk.js as the primary candidate, as already planned: equal measured speed, built-in
scalar mapping and a hardware picker whose cost does not grow with model size. Keep the viewer behind a
thin `FEMViewer` interface so Three.js remains a drop-in alternative if the size or GPU-selector
behaviour turns out to be a problem on real hardware. **Before Phase 3 is closed, repeat the timing on a
machine with a real GPU** (not possible in this headless run).

## Decision (2026-11-23)
vtk.js approved by the user as the 3D viewer library. Three.js stays only as a fallback behind the
`FEMViewer` interface. Still open: timing on a machine with a real GPU (to be repeated in Phase 3).
