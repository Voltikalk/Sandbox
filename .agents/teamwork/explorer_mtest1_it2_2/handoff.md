# Handoff Report: Milestone M_TEST_1 (Iteration 2) — CLI Syntax & Tier Scope Explorer

- **Agent**: `explorer_mtest1_it2_2`
- **Role**: CLI Syntax & Tier Scope Explorer
- **Milestone**: M_TEST_1 (Test Infra & Runner Harness) — Iteration 2
- **State Folder**: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_2`
- **Date**: 2026-11-11
- **Target File**: `tests/e2e/test_main.cpp`
- **Handoff Type**: Hard (Investigation complete, drop-in fix designed and empirically validated)

---

## 1. Observation

Direct empirical observations from `c:\Users\Drilla\Desktop\Sandbox` against `tests/e2e/test_main.cpp`, `TEST_INFRA.md`, and build artifacts:

### 1.1 Specification Contract (`TEST_INFRA.md § 4.3`)
- **CLI Options & Semantics**:
  > | Flag | Description | Example |
  > |:---|:---|:---|
  > | `--all` | Execute all test tiers (Tiers 1 to 4) | `sandbox_e2e_tests --all` |
  > | `--tier=N` | Execute only tests belonging to Tier N ($N \in \{1, 2, 3, 4, 5\}$) | `sandbox_e2e_tests --tier=1` |
  > | `--test=<substring>` | Execute tests whose suite or name contains `<substring>` | `sandbox_e2e_tests --test=Powder` |
  > | `--list` | Print a list of all registered tests and exit | `sandbox_e2e_tests --list` |
  > | `--verbose` | Print individual assertion passes, step timings, and debug stats | `sandbox_e2e_tests --verbose` |
  > | `--help` | Display usage instructions and available options | `sandbox_e2e_tests --help` |
- **Exit Codes**:
  > - `0`: All executed tests passed.
  > - `1`: One or more tests failed (assertion violation or unhandled exception).
  > - `2`: Command-line syntax or invocation error (unknown flag, invalid tier argument).

### 1.2 Baseline Code Defects in `tests/e2e/test_main.cpp`
- **Defect 1: Conflation of Help and Syntax Errors** (`tests/e2e/test_main.cpp:102-105, 182-186`):
  ```cpp
  // Lines 102-105:
  } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      opts.showHelp = true;
  }
  // Lines 182-186:
  CommandLineOptions opts = ParseCommandLine(argc, argv);
  if (opts.showHelp) {
      PrintUsage(argv[0]);
      return 0;
  }
  ```
  Whenever any unknown argument was encountered, `opts.showHelp` was set to `true`, causing `main()` to print usage and exit with code `0`.
- **Defect 2: Lack of Tier Argument Validation** (`tests/e2e/test_main.cpp:74-88`):
  Exact matching (`arg == "--tier=1"`) was used. Any invalid tier argument (such as `--tier=9`, `--tier=abc`, `--tier=`, or `--tier`) fell through to the `else` block, triggering Defect 1 and exiting with code `0`.
- **Defect 3: Tier 5 Enabled by Default and by `--all`** (`tests/e2e/test_main.cpp:89-91, 108-110`):
  ```cpp
  // Lines 89-91:
  } else if (arg == "--all") {
      opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = opts.runTier5 = true;
      explicitTierSelected = true;
  }
  // Lines 108-110:
  if (!explicitTierSelected) {
      opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = opts.runTier5 = true;
  }
  ```
  This violated `TEST_INFRA.md § 4.3` (which defines `--all` as Tiers 1-4) and activated adversarial hardening tests (scheduled for M5) under CTest `e2e_all` and standard runs.

### 1.3 Verbatim Empirical Reproduction
Executing the unpatched executable:
1. Unknown flag:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --invalid-flag; Write-Host "Exit code: $LASTEXITCODE"
   ```
   *Output*:
   ```
   Unknown argument: --invalid-flag
   Usage: ...
   Exit code: 0
   ```
2. Invalid tier argument:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --tier=9; Write-Host "Exit code: $LASTEXITCODE"
   ```
   *Output*:
   ```
   Unknown argument: --tier=9
   Usage: ...
   Exit code: 0
   ```
3. Flag `--all`:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --all
   ```
   *Output*:
   ```
   Tiers Active: T1 T2 T3 T4 T5 
   ```
4. Default execution (no arguments):
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe
   ```
   *Output*:
   ```
   Tiers Active: T1 T2 T3 T4 T5 
   ```

---

## 2. Logic Chain

1. **Premise**: `TEST_INFRA.md § 4.3` explicitly reserves and mandates exit code `2` for syntax/invocation errors, reserving exit code `0` for clean passes and user help/list queries.
2. **Observation**: In `tests/e2e/test_main.cpp`, `opts.showHelp = true` was set on syntax errors, and `main()` unconditionally returned `0` whenever `opts.showHelp` was true.
3. **Inference**: To satisfy § 4.3, `CommandLineOptions` must introduce dedicated state fields:
   - `bool hasSyntaxError{false};`
   - `std::string errorMessage{""};`
4. **Precedence Rule**: If both a valid flag / help request and a syntax error occur on the command line (e.g. `--help --invalid-flag` or `--tier=1 --bad`), the syntax error must take precedence. In `main()`, `if (opts.hasSyntaxError)` must be evaluated before `if (opts.showHelp)`.
5. **Tier Validation Rule**: Rather than hardcoding 5 individual string equality checks that fall through to generic unknown arguments, `--tier=` must be parsed by prefix (`arg.rfind("--tier=", 0) == 0`). The payload must be validated strictly against `{"1", "2", "3", "4", "5"}`:
   - If in range: activate the corresponding tier flag (`opts.runTierN = true`) and set `explicitTierSelected = true`.
   - If out of range (e.g. `--tier=9`, `--tier=abc`, `--tier=`): set `opts.hasSyntaxError = true` and `opts.errorMessage = "Invalid tier argument: " + arg + " (expected 1, 2, 3, 4, or 5)"`.
   - If `--tier` is passed without `=`: set `opts.hasSyntaxError = true` and `opts.errorMessage = "Missing tier value for '--tier'. Expected '--tier=N' (N in 1..5)"`.
6. **Tier Scope Rule**: Per `TEST_INFRA.md § 4.3`, `--all` executes Tiers 1 through 4. Therefore:
   - `--all` must set `opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true; opts.runTier5 = false;`.
   - Default execution (when `!explicitTierSelected`) must set `opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true; opts.runTier5 = false;`.
   - Tier 5 is strictly opt-in and is only activated when explicitly requested via `--tier=5`.
7. **Empirical Validation**: The proposed fix was compiled with MSVC 17 2022 C++20 and validated across 14 distinct invocation scenarios, achieving 100% compliance with `TEST_INFRA.md`.

---

## 3. Caveats

1. **Scope Boundaries**:
   This investigation and patch strictly target `tests/e2e/test_main.cpp` (CLI parser, error codes, and tier activation). Peer explorer `explorer_mtest1_it2_1` addresses `e2e_framework.hpp` type comparison macros, and `explorer_mtest1_it2_3` addresses asset diagnostic checks and sanity tests.
2. **Multiple Invalid Arguments**:
   When multiple invalid arguments are passed (e.g. `--foo --bar`), all error messages are accumulated into `opts.errorMessage` separated by newlines, presenting a comprehensive diagnostic before the usage synopsis.
3. **Empty Filter Semantics**:
   Passing `--filter=` or `--test=` with an empty string sets `opts.filter = ""`, which matches all registered tests (returning exit code 0). Missing filter flags without `=` (`--filter` / `--test`) are flagged as syntax errors (exit code 2).
4. **Explorer Read-Only Discipline**:
   In strict accordance with the Explorer archetype, `tests/e2e/test_main.cpp` was verified and then restored to its pre-iteration-2 baseline. The complete, machine-applicable patch is preserved in `test_main_cli_syntax.patch` for the Worker.

---

## 4. Conclusion & Proposed Implementation

### 4.1 Summary of Changes
1. **`struct CommandLineOptions`**: Added `bool hasSyntaxError{false};` and `std::string errorMessage{""};`.
2. **`void PrintUsage(const char* exeName)`**: Updated usage text to clarify that `--all` runs Tiers 1 to 4 and `--tier=5` is opt-in adversarial hardening.
3. **`CommandLineOptions ParseCommandLine(int argc, char* argv[])`**:
   - Prefix match for `--tier=` with validation of $N \in \{1, 2, 3, 4, 5\}$.
   - Error detection for bare `--tier`, `--filter`, `--test`.
   - `--all` sets `runTier1..runTier4 = true`, `runTier5 = false`.
   - Default sets `runTier1..runTier4 = true`, `runTier5 = false`.
   - Any unknown argument sets `hasSyntaxError = true` and accumulates into `errorMessage`.
4. **`int main(int argc, char* argv[])`**:
   - Evaluates `opts.hasSyntaxError` first: prints `opts.errorMessage` to `std::cerr`, calls `PrintUsage(argv[0])`, and returns exit code `2`.
   - Evaluates `opts.showHelp`: calls `PrintUsage(argv[0])` and returns exit code `0`.

---

### 4.2 Complete Drop-In Implementation for Worker

Below is the complete, drop-in replacement for `tests/e2e/test_main.cpp` from line 40 to line 215:

```cpp
// ============================================================================
// CLI Options & Parsing
// ============================================================================
struct CommandLineOptions {
    bool runTier1{false};
    bool runTier2{false};
    bool runTier3{false};
    bool runTier4{false};
    bool runTier5{false};
    bool verbose{false};
    bool listOnly{false};
    bool showHelp{false};
    bool hasSyntaxError{false};
    std::string errorMessage{""};
    std::string filter{""};
};

void PrintUsage(const char* exeName) {
    std::cout << "Usage: " << exeName << " [options]\n\n"
              << "Options:\n"
              << "  --tier=1           Run only Tier 1: Feature Coverage Tests\n"
              << "  --tier=2           Run only Tier 2: Boundary & Corner Case Tests\n"
              << "  --tier=3           Run only Tier 3: Pairwise Cross-Feature Tests\n"
              << "  --tier=4           Run only Tier 4: Workload & Scenario Tests\n"
              << "  --tier=5           Run only Tier 5: Adversarial Hardening Tests (opt-in)\n"
              << "  --all              Run Tiers 1 to 4 (default if no tier specified)\n"
              << "  --verbose, -v      Enable detailed per-test logging and assertions\n"
              << "  --filter=<str>     Run only tests whose Suite.Name contains <str>\n"
              << "  --test=<str>       Alias for --filter=<str>\n"
              << "  --list             List all registered tests without running\n"
              << "  --help, -h         Show this help message\n";
}

CommandLineOptions ParseCommandLine(int argc, char* argv[]) {
    CommandLineOptions opts;
    bool explicitTierSelected = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg.rfind("--tier=", 0) == 0) {
            std::string_view val = arg.substr(7);
            if (val == "1") {
                opts.runTier1 = true;
                explicitTierSelected = true;
            } else if (val == "2") {
                opts.runTier2 = true;
                explicitTierSelected = true;
            } else if (val == "3") {
                opts.runTier3 = true;
                explicitTierSelected = true;
            } else if (val == "4") {
                opts.runTier4 = true;
                explicitTierSelected = true;
            } else if (val == "5") {
                opts.runTier5 = true;
                explicitTierSelected = true;
            } else {
                opts.hasSyntaxError = true;
                if (!opts.errorMessage.empty()) opts.errorMessage += "\n";
                opts.errorMessage += "Invalid tier argument: " + std::string(arg) + " (expected 1, 2, 3, 4, or 5)";
            }
        } else if (arg == "--tier") {
            opts.hasSyntaxError = true;
            if (!opts.errorMessage.empty()) opts.errorMessage += "\n";
            opts.errorMessage += "Missing tier value for '--tier'. Expected '--tier=N' (N in 1..5)";
        } else if (arg == "--all") {
            opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true;
            opts.runTier5 = false;
            explicitTierSelected = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--list") {
            opts.listOnly = true;
        } else if (arg == "--help" || arg == "-h") {
            opts.showHelp = true;
        } else if (arg.rfind("--filter=", 0) == 0) {
            opts.filter = std::string(arg.substr(9));
        } else if (arg.rfind("--test=", 0) == 0) {
            opts.filter = std::string(arg.substr(7));
        } else if (arg == "--filter" || arg == "--test") {
            opts.hasSyntaxError = true;
            if (!opts.errorMessage.empty()) opts.errorMessage += "\n";
            opts.errorMessage += "Missing argument for option: " + std::string(arg) + " (expected '" + std::string(arg) + "=<str>')";
        } else {
            opts.hasSyntaxError = true;
            if (!opts.errorMessage.empty()) opts.errorMessage += "\n";
            opts.errorMessage += "Unknown argument: " + std::string(arg);
        }
    }

    // Default execution: if no tier or --all was explicitly specified, enable Tiers 1-4 (Tier 5 is opt-in)
    if (!explicitTierSelected) {
        opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true;
        opts.runTier5 = false;
    }

    return opts;
}
```

And in `int main(int argc, char* argv[])`:
```cpp
int main(int argc, char* argv[]) {
    EnableVirtualTerminal();

    CommandLineOptions opts = ParseCommandLine(argc, argv);
    if (opts.hasSyntaxError) {
        if (!opts.errorMessage.empty()) {
            std::cerr << opts.errorMessage << "\n\n";
        }
        PrintUsage(argv[0]);
        return 2;
    }

    if (opts.showHelp) {
        PrintUsage(argv[0]);
        return 0;
    }

    const auto& allTests = TestRegistry::Instance().GetTests();
...
```

---

## 5. Verification Method

To independently verify the drop-in implementation:

### 5.1 Build Command
```powershell
cmake --build build --config Release --target sandbox_e2e_tests
```
*Expected*: Clean build with 0 warnings and 0 errors.

### 5.2 Empirical Test Matrix & Verification Commands
| Scenario | Command | Expected Output & Behavior | Expected Exit Code |
|:---|:---|:---|:---:|
| **Unknown flag** | `.\build\Release\sandbox_e2e_tests.exe --invalid-flag` | `Unknown argument: --invalid-flag` + Usage | `2` |
| **Invalid tier ($>5$)** | `.\build\Release\sandbox_e2e_tests.exe --tier=9` | `Invalid tier argument: --tier=9 (expected 1, 2, 3, 4, or 5)` + Usage | `2` |
| **Invalid tier (non-numeric)** | `.\build\Release\sandbox_e2e_tests.exe --tier=abc` | `Invalid tier argument: --tier=abc (expected 1, 2, 3, 4, or 5)` + Usage | `2` |
| **Invalid tier (empty)** | `.\build\Release\sandbox_e2e_tests.exe --tier=` | `Invalid tier argument: --tier= (expected 1, 2, 3, 4, or 5)` + Usage | `2` |
| **Missing tier value** | `.\build\Release\sandbox_e2e_tests.exe --tier` | `Missing tier value for '--tier'. Expected '--tier=N' (N in 1..5)` + Usage | `2` |
| **Missing filter value** | `.\build\Release\sandbox_e2e_tests.exe --filter` | `Missing argument for option: --filter (expected '--filter=<str>')` + Usage | `2` |
| **Missing test value** | `.\build\Release\sandbox_e2e_tests.exe --test` | `Missing argument for option: --test (expected '--test=<str>')` + Usage | `2` |
| **Valid + Invalid flag** | `.\build\Release\sandbox_e2e_tests.exe --tier=1 --invalid-option` | `Unknown argument: --invalid-option` + Usage (tests aborted) | `2` |
| **Help flag (`--help`)** | `.\build\Release\sandbox_e2e_tests.exe --help` | Usage printed without error message | `0` |
| **Help flag (`-h`)** | `.\build\Release\sandbox_e2e_tests.exe -h` | Usage printed without error message | `0` |
| **List flag (`--list`)** | `.\build\Release\sandbox_e2e_tests.exe --list` | `Registered E2E Tests (1 total)` | `0` |
| **Flag `--all`** | `.\build\Release\sandbox_e2e_tests.exe --all` | `Tiers Active: T1 T2 T3 T4 ` (Tier 5 excluded) | `0` |
| **Default run (no flags)**| `.\build\Release\sandbox_e2e_tests.exe` | `Tiers Active: T1 T2 T3 T4 ` (Tier 5 excluded) | `0` |
| **Opt-in Tier 5** | `.\build\Release\sandbox_e2e_tests.exe --tier=5` | `Tiers Active: T5 ` | `0` |

### 5.3 Automated PowerShell Test Suite
```powershell
$bin = ".\build\Release\sandbox_e2e_tests.exe"

# 1. Verify Syntax Errors return Exit Code 2
$cases2 = @("--invalid-flag", "--tier=9", "--tier=abc", "--tier=", "--tier", "--filter", "--test", "--tier=1 --bad")
foreach ($c in $cases2) {
    Invoke-Expression "$bin $c > `$null 2>&1"
    if ($LASTEXITCODE -ne 2) { throw "FAILED: '$c' expected exit code 2, got $LASTEXITCODE" }
}
Write-Host "All syntax error cases returned exit code 2: PASS"

# 2. Verify Help and List return Exit Code 0
$cases0 = @("--help", "-h", "--list")
foreach ($c in $cases0) {
    Invoke-Expression "$bin $c > `$null 2>&1"
    if ($LASTEXITCODE -ne 0) { throw "FAILED: '$c' expected exit code 0, got $LASTEXITCODE" }
}
Write-Host "All help/list cases returned exit code 0: PASS"

# 3. Verify Tier Scope for --all and default
$outAll = (& $bin --all) | Out-String
if ($outAll -notmatch "Tiers Active: T1 T2 T3 T4 \r?\n" -or $outAll -match "T5") {
    throw "FAILED: --all output did not isolate Tiers 1-4. Got: $outAll"
}
Write-Host "--all Tier Scope isolation: PASS"

$outDef = (& $bin) | Out-String
if ($outDef -notmatch "Tiers Active: T1 T2 T3 T4 \r?\n" -or $outDef -match "T5") {
    throw "FAILED: Default execution output did not isolate Tiers 1-4. Got: $outDef"
}
Write-Host "Default execution Tier Scope isolation: PASS"

# 4. Verify CTest passes cleanly
ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "CTest failed" }
Write-Host "CTest suite: PASS"
```

### 5.4 Invalidation Conditions
The fix strategy will be invalidated if:
1. An unknown argument or invalid tier number returns exit code `0` or `1`.
2. A valid `--help` or `-h` invocation returns exit code `2`.
3. `--all` or default execution enables `runTier5`.
4. `--tier=5` fails to activate Tier 5 when explicitly requested.

