# Handoff Report: Milestone M_TEST_1 (Test Runner & CLI Challenger 2)

- **Agent**: `challenger_mtest1_2` (Test Runner & CLI Challenger 2)
- **Role**: Critic & Specialist (Empirical Challenger)
- **Milestone**: M_TEST_1 (Test Infra & Runner Harness)
- **Date**: 2026-11-11
- **Explicit Verdict**: `CHANGES_REQUESTED`

---

## 1. Observation

Direct empirical observations from executing commands in `c:\Users\Drilla\Desktop\Sandbox` against the build artifacts and codebase.

### 1.1 Finding 1 (CRITICAL): Invalid CLI Flags Exit with Code 0 Instead of Code 2
- **Specification Contract**: `TEST_INFRA.md § 4.3` explicitly defines runner exit codes:
  > **Exit Codes**
  > - `0`: All executed tests passed.
  > - `1`: One or more tests failed (assertion violation or unhandled exception).
  > - `2`: Command-line syntax or invocation error (unknown flag, invalid tier argument).

- **Implementation in `tests/e2e/test_main.cpp`**:
  Lines 102–105:
  ```cpp
  } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      opts.showHelp = true;
  }
  ```
  Lines 182–186:
  ```cpp
  CommandLineOptions opts = ParseCommandLine(argc, argv);
  if (opts.showHelp) {
      PrintUsage(argv[0]);
      return 0;
  }
  ```
  The variable `opts.showHelp` conflates a valid user help inquiry (`--help`, `-h`) with a command-line syntax or invocation error. When any unknown argument is encountered, it sets `opts.showHelp = true`, prints the usage synopsis, and returns `0`.

- **Verbatim Empirical Command Invocations and Results**:
  1. Invalid flag `--invalid-flag`:
     ```powershell
     .\build\Release\sandbox_e2e_tests.exe --invalid-flag; Write-Host "Exit code: $LASTEXITCODE"
     ```
     **Output**:
     ```
     Unknown argument: --invalid-flag
     Usage: C:\Users\Drilla\Desktop\Sandbox\build\Release\sandbox_e2e_tests.exe [options]
     ...
     Exit code: 0
     ```
     Observed: Exit code is `0` (`$LASTEXITCODE` = 0).
  2. Invalid tier argument `--tier=9`:
     ```powershell
     .\build\Release\sandbox_e2e_tests.exe --tier=9; Write-Host "Exit code: $LASTEXITCODE"
     ```
     **Output**:
     ```
     Unknown argument: --tier=9
     Usage: ...
     Exit code: 0
     ```
     Observed: Exit code is `0`.
  3. Non-numeric tier `--tier=abc`:
     `$LASTEXITCODE` = `0`.
  4. Missing tier value `--tier`:
     `$LASTEXITCODE` = `0`.
  5. Missing filter argument `--filter`:
     `$LASTEXITCODE` = `0`.
  6. Unknown flag combined with valid flag:
     ```powershell
     .\build\Release\sandbox_e2e_tests.exe --tier=1 --invalid-option; Write-Host "Exit code: $LASTEXITCODE"
     ```
     **Output**:
     ```
     Unknown argument: --invalid-option
     Usage: ...
     Exit code: 0
     ```
     Observed: When a valid option is combined with a typo or unknown option, test execution is completely bypassed and the program terminates with exit code `0`.

### 1.2 Finding 2 (MEDIUM): Flag `--all` and Default Execution Improperly Activate Tier 5
- **Specification Contract**: `TEST_INFRA.md § 4.3` explicitly defines `--all`:
  > | Flag | Description | Example |
  > |:---|:---|:---|
  > | `--all` | Execute all test tiers (Tiers 1 to 4) | `sandbox_e2e_tests --all` |
  > | `--tier=N` | Execute only tests belonging to Tier N ($N \in \{1, 2, 3, 4, 5\}$) | `sandbox_e2e_tests --tier=1` |

- **Implementation in `tests/e2e/test_main.cpp`**:
  Line 89–91:
  ```cpp
  } else if (arg == "--all") {
      opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = opts.runTier5 = true;
      explicitTierSelected = true;
  }
  ```
  Lines 108–110:
  ```cpp
  if (!explicitTierSelected) {
      opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = opts.runTier5 = true;
  }
  ```
- **Observed Behavior**:
  `--all` (and running without arguments) enables Tier 5 (`opts.runTier5 = true`).
  According to `PROJECT.md` and `SCOPE.md`, Tier 5 is "Adversarial Hardening Tests" scheduled for milestone M_TEST_5 / M5.
  CTest registers `e2e_all` as:
  ```cmake
  add_test(NAME e2e_all COMMAND sandbox_e2e_tests --all)
  ```
  When adversarial or failing stress tests are added to Tier 5, running CTest target `e2e_all` runs Tier 5 and fails, even though CTest only has individual tier targets for Tiers 1 through 4 (`e2e_tier1`, `e2e_tier2`, `e2e_tier3`, `e2e_tier4`).

### 1.3 Finding 3 (VERIFIED PASS): Substring and Tier Filtering
- **Observation**:
  - `--tier=1`: Successfully selected only Tier 1 tests (`HarnessSanity.AssertionsAndWorldFixture`). Output: `Total Registered: N | Selected: 1`, `Tier 1: 1/1 passed (PASSED) in 1.66 ms`, exit code 0.
  - `--tier=2`, `--tier=3`, `--tier=4`: Output: `Selected: 0`, `[NOTICE] No tests match the active criteria or filter.`, exit code 0.
  - `--filter=Sanity`: Matched `HarnessSanity.AssertionsAndWorldFixture`, passed, exit code 0.
  - `--filter=HarnessSanity`: Matched, passed, exit code 0.
  - `--filter=AssertionsAndWorldFixture`: Matched, passed, exit code 0.
  - `--test=Sanity`: Alias matched, passed, exit code 0.
  - `--filter=NonExistent`: Matched 0 tests, printed notice, exit code 0.
  - `--filter=`: Matched all registered tests, exit code 0.
  - Substring matching is case-sensitive (`--filter=sanity` matched 0 tests).

### 1.4 Finding 4 (VERIFIED PASS): Asset Path Resolution Across Working Directories
- **Tested Scenarios**:
  1. From project root `c:\Users\Drilla\Desktop\Sandbox`:
     - Log: `[INFO ] [MaterialRegistry] Successfully loaded 18 materials from assets/materials` (Resolved via Layer 1: CWD).
  2. From build directory `c:\Users\Drilla\Desktop\Sandbox\build`:
     - Log: `[INFO ] [MaterialRegistry] Successfully loaded 18 materials from C:\Users\Drilla\Desktop\Sandbox\build\Release\assets/materials` (Resolved via Layer 2: Executable Dir).
  3. From binary directory `c:\Users\Drilla\Desktop\Sandbox\build\Release`:
     - Log: `[INFO ] [MaterialRegistry] Successfully loaded 18 materials from assets/materials` (Resolved via Layer 1).
  4. From arbitrary temporary directory (`C:\Users\Drilla\AppData\Local\Temp\e2e_cwd_...`):
     - Log: `[INFO ] [MaterialRegistry] Successfully loaded 18 materials from C:\Users\Drilla\Desktop\Sandbox\build\Release\assets/materials` (Resolved via Layer 2).
  5. Standalone executable copied to temporary directory without local assets:
     - Log: `[INFO ] [MaterialRegistry] Successfully loaded 18 materials from C:/Users/Drilla/Desktop/Sandbox/assets\materials` (Resolved via Layer 3: Compile-time `SANDBOX_ASSETS_DIR`).
- **Minor Observation on Path Separators**:
  In `tests/e2e/e2e_framework.hpp` lines 291 and 306, `relativePath.rfind("assets/", 0) == 0` specifically matches forward slashes. If Windows native backslashes are passed (`assets\\materials`), Layer 2 and Layer 3 prefix replacement does not trigger, falling back to `exeDir / relativePath` or `baseAssets / relativePath`. Standardizing path separators with `std::filesystem::path` would improve Windows robustness.

### 1.5 Finding 5 (OBSERVED PASS/FAIL DEPENDENCY): CTest Integration
- Running `ctest --test-dir build -C Release --output-on-failure`:
  - When only the baseline Tier 1 sanity test is present: 6/6 tests pass (100% pass rate in 0.14 sec).
  - CTest targets correctly configure `WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"`.
  - When Tier 5 tests are present, `e2e_all` fails because `--all` includes Tier 5.

---

## 2. Logic Chain

1. **Premise**: `TEST_INFRA.md § 4.3` is the authoritative specification for the test harness and CLI contract created during M_TEST_1.
2. **Contractual Mandate**: Under § 4.3, exit code `2` is specifically reserved and mandated for:
   `Command-line syntax or invocation error (unknown flag, invalid tier argument)`.
3. **Observation**: Executing `.\build\Release\sandbox_e2e_tests.exe --invalid-flag`, `.\build\Release\sandbox_e2e_tests.exe --tier=9`, and `.\build\Release\sandbox_e2e_tests.exe --tier` all return exit code `0`.
4. **Code Cause**: In `tests/e2e/test_main.cpp`, `opts.showHelp = true` is set on unknown arguments (lines 102–105), and `main()` returns `0` whenever `opts.showHelp` is true (lines 182–186).
5. **Blast Radius / Impact**:
   - In automated CI/CD pipelines or batch scripts, a developer or workflow configuration with a misspelled flag (e.g. `--ter=1`, `--alll`, `--verboes`) will exit with code `0`.
   - CI runners relying on exit status will mark the test step as SUCCESSFUL without executing any tests.
   - This undermines the primary guarantee of an automated test harness.
6. **Secondary Contractual Discrepancy**:
   - `TEST_INFRA.md § 4.3` explicitly defines `--all` as "Execute all test tiers (Tiers 1 to 4)".
   - `test_main.cpp` line 90 and 109 enable Tier 5 on `--all` and on default runs.
   - Tier 5 represents adversarial hardening tests scheduled for M5/M_TEST_5, not standard feature verification. Running Tier 5 under `--all` breaks CTest `e2e_all`.
7. **Conclusion**: Milestone M_TEST_1 has not satisfied all requirements of `TEST_INFRA.md § 4.3`. Implementation changes are required.

---

## 3. Caveats

1. **Assertion Bug Identified by Challenger 1**:
   Challenger 1 identified a precision issue in `ASSERT_FLOAT_NEAR` (`tests/e2e/tier_empirical_stress.cpp`), where single-precision IEEE 754 float differences (e.g. `1.1f - 1.0f`) can exceed float epsilon when cast to `double`. This review focused on the CLI runner, flag combinations, working directories, and CTest integration as assigned.
2. **Missing Test Tiers**:
   Tiers 2, 3, and 4 currently contain 0 test cases; returning 0 tests with exit code 0 when `--tier=2` is invoked is standard behavior until milestones M_TEST_2, M_TEST_3, and M_TEST_4 populate those test suites.

---

## 4. Conclusion

**Verdict: `CHANGES_REQUESTED`**

Milestone M_TEST_1 cannot be confirmed in its current state due to two non-conformances with `TEST_INFRA.md § 4.3`:

### Required Action Items for Worker:
1. **Fix CLI Exit Code for Syntax/Invocation Errors**:
   - In `tests/e2e/test_main.cpp`, distinguish between a user requesting `--help` / `-h` (which should exit with `0`) and unknown/invalid options or malformed arguments (which MUST exit with `2`).
   - Add a field to `CommandLineOptions`: e.g. `bool syntaxError{false};` or `std::string syntaxErrorMessage{""};`.
   - In `ParseCommandLine()`, when an unknown argument or invalid tier (e.g. not in 1..5) is found, set `opts.syntaxError = true;`.
   - In `main()`:
     ```cpp
     if (opts.syntaxError) {
         PrintUsage(argv[0]);
         return 2;
     }
     if (opts.showHelp) {
         PrintUsage(argv[0]);
         return 0;
     }
     ```
2. **Align `--all` and Default Execution with Specification**:
   - In `tests/e2e/test_main.cpp`, update `--all` and default no-tier execution to enable Tiers 1 through 4:
     ```cpp
     opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true;
     opts.runTier5 = false; // Tier 5 is adversarial, opt-in via --tier=5
     ```
   - Only enable Tier 5 when explicitly requested via `--tier=5`.
3. **Rebuild and Re-verify**:
   - Ensure `.\build\Release\sandbox_e2e_tests.exe --invalid-flag` exits with `2`.
   - Ensure `.\build\Release\sandbox_e2e_tests.exe --help` exits with `0`.
   - Ensure `ctest --test-dir build -C Release` passes cleanly.

---

## 5. Verification Method

To verify the required fixes:

1. **Verify Invalid Flag Returns Exit Code 2**:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --invalid-flag
   if ($LASTEXITCODE -ne 2) { throw "Expected exit code 2, got $LASTEXITCODE" }
   ```

2. **Verify Invalid Tier Returns Exit Code 2**:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --tier=9
   if ($LASTEXITCODE -ne 2) { throw "Expected exit code 2, got $LASTEXITCODE" }
   ```

3. **Verify Help Flag Returns Exit Code 0**:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --help
   if ($LASTEXITCODE -ne 0) { throw "Expected exit code 0, got $LASTEXITCODE" }
   ```

4. **Verify `--all` Excludes Tier 5 Unless Explicitly Specified**:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --all
   # Output must state: Tiers Active: T1 T2 T3 T4 (T5 excluded)
   ```

5. **Verify Full CTest Suite**:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
   *Expected*: All registered tests pass.
