# Task Assignment: Explorer 2 - E2E Framework & Macro Architecture

## Mission
Investigate and design `tests/e2e/e2e_framework.hpp` containing the test registry, assertions, and headless simulation test fixture.

## Input Files (Read-only)
- `ORIGINAL_REQUEST.md`: `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
- `PROJECT.md`: `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
- `SCOPE.md`: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md`

## Specific Questions to Answer
1. Design lightweight standalone macro suite: `E2E_TEST(suite, name, tier)`, `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_LT`, `ASSERT_LE`, `ASSERT_GT`, `ASSERT_GE`, `ASSERT_FLOAT_NEAR(val, exp, eps)`.
2. How to implement the static test registration mechanism without external dependencies?
3. How to structure test metadata (Tier: 1, 2, 3, 4; Suite name; Test name; Execution function pointer; Description)?
4. How to design headless simulation test helpers and fixtures that conform to `PROJECT.md § Interface Contracts` (`World`, `MaterialRegistry`, `ReactionSystem`, stepping/ticking)?
5. How to allow the framework to run self-tests / smoke-tests out-of-the-box so milestone M_TEST_1 can verify the runner compiles and passes with exit code 0?

## Output
Write your comprehensive analysis and handoff report to:
`c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra\explorer_2\handoff.md`
Do NOT write or modify source code.


## 2026-11-10T19:29:28Z
You are Explorer 2 for Milestone M_TEST_1 (Test Infra & Runner Harness).
Working directory: c:\Users\Drilla\Desktop\Sandbox
State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra\explorer_2
Dispatch file: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra\explorer_2\DISPATCH.md
Original request: c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md
Project specification: c:\Users\Drilla\Desktop\Sandbox\PROJECT.md
Scope document: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md

Investigate and design `tests/e2e/e2e_framework.hpp` architecture (test registry, assertion macros, headless test fixtures, simulation mocks/hooks).
Read your DISPATCH.md for specific tasks.
Deliver your handoff report to:
c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra\explorer_2\handoff.md
Notify the orchestrator via send_message when complete.
