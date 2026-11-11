## 2026-11-11T06:38:36Z
You are a Worker agent for Milestone M_TEST_1 (Test Infra & Runner Harness) — Iteration 2.
Your role: Test Framework & Infra Remediation Worker.
Working directory: c:\Users\Drilla\Desktop\Sandbox
State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\worker_mtest1_2

Mandatory files to read first:
- ORIGINAL_REQUEST.md: c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md
- PROJECT.md: c:\Users\Drilla\Desktop\Sandbox\PROJECT.md
- SCOPE.md: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md
- GATE_STATUS.md: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra_gen2\GATE_STATUS.md
- Explorer 1 (It 2) Report & Patch: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_1\handoff.md and c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_1\proposed_e2e_framework.patch
- Explorer 2 (It 2) Report & Patch: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_2\handoff.md and c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_2\test_main_cli_syntax.patch
- Explorer 3 (It 2) Report: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_3\handoff.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Write Ownership (exclusive to you):
- tests/e2e/e2e_framework.hpp
- tests/e2e/test_main.cpp

Your mission:
1. Update `tests/e2e/e2e_framework.hpp`:
   - Add `#undef min` and `#undef max` after `<windows.h>`.
   - Add `is_standard_integer_v` trait discriminating standard integers from `bool`, `char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`.
   - Implement safe comparators `CompareEqual`, `CompareNotEqual`, `CompareGreater`, `CompareLess`, `CompareGreaterEqual`, `CompareLessEqual` using `std::cmp_*` for standard integers and standard operators otherwise.
   - Implement updated `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_GT`, `ASSERT_LT`, `ASSERT_GE`, `ASSERT_LE` macros.
   - Improve `ASSERT_FLOAT_NEAR` difference formatting.
   - Normalize path separators in `ResolveAssetPath` via `std::filesystem::path::lexically_normal().generic_string()`.
   - In `TestWorldFixture::EnsureMaterialsLoaded()`, check return value of `LoadFromDirectory` and log error if false.
2. Update `tests/e2e/test_main.cpp`:
   - Add `#undef min` and `#undef max` after `<windows.h>`.
   - In `CommandLineOptions`, add `bool hasSyntaxError{false};` and `std::string errorMessage{""};`.
   - In `ParseCommandLine()`, validate `--tier=N` (N in 1..5). Missing values on `--tier`, `--filter`, `--test` or unknown arguments set `hasSyntaxError = true`.
   - Set `--all` and default execution to run Tiers 1..4 only (`runTier5 = false`). Tier 5 is strictly opt-in via `--tier=5`.
   - In `main()`, check `hasSyntaxError` first: prints `errorMessage`, prints usage, and returns exit code 2. User help (`--help`, `-h`) returns exit code 0.
   - Expand `E2E_TEST(HarnessSanity, AssertionsAndWorldFixture, 1)` with all assertions: boolean equality (`ASSERT_EQ(true, true)`, `ASSERT_EQ(false, false)`, `ASSERT_NE(true, false)`), character equality (`ASSERT_EQ('P', 'P')`, `ASSERT_NE('A', 'B')`), mixed-signedness integers (`ASSERT_LT(-1, 1u)`, `ASSERT_GT(10u, 5)`, `ASSERT_GE(5u, 5)`, `ASSERT_LE(5, 5u)`), float near (`ASSERT_FLOAT_NEAR(1.0f, 1.0f, 1e-6f)`).
3. Rebuild and verify:
   - `cmake --build build --config Release --target sandbox_e2e_tests` -> 0 errors, 0 warnings.
   - `.\build\Release\sandbox_e2e_tests.exe --verbose` -> Sanity test passes with 100% success.
   - `.\build\Release\sandbox_e2e_tests.exe --invalid-flag` -> Exit code 2.
   - `.\build\Release\sandbox_e2e_tests.exe --tier=9` -> Exit code 2.
   - `.\build\Release\sandbox_e2e_tests.exe --help` -> Exit code 0.
   - `ctest --test-dir build -C Release --output-on-failure` -> 6/6 tests pass (100%).
4. Write handoff report to `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\worker_mtest1_2\handoff.md`.
5. Send completion message to parent orchestrator.
