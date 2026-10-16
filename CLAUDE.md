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
4. Do not ask what to do next when the roadmap provides an unambiguous next step, **except that every work period still ends at the mandatory approval checkpoint defined below** — identifying the next task does not authorize starting it without that checkpoint.
5. Update the status document with observed evidence after each completed or blocked milestone.
6. Do not repeat previously completed and verified tasks unnecessarily; treat the repository and Git history as the source of truth over any status document's claims.

## Autonomous execution rules
Within each approved work period (i.e. between one mandatory approval checkpoint and the next), you are authorized to operate autonomously:
- Identify the next incomplete roadmap task, inspect relevant source files, plan the implementation, write and modify code, compile, run focused and full regression tests, investigate and correct compilation errors and numerical discrepancies, improve performance without sacrificing correctness, and update technical documentation — all without repeatedly seeking permission for routine steps.
- Choose implementation details consistent with the existing architecture and documented acceptance criteria. If a task is large, divide it into testable increments.
- If compilation or tests fail: reproduce; preserve logs; identify the root cause; make the smallest justified correction; rerun focused and full tests. Continue within the same milestone until passing or genuinely blocked.
- If a tool/environment issue prevents progress, try safe non-destructive diagnostics and a separate clean build directory; do not confuse build failures with numerical verification failures. If retries begin making things worse (e.g. previously-good artifacts becoming corrupted) rather than converging, stop retrying and report it as a blocker instead of continuing to retry into a worsening state.
- If one task is blocked but another independent roadmap task is safe, document the blocker and proceed with that task.
- Do not silently change engineering assumptions, sign conventions, units, coordinate conventions, or public APIs.
- Do not silently expand the scope to later phases.
- Do not ask routine questions such as "what should I do next?", "should I continue coding?", "should I run the tests?", "should I fix this error?", or "which roadmap task should I select?" — decide independently within the approved scope and current work period.

## Ask the user only when
- A decision changes the solver's numerical formulation, intended supported physics, or major architecture and the roadmap does not resolve it.
- An action would delete/overwrite valuable data, rewrite Git history, alter the main KOPRUQ repository, or expose private material.
- External credentials, licensing, procurement, or security settings require user action.
- Two materially different engineering requirements remain ambiguous after inspecting project documentation.
- A major blocker prevents safe progress (see Mandatory approval checkpoint below): issue a progress report immediately and wait for approval rather than continuing to push through it.
Otherwise decide, implement, verify, and report. **Never bypass approval or security controls to reduce questions.**

## Non-negotiable engineering quality gates
- Independently derive analytical or dense-reference expected results; do not calculate expected results using the production implementation under test.
- Verify stiffness matrices, coordinate transformations, DOF numbering, sparse matrix assembly, boundary conditions, displacements, support reactions, element end forces, force/moment equilibrium, energy consistency, and numerical convergence/stability, as relevant to the milestone.
- Preserve every existing regression test. Never overwrite or truncate a test file to add new tests; use dedicated new test files. Never delete, disable, or overwrite existing tests to hide failures. Compare test inventory before and after changes.
- Historical loss of 25 tests is not recoverable from available evidence. New tests are **requalification**, not historical restoration. Never claim otherwise.
- Run focused tests and full Debug and Release CTest after substantive changes. Report actual counts and numerical tolerances.
- Never claim PASS when tests could not run or were not executed. Do not weaken assertions or modify expected numerical results merely to force PASS.
- Avoid Eigen expression-template lifetime bugs; materialize intermediate matrices when necessary.
- Prefer sparse algorithms for production, dense independently constructed references for small verification cases.
- Keep deterministic node/element/DOF ordering and stable results.

## Git and reporting
- Before edits: inspect `git status` and record the baseline. Preserve unrelated user changes.
- Before commits: inspect `git diff`, changed files, test inventory, and secrets/binary artifacts. Never use `git reset --hard`, destructive Git operations, destructive clean commands, or force push without explicit approval. Do not push to GitHub without explicit authorization.
- Commit only verified, scoped changes, using descriptive messages. Do not push automatically unless the user has authorized that workflow.
- At the end of each milestone, report: status (COMPLETE/INCOMPLETE/BLOCKED), modified files, numerical evidence, Debug/Release test counts, outstanding risks, and next roadmap task.
- Keep `docs/DEVELOPMENT_STATUS.md` (and `docs/REQUALIFICATION_STATUS.md` where applicable) synchronized with actual evidence after each verified milestone: verified functionality, test counts, numerical results, remaining limitations, current development phase, and next planned work. Distinguish reported historical results from results verified in the current session. Never classify unverified functionality as complete.

## Development cycle
Follow this sequence for each work period: **PLAN → IMPLEMENT → BUILD → TEST → DEBUG → VERIFY → DOCUMENT → REPORT → WAIT FOR APPROVAL.** Never skip independent engineering verification.

## Periodic progress reporting
Provide a technical progress report:
- Approximately every 30 minutes of active work, when practical. If precise elapsed-time tracking is unavailable, use natural task boundaries as checkpoints instead, and do not claim that exactly 30 minutes have elapsed.
- After completing a major development milestone.
- When encountering a significant technical or numerical blocker.
- Before making any high-impact engineering or architectural decision.

Do not interrupt an active build or test in a way that could corrupt files or invalidate results merely to hit a reporting cadence.

**All progress reports must be written in Turkish**, using this exact format:

```
# KOPRUQ-SOLVER — DEVELOPMENT PROGRESS REPORT

**Phase:** Current development phase

**1. Completed Work**
- Implemented features
- Modified files
- Engineering improvements

**2. Verification Results**
- Debug tests: PASS / FAIL / NOT RUN
- Release tests: PASS / FAIL / NOT RUN
- Numerical verification errors
- Relevant engineering benchmarks

**3. Problems and Corrections**
- Problems encountered
- Root causes
- Corrections applied
- Remaining issues

**4. Development Status**
- Current milestone completion
- Known limitations
- Git working tree status

**5. Next Planned Tasks**
- Task 1
- Task 2
- Task 3

**6. Approval Request**

Development is paused at the current checkpoint.

Do you approve continuing with the next planned tasks?

**WAITING FOR USER APPROVAL**
```

## Mandatory approval checkpoint
This is a strict requirement, stricter than and superseding the old looser "continuous work" habit of working through the whole roadmap unattended: **every progress report is an approval checkpoint, not merely an informational message.**

After every progress report:
1. Stop initiating new development tasks.
2. Do not proceed to the next roadmap task.
3. Do not start new code modifications.
4. Do not automatically continue debugging or testing.
5. Wait for an explicit user response.

Only resume development after the user gives clear approval, such as: "Continue", "Approved", "Devam", "Onaylıyorum". If the user requests changes to the proposed plan, revise the plan before proceeding. **Silence does not constitute approval.** Do not promise unattended or background execution; continue only while the current Claude Code session is running, within an approved work period, and permitted.

Where this section conflicts with any looser phrasing elsewhere in this file (e.g. "do not stop after every successful micro-task"), this section governs: resolve conflicting instructions by applying the more restrictive safety/approval rule.

## Error handling
Within an approved work period, diagnose and resolve errors independently; do not repeatedly request permission for routine debugging. For persistent errors: record the root cause, document attempted corrections, preserve existing source code, report remaining blockers, and propose the next technically justified action. If a major blocker prevents safe progress, issue a progress report immediately (per the format above) and wait for approval rather than continuing to push through it.
