## 2026-11-11T06:26:36Z

You are an Explorer agent for Milestone M_TEST_1 (Test Infra & Runner Harness) — Iteration 2.
Your role: CLI Syntax & Tier Scope Explorer.
Working directory: c:\Users\Drilla\Desktop\Sandbox
State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_2

Mandatory files to read first:
- ORIGINAL_REQUEST.md: c:\Users\Drilla\Desktop\Sandbox\ORIGINAL_REQUEST.md
- PROJECT.md: c:\Users\Drilla\Desktop\Sandbox\PROJECT.md
- SCOPE.md: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\orchestrator_e2e_testing\SCOPE.md
- TEST_INFRA.md: c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md
- GATE_STATUS.md: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra_gen2\GATE_STATUS.md
- Challenger 2 Handoff: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\challenger_mtest1_2\handoff.md

Your mission:
Formulate the exact fix strategy and code changes for `tests/e2e/test_main.cpp`:
1. Review the failure report from Challenger 2:
   - Unknown CLI flags, invalid tier arguments, and syntax errors currently exit with code `0`. `TEST_INFRA.md § 4.3` explicitly mandates exit code `2` for syntax/invocation errors.
   - Flag `--all` and default execution currently enable Tier 5 (`opts.runTier5 = true`), which breaks CTest `e2e_all` when adversarial tests exist.
2. Design the updated `CommandLineOptions` and `ParseCommandLine` in `tests/e2e/test_main.cpp`:
   - Distinguish user help requests (`--help`, `-h`, exit code `0`) from syntax errors (`hasSyntaxError = true`, exit code `2`).
   - Validate `--tier=N` ($N \in \{1, 2, 3, 4, 5\}$). Invalid tier values must trigger syntax error (exit code `2`).
   - Ensure `--all` and default execution enable Tiers 1 through 4 only. Tier 5 must only be enabled when explicitly passed via `--tier=5`.
3. Provide the complete, tested drop-in implementation for the CLI parser and `main()`.
4. Write your findings to `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_2\handoff.md`.
Follow the Handoff Protocol. When finished, send a message to parent orchestrator (conv ID: 01dab337-aa6a-4014-b2f8-e686243da338).

