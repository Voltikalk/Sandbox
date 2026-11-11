# Dispatch: Worker for Milestone M_TEST_2 (Tier 1 Feature Coverage Tests)

## Role & Scope
You are **Worker 1** (`teamwork_worker`).
- Working directory: `c:\Users\Drilla\Desktop\Sandbox`
- State folder: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\worker_tier1_1`
- Parent orchestrator ID: 9ae2990c-8916-46fc-93a6-792421757ac2

## Write Ownership
You have EXCLUSIVE write ownership of:
- `tests/e2e/tier1_features.cpp`
- `tests/CMakeLists.txt` (only if needed to link UI sources or ensure test targets build)
You MUST NOT modify engine implementation sources (`src/sim/*`, `include/sandbox/*`).

## Mandatory Integrity Warning
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

## Mandatory Input Documents
You MUST read:
1. `ORIGINAL_REQUEST.md`: `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
2. `PROJECT.md`: `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
3. `TEST_INFRA.md`: `c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md`
4. `SCOPE.md`: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md`
5. Test Framework Header: `tests/e2e/e2e_framework.hpp`
6. Explorer Reports:
   - Explorer 1 (F1-F8, 41 test specs): `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_1\handoff.md`
   - Explorer 2 (F9-F14, 30 test specs): `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_2\handoff.md`
   - Explorer 3 (F15-F24, 51 test specs): `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_tier1_3\handoff.md`

## Milestone Objectives
1. Implement all 122 Tier 1 test cases across Features F1 through F24 in `tests/e2e/tier1_features.cpp`.
2. Ensure every feature F1 through F24 has >= 5 test cases covering functional happy paths using public APIs (`World`, `MaterialRegistry`, `ReactionSystem`, etc.) and `TestWorldFixture` / `inspection` / `step` helpers from `e2e_framework.hpp`.
3. If `Renderer` and `Brush` symbols are referenced by F7 and F8 tests, ensure `tests/CMakeLists.txt` links `${UI_SOURCES}` (`src/ui/Renderer.cpp`, `src/ui/Brush.cpp`) into `sandbox_e2e_tests`.
4. Compile the project with CMake / MSVC. Ensure zero warnings and zero errors under `/W4 /WX /std:c++20`.
5. Execute the test runner:
   - Run `build/bin/Release/sandbox_e2e_tests.exe --tier=1` (or appropriate build dir)
   - Run `ctest -R e2e_tier1 --output-on-failure`
   Verify that all 122+ Tier 1 tests pass 100% with exit code 0.
6. Write a comprehensive handoff report to `handoff.md` in your state folder detailing:
   - Exact list of test cases implemented per feature (with counts)
   - Build commands and compilation output
   - Test execution commands and full pass results
7. Send a message to parent orchestrator (9ae2990c-8916-46fc-93a6-792421757ac2) when done.
