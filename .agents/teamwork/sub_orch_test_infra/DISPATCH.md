# Dispatch: Sub-Orchestrator for Milestone M_TEST_1 (Test Infra & Runner Harness)

## Role & Scope
You are the **Sub-Orchestrator** for Milestone **M_TEST_1** (Test Infra & Runner Harness).
- Working directory: `c:\Users\Drilla\Desktop\Sandbox`
- State folder: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra`
- Parent conversation ID: c55a0c15-beb7-4a48-8775-5e1ffc0ea5b5

## Reference Documents
- `ORIGINAL_REQUEST.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
- `PROJECT.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
- `SCOPE.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md`

## Milestone Objectives
1. Design and establish the opaque-box requirement-driven E2E test architecture for the sandbox engine.
2. Implement test harness header `tests/e2e/e2e_framework.hpp` containing:
   - Lightweight standalone test registry and runner macros (`E2E_TEST`, `ASSERT_TRUE`, `ASSERT_EQ`, `ASSERT_FLOAT_NEAR`, etc.).
   - Headless test execution helpers (World instantiation, deterministic stepping / ticking, cell queries and verification utilities).
3. Implement `tests/e2e/test_main.cpp`:
   - Standalone CLI runner with test filtering (`--tier=1,2,3,4` or `--all`), verbose reporting, and clean summary output.
4. Integrate with `tests/CMakeLists.txt` and root `CMakeLists.txt` to register target `sandbox_e2e_tests` with CTest.
5. Create and publish `TEST_INFRA.md` at project root following the prompt template:
   - Test philosophy, feature inventory mapping, test architecture, real-world application scenarios outline, and coverage thresholds.
6. Verify that the harness compiles cleanly and executes with exit code 0.

## Workflow Instructions
- You are a Sub-Orchestrator. Assess your scope: this milestone fits a single cycle (2B Iteration Loop).
- Execute the standard cycle:
  - Step a: Spawn 3 `teamwork_explorer` agents to inspect existing CMake setup, tests layout, and formulate the exact test infra implementation plan.
  - Step b: Spawn a `teamwork_worker` (or `teamwork_test_writer`) with Explorer findings, write ownership (`tests/e2e/*`, `tests/CMakeLists.txt`, `TEST_INFRA.md`), and mandatory integrity warning.
  - Step c: Spawn 2 `teamwork_reviewer` agents.
  - Step d: Spawn 2 `teamwork_challenger` agents.
  - Step e: Spawn a `teamwork_auditor` agent for forensic integrity verification.
  - Step f: Gate check in `GATE_STATUS.md`.
- Maintain `BRIEFING.md` and `progress.md` in your state folder.
- When gate passes, deliver a self-contained `handoff.md` and report to parent via `send_message`.
