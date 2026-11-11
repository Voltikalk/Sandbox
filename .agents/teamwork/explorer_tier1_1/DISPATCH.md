# Dispatch: Explorer 1 for Tier 1 Tests (Features F1 - F8)

## Role & Scope
You are **Explorer 1** (`teamwork_explorer`).
- Working directory: `c:\Users\Drilla\Desktop\Sandbox`
- State folder: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_1`
- Parent orchestrator ID: 9ae2990c-8916-46fc-93a6-792421757ac2

## Mandatory Input Documents
You MUST read:
1. `ORIGINAL_REQUEST.md`: `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
2. `PROJECT.md`: `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
3. `TEST_INFRA.md`: `c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md`
4. `SCOPE.md`: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md`
5. Test Framework Header: `tests/e2e/e2e_framework.hpp`
6. Existing project headers in `include/` and `src/` relevant to F1-F8:
   - Chunk layout, World, MaterialRegistry, Material definitions, Powder/Liquid automata, Brush helpers, Texture rendering helpers.

## Mission: Investigate Features F1 - F8
Examine the public APIs, types, and test framework for:
- **F1**: 64x64 SoA Chunk Layout
- **F2**: MaterialRegistry & Schema
- **F3**: Core Materials Assets
- **F4**: Powder Cellular Automata
- **F5**: Liquid Cellular Automata
- **F6**: Density Mutual Displacement
- **F7**: Streaming Texture Rendering
- **F8**: Interactive Mouse Brush

For each feature F1 through F8:
1. Identify the exact public classes, functions, enums, and constants to call.
2. Formulate at least 5 concrete, happy-path test case specifications for Tier 1 (name, setup, ticks/actions, assertion invariants).
3. Provide code snippets using `e2e_framework.hpp` showing exactly how the Worker should write each test.
4. Note any caveats, limitations, or prerequisites.

Write your report to `handoff.md` in your state folder and notify parent via `send_message`.


## 2026-11-11T07:21:14Z
You are Explorer 1 for Tier 1 Tests (Features F1 - F8).
Read your dispatch file at c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_1\DISPATCH.md.
Also read ORIGINAL_REQUEST.md, PROJECT.md, TEST_INFRA.md, SCOPE.md, and tests/e2e/e2e_framework.hpp.
Explore the public APIs for Features F1 through F8 in the codebase.
Formulate at least 5 concrete Tier 1 test specifications for each feature (>= 40 tests total for F1-F8).
Include exact C++ code snippets using e2e_framework.hpp.
Write your comprehensive report to c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_1\handoff.md.
When finished, send a message to parent (9ae2990c-8916-46fc-93a6-792421757ac2) with summary.
