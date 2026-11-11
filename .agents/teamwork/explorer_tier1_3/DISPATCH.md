# Dispatch: Explorer 3 for Tier 1 Tests (Features F15 - F24)

## Role & Scope
You are **Explorer 3** (`teamwork_explorer`).
- Working directory: `c:\Users\Drilla\Desktop\Sandbox`
- State folder: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_3`
- Parent orchestrator ID: 9ae2990c-8916-46fc-93a6-792421757ac2

## Mandatory Input Documents
You MUST read:
1. `ORIGINAL_REQUEST.md`: `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
2. `PROJECT.md`: `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
3. `TEST_INFRA.md`: `c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md`
4. `SCOPE.md`: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md`
5. Test Framework Header: `tests/e2e/e2e_framework.hpp`
6. Existing project headers and source files relevant to F15-F24:
   - Concurrency (ThreadPool, 4-phase checkerboard, deterministic stepping)
   - Sleep & cascade wake (Chunk sleep status, active counts)
   - Performance telemetry (tick timing, 300k cells bench)
   - ImGui Palette data structures / categories
   - UI simulation controls (pause, step, gravity vector)
   - Telemetry overlay structures (F3 overlay metrics)
   - Binary RLE Save/Load (`SaveWorldToFile`, `LoadWorldFromFile`, magic bytes)
   - Headless CLI benchmark runner
   - CTest test structure
   - Documentation presence and structure (README, ARCH)

## Mission: Investigate Features F15 - F24
Examine the public APIs, types, and test framework for:
- **F15**: 4-Phase Checkerboard Concurrency
- **F16**: Sleeping Chunks & Cascade Wake
- **F17**: 300k+ Cells @ 60 TPS Perf
- **F18**: Dear ImGui Category Palette
- **F19**: UI Simulation Controls
- **F20**: F3 Debug Telemetry Overlay
- **F21**: Binary RLE Save/Load
- **F22**: Headless CLI Benchmark
- **F23**: CTest Modular Test Suite
- **F24**: Documentation (README/ARCH)

For each feature F15 through F24:
1. Identify the exact public classes, functions, enums, and constants to call.
2. Formulate at least 5 concrete, happy-path test case specifications for Tier 1 (name, setup, ticks/actions, assertion invariants).
3. Provide code snippets using `e2e_framework.hpp` showing exactly how the Worker should write each test.
4. Note any caveats, limitations, or prerequisites.

Write your report to `handoff.md` in your state folder and notify parent via `send_message`.


## 2026-11-11T07:21:33Z
[Message] timestamp=2026-11-11T07:21:33Z sender=9ae2990c-8916-46fc-93a6-792421757ac2 priority=MESSAGE_PRIORITY_HIGH content=You are Explorer 3 for Tier 1 Tests (Features F15 - F24).
Read your dispatch file at c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_3\DISPATCH.md.
Also read ORIGINAL_REQUEST.md, PROJECT.md, TEST_INFRA.md, SCOPE.md, and tests/e2e/e2e_framework.hpp.
Explore the public APIs for Features F15 through F24 in the codebase.
Formulate at least 5 concrete Tier 1 test specifications for each feature (>= 50 tests total for F15-F24).
Include exact C++ code snippets using e2e_framework.hpp.
Write your comprehensive report to c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_3\handoff.md.
When finished, send a message to parent (9ae2990c-8916-46fc-93a6-792421757ac2) with summary.
