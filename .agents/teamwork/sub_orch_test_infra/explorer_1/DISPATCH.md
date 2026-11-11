# Task Assignment: Explorer 1 - CMake Build & CTest Integration

## Mission
Investigate the CMake configuration and compiler setup for the standalone E2E test runner target `sandbox_e2e_tests`.

## Input Files (Read-only)
- `ORIGINAL_REQUEST.md`: `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
- `PROJECT.md`: `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
- `SCOPE.md`: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md`
- Root `CMakeLists.txt`: `c:\Users\Drilla\Desktop\Sandbox\CMakeLists.txt`
- `src/` directory structure and existing code

## Specific Questions to Answer
1. How should root `CMakeLists.txt` include `tests/CMakeLists.txt` (e.g. `enable_testing()`, `add_subdirectory(tests)`)?
2. How should `sandbox_e2e_tests` target be configured in `tests/CMakeLists.txt`? What include directories, compile definitions, and C++20 flags are needed?
3. How to ensure `tests/e2e/test_main.cpp` and `tests/e2e/e2e_framework.hpp` can compile cleanly right now even if `src/sim` is being developed concurrently?
4. How to register test targets with CTest (e.g. `add_test(NAME e2e_all COMMAND sandbox_e2e_tests --all)`)?
5. What are the exact build commands and test commands under Windows (CMake / MSVC / Ninja / msbuild)?

## Output
Write your comprehensive analysis and handoff report to:
`c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra\explorer_1\handoff.md`
Do NOT write or modify source code.


## 2026-11-10T19:29:13Z
You are Explorer 1 for Milestone M_TEST_1 (Test Infra & Runner Harness).
Working directory: c:\Users\Drilla\Desktop\Sandbox
State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra\explorer_1
Dispatch file: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra\explorer_1\DISPATCH.md
Original request: c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md
Project specification: c:\Users\Drilla\Desktop\Sandbox\PROJECT.md
Scope document: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md

Investigate CMake build setup and CTest integration for the standalone `sandbox_e2e_tests` executable.
Read your DISPATCH.md for specific tasks.
Deliver your handoff report to:
c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra\explorer_1\handoff.md
Notify the orchestrator via send_message when complete.
