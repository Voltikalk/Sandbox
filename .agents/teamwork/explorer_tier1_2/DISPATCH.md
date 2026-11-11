# Dispatch: Explorer 2 for Tier 1 Tests (Features F9 - F14)

## Role & Scope
You are **Explorer 2** (`teamwork_explorer`).
- Working directory: `c:\Users\Drilla\Desktop\Sandbox`
- State folder: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_2`
- Parent orchestrator ID: 9ae2990c-8916-46fc-93a6-792421757ac2

## Mandatory Input Documents
You MUST read:
1. `ORIGINAL_REQUEST.md`: `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
2. `PROJECT.md`: `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
3. `TEST_INFRA.md`: `c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md`
4. `SCOPE.md`: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md`
5. Test Framework Header: `tests/e2e/e2e_framework.hpp`
6. Existing project headers in `include/` and `src/` relevant to F9-F14:
   - Heat diffusion system, Material phase transitions, ReactionSystem, Gas cellular automata, Fire & Combustion, ExplosionSystem.

## Mission: Investigate Features F9 - F14
Examine the public APIs, types, and test framework for:
- **F9**: Cellular Heat Diffusion
- **F10**: Material Phase Transitions
- **F11**: ReactionSystem & Table
- **F12**: Gas Cellular Automata
- **F13**: Fire & Combustion
- **F14**: ExplosionSystem

For each feature F9 through F14:
1. Identify the exact public classes, functions, enums, and constants to call.
2. Formulate at least 5 concrete, happy-path test case specifications for Tier 1 (name, setup, ticks/actions, assertion invariants).
3. Provide code snippets using `e2e_framework.hpp` showing exactly how the Worker should write each test.
4. Note any caveats, limitations, or prerequisites.

Write your report to `handoff.md` in your state folder and notify parent via `send_message`.


## 2026-11-11T07:21:24Z
You are Explorer 2 for Tier 1 Tests (Features F9 - F14).
Read your dispatch file at c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_2\DISPATCH.md.
Also read ORIGINAL_REQUEST.md, PROJECT.md, TEST_INFRA.md, SCOPE.md, and tests/e2e/e2e_framework.hpp.
Explore the public APIs for Features F9 through F14 in the codebase.
Formulate at least 5 concrete Tier 1 test specifications for each feature (>= 30 tests total for F9-F14).
Include exact C++ code snippets using e2e_framework.hpp.
Write your comprehensive report to c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_2\handoff.md.
When finished, send a message to parent (9ae2990c-8916-46fc-93a6-792421757ac2) with summary.
