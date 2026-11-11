## 2026-11-11T06:26:27Z
You are an Explorer agent for Milestone M_TEST_1 (Test Infra & Runner Harness) — Iteration 2.
Your role: Type Comparison & Macro Explorer.
Working directory: c:\Users\Drilla\Desktop\Sandbox
State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_1

Mandatory files to read first:
- ORIGINAL_REQUEST.md: c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md
- PROJECT.md: c:\Users\Drilla\Desktop\Sandbox\PROJECT.md
- SCOPE.md: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md
- GATE_STATUS.md: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra_gen2\GATE_STATUS.md
- Reviewer 1 Handoff: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\reviewer_mtest1_1\handoff.md
- Challenger 1 Handoff: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\challenger_mtest1_1\handoff.md

Your mission:
Formulate the exact fix strategy and code changes for `tests/e2e/e2e_framework.hpp` regarding type comparisons and assertion macros:
1. Review the failure reports from Reviewer 1 and Challenger 1:
   - `std::cmp_equal` in `CompareEqual` triggers MSVC `static_assert` when `bool` or `char` is passed.
   - `ASSERT_LT` / `ASSERT_GT` use raw relational operators, triggering warning C4018 and inverted logic on signed vs unsigned integers (e.g. `ASSERT_LT(-1, 1u)`).
   - `ASSERT_FLOAT_NEAR` difference formatting.
2. Design a robust C++20 standard integer concept / trait `is_standard_integer_v` that includes signed and unsigned char/short/int/long/long long, but EXCLUDES `bool`, `char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`.
3. Provide the complete, tested drop-in implementation for:
   - `CompareEqual` (using `std::cmp_equal` for standard integers, `a == b` otherwise)
   - `CompareNotEqual` (using `std::cmp_not_equal` for standard integers, `a != b` otherwise)
   - `CompareLess` (using `std::cmp_less` for standard integers, `a < b` otherwise)
   - `CompareGreater` (using `std::cmp_greater` for standard integers, `a > b` otherwise)
   - `CompareLessEqual` (using `std::cmp_less_equal` for standard integers, `a <= b` otherwise)
   - `CompareGreaterEqual` (using `std::cmp_greater_equal` for standard integers, `a >= b` otherwise)
   - `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_LT`, `ASSERT_GT`, `ASSERT_LE`, `ASSERT_GE` macros.
   - `ASSERT_FLOAT_NEAR` macro with improved formatting.
4. Write your findings to `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_1\handoff.md`.
Follow the Handoff Protocol. When finished, send a message to parent orchestrator (conv ID: 01dab337-aa6a-4014-b2f8-e686243da338).
