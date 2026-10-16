# KOPRUQ-SOLVER — Claude Code Project Instructions

## Mission
Develop and independently verify a production-grade, standalone, high-performance, 3D structural finite-element solver for bridge engineering. The solver will ultimately integrate with KOPRUQ, but integration is not authorized yet.

## Project boundaries
- Authorized workspace: `C:\_ZHarput_Data\KOPRUQ-SOLVER`.
- **Never modify** `C:\_ZHarput_Data\KOPRUQ` unless the user explicitly approves integration.
- C++20, CMake, MSVC/Ninja, Eigen, optional SuiteSparse, GoogleTest; Windows PowerShell scripts.
- Internal solver only: no MIDAS, OpenSees, or third-party analysis engines.
- Units: m, kN, kNm, kN/m², radians. Six DOFs per node: UX, UY, UZ, RX, RY, RZ.
- Treat external packages as numerical libraries, not structural solvers.

## Required startup procedure — every session
1. Read this file, `docs/SOLVER_ROADMAP.md`, `docs/DEVELOPMENT_STATUS.md`, and `docs/REQUALIFICATION_STATUS.md` if present.
2. Inspect Git status, current HEAD, CMake targets, and existing tests; do not presume the status document is current.
3. Identify the first unfinished, unblocked task in the roadmap; formulate a small implementation plan and execute it.
4. Do not ask what to do next when the roadmap provides an unambiguous next step.
5. Update the status document with observed evidence after each completed or blocked milestone.

## Autonomous execution rules
- You may inspect and edit files **inside the authorized solver workspace**, create tests and documentation, configure, compile, run tests, diagnose build failures, and fix local defects without repeatedly seeking permission.
- Choose implementation details consistent with the existing architecture and documented acceptance criteria. If a task is large, divide it into testable increments.
- If compilation or tests fail: reproduce; preserve logs; identify the root cause; make the smallest justified correction; rerun focused and full tests. Continue within the same milestone until passing or genuinely blocked.
- If a tool/environment issue prevents progress, try safe non-destructive diagnostics and a separate clean build directory; do not confuse build failures with numerical verification failures.
- If one task is blocked but another independent roadmap task is safe, document the blocker and proceed with that task.
- Do not silently change engineering assumptions, sign conventions, units, coordinate conventions, or public APIs.
- Do not silently expand the scope to later phases.

## Ask the user only when
- A decision changes the solver's numerical formulation, intended supported physics, or major architecture and the roadmap does not resolve it.
- An action would delete/overwrite valuable data, rewrite Git history, alter the main KOPRUQ repository, or expose private material.
- External credentials, licensing, procurement, or security settings require user action.
- Two materially different engineering requirements remain ambiguous after inspecting project documentation.
Otherwise decide, implement, verify, and report. **Never bypass approval or security controls to reduce questions.**

## Non-negotiable engineering quality gates
- Independently derive analytical or dense-reference expected results; do not calculate expected results using the production implementation under test.
- Verify units, force/moment signs, element local/global axes, matrix symmetry, rigid-body modes, equilibrium, and energy as relevant.
- Preserve every existing regression test. Never overwrite or truncate a test file to add new tests; use dedicated new test files. Compare test inventory before and after changes.
- Historical loss of 25 tests is not recoverable from available evidence. New tests are **requalification**, not historical restoration. Never claim otherwise.
- Run focused tests and full Debug and Release CTest after substantive changes. Report actual counts and numerical tolerances.
- Never claim PASS when tests could not run. Do not weaken assertions to force PASS.
- Avoid Eigen expression-template lifetime bugs; materialize intermediate matrices when necessary.
- Prefer sparse algorithms for production, dense independently constructed references for small verification cases.
- Keep deterministic node/element/DOF ordering and stable results.

## Git and reporting
- Before edits: inspect `git status` and record the baseline. Preserve unrelated user changes.
- Before commits: inspect `git diff`, changed files, test inventory, and secrets/binary artifacts. Never use `git reset --hard`, destructive clean commands, or force push without explicit approval.
- Commit only verified, scoped changes, using descriptive messages. Do not push automatically unless the user has authorized that workflow.
- At the end of each milestone, report: status (COMPLETE/INCOMPLETE), modified files, numerical evidence, Debug/Release test counts, outstanding risks, and next roadmap task.
- Keep `docs/DEVELOPMENT_STATUS.md` synchronized with actual evidence; distinguish reported historical results from results verified in the current session.

## Continuous work policy
Work through the roadmap in sequence, using milestone acceptance gates. Do not stop after every successful micro-task to ask for the next instruction. Stop at explicit approval gates, unresolved engineering decisions, or genuine external blockers. Do not promise unattended or background execution; continue only while the current Claude Code session is running and permitted.
