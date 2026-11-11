# Dispatch: Sub-Orchestrator for Milestone M_TEST_2 (Tier 1 Feature Coverage Tests)

## Role & Scope
You are the **Sub-Orchestrator** for Milestone **M_TEST_2** (Tier 1 Feature Coverage Tests).
- Working directory: `c:\Users\Drilla\Desktop\Sandbox`
- State folder: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_tier1_tests`
- Parent conversation ID: c55a0c15-beb7-4a48-8775-5e1ffc0ea5b5

## Reference Documents
- `ORIGINAL_REQUEST.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md`
- `PROJECT.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\PROJECT.md`
- `SCOPE.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md`
- `TEST_INFRA.md` (read-only): `c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md`
- Test Framework Header: `tests/e2e/e2e_framework.hpp`

## Milestone Objectives
1. Implement comprehensive **Tier 1 Feature Coverage Tests** in `tests/e2e/tier1_features.cpp`.
2. Follow the mathematical threshold in `TEST_INFRA.md § 6`:
   - Feature coverage requires $\ge 5$ test cases per feature in isolation (happy-path).
   - Must cover all 24 functional features (F1 through F24) identified in `PROJECT.md § Feature Inventory` and `TEST_INFRA.md § 3`.
   - Total test cases in Tier 1: $\ge 120$ test cases.
3. Use the established test registration macro `SANDBOX_TEST_T1(Suite, Name)` (or `SANDBOX_TEST(1, Suite, Name)`).
4. Utilize `TestWorldFixture` and inspection/stepping helpers (`inspection::CountMaterial`, `inspection::CountMaterialInBox`, `step::Ticks`, etc.) from `tests/e2e/e2e_framework.hpp`.
5. Ensure tests are 100% opaque-box, requirement-driven, deterministic, and headless.
6. Verify that `sandbox_e2e_tests --tier=1` and `ctest -R e2e_tier1` compile cleanly under MSVC `/W4 /WX /std:c++20` and pass 100% with exit code 0.

## Workflow Instructions
- Follow the 2B Iteration Loop (Explorer -> Worker/Test Writer -> Reviewer -> Challenger -> Auditor -> Gate).
  - Step a: Spawn 3 `teamwork_explorer` agents to partition the 24 features and map out test cases.
  - Step b: Spawn a `teamwork_worker` (or `teamwork_test_writer`) with write ownership of `tests/e2e/tier1_features.cpp` and the mandatory integrity warning.
  - Step c: Spawn 2 `teamwork_reviewer` agents.
  - Step d: Spawn 2 `teamwork_challenger` agents.
  - Step e: Spawn a `teamwork_auditor` agent for forensic integrity verification.
  - Step f: Gate evaluation in `GATE_STATUS.md`.
- Maintain `BRIEFING.md` and `progress.md` in your state folder.
- When gate passes, deliver a self-contained `handoff.md` and notify parent via `send_message`.
