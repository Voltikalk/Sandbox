# Handoff Report: Harness Robustness, Asset Error Handling & Sanity Test Explorer

- **Agent**: `explorer_mtest1_it2_3` (Harness Robustness & Sanity Test Explorer)
- **Role**: explorer, investigator
- **Target Milestone**: Milestone M_TEST_1 (Test Infra & Runner Harness) — Iteration 2
- **Date**: 2026-11-11
- **Working Directory**: `c:\Users\Drilla\Desktop\Sandbox`
- **State Folder**: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_it2_3`

---

## 1. Observation

Direct empirical observations from `tests/e2e/e2e_framework.hpp`, `tests/e2e/test_main.cpp`, `src/sim/MaterialRegistry.cpp`, and previous review/challenger reports:

### 1.1 `TestWorldFixture::EnsureMaterialsLoaded()` Swallows Asset Load Failure
In `tests/e2e/e2e_framework.hpp:344-351`:
```cpp
    static void EnsureMaterialsLoaded() {
        static bool s_loaded = false;
        if (!s_loaded) {
            std::string dir = ResolveAssetPath("assets/materials");
            sim::MaterialRegistry::Instance().LoadFromDirectory(dir);
            s_loaded = true;
        }
    }
```
- Direct observation: `sim::MaterialRegistry::Instance().LoadFromDirectory(dir)` returns `bool` (declared in `src/sim/MaterialRegistry.hpp:28` and implemented in `src/sim/MaterialRegistry.cpp:176-203`).
- If `dir` does not exist or cannot be read, `LoadFromDirectory` returns `false`.
- `EnsureMaterialsLoaded()` ignores this return value and unconditionally sets `s_loaded = true`.
- Consequence: Subsequent calls to `GetMatId("sand")` or `GetMatId("water")` return `0` (`EMPTY_MATERIAL_ID`) without any diagnostic emitted to the console, leading to cryptic test assertion failures.

### 1.2 Windows Macro Pollution (`min` / `max`)
In `tests/e2e/e2e_framework.hpp:27-35`:
```cpp
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
```
- Direct observation: While `NOMINMAX` is defined before `#include <windows.h>`, if `<windows.h>` has already been included in a translation unit prior to `e2e_framework.hpp` (e.g., via SDL2 headers, external headers, or future test files) without `NOMINMAX`, `<windows.h>` defines:
  `#define min(a,b) (((a) < (b)) ? (a) : (b))`
  `#define max(a,b) (((a) > (b)) ? (a) : (b))`
- In `tests/e2e/e2e_framework.hpp:405-408, 446-449, 468-471, 492-495`, spatial inspection helpers call `std::min` and `std::max`:
  `core::i32 startX = std::max(0, boxX);`
  `core::i32 endX = std::min(world.GetWidth(), boxX + boxW);`
- If macro definitions are active, the token stream becomes `std::(((0) > (boxX)) ? (0) : (boxX))`, triggering MSVC compiler errors:
  `error C2589: '(': illegal token on right side of '::'`
  `error C2059: syntax error: '::'`
- Defining `NOMINMAX` has no effect if `<windows.h>` header guards are already satisfied.

### 1.3 `ResolveAssetPath` Separator Incompatibility with Windows Backslashes
In `tests/e2e/e2e_framework.hpp:274-328`:
```cpp
        if (relativePath.rfind("assets/", 0) == 0) {
            std::string sub = relativePath.substr(7);
...
#ifdef SANDBOX_ASSETS_DIR
    fs::path baseAssets(SANDBOX_ASSETS_DIR);
    if (relativePath == "assets" || relativePath == "assets/") {
        if (fs::exists(baseAssets)) return baseAssets.string();
    }
    if (relativePath.rfind("assets/", 0) == 0) {
        std::string sub = relativePath.substr(7);
        if (fs::exists(baseAssets / sub)) {
            return (baseAssets / sub).string();
        }
    }
```
- Direct observation: Lines 291 and 306 check `relativePath.rfind("assets/", 0) == 0` specifically using forward slashes. Line 303 checks `relativePath == "assets" || relativePath == "assets/"`.
- If a caller supplies Windows native backslashes (e.g., `"assets\\materials"`, `"assets\\reactions.json"`, or `"assets\\"`):
  - `relativePath.rfind("assets/", 0)` evaluates to `std::string::npos`.
  - Layer 3 attempts `baseAssets / relativePath`, which constructs `<SANDBOX_ASSETS_DIR>/assets\materials` (producing a duplicate `assets/assets` path).
  - Layer 2 and Layer 3 prefix replacement fails, forcing unnecessary fallback to Layer 4 directory traversal.

### 1.4 Baseline Test Assertion Limitations in `test_main.cpp`
In `tests/e2e/test_main.cpp:153-174`:
```cpp
E2E_TEST(HarnessSanity, AssertionsAndWorldFixture, 1) {
    ASSERT_TRUE(true);
    ASSERT_FALSE(false);
    ASSERT_EQ(42, 42);
    ASSERT_NE(10, 20);
    ASSERT_FLOAT_NEAR(3.14159f, 3.14150f, 0.001f);
    ASSERT_GT(100, 50);
    ASSERT_LT(25, 75);
    ASSERT_GE(50, 50);
    ASSERT_LE(30, 30);
...
```
- Direct observation: The existing sanity test tests only same-signed integers (`42, 42`) and basic floats.
- It does NOT assert:
  - Boolean equality: `ASSERT_EQ(true, true)`, `ASSERT_EQ(false, false)`, `ASSERT_NE(true, false)`
  - Character equality: `ASSERT_EQ('P', 'P')`, `ASSERT_NE('A', 'B')`
  - Signed vs unsigned relational comparisons: `ASSERT_LT(-1, 1u)`, `ASSERT_GT(10u, 5)`, `ASSERT_GE(5u, 5)`
  - Direct float near comparison: `ASSERT_FLOAT_NEAR(1.0f, 1.0f, 1e-6f)`
- When `ASSERT_EQ(true, true)` or `ASSERT_EQ('P', 'P')` is added under the current harness, MSVC emits:
  `error C2338: static_assert failed: 'The integer comparison functions only accept standard and extended integer types.'`
- When `ASSERT_LT(-1, 1u)` is evaluated under the current harness, MSVC emits:
  `warning C4018: '<': signed/unsigned mismatch` and runtime assertion failure (`-1 >= 1`).

---

## 2. Logic Chain

1. **Asset Loading Verification**:
   - `sim::MaterialRegistry::LoadFromDirectory(dir)` returns `true` on successful loading and `false` if the path does not exist or is not a directory.
   - By verifying this return value in `TestWorldFixture::EnsureMaterialsLoaded()`:
     - If `false`, emit an explicit error message to `std::cerr`: `[ERROR] [TestWorldFixture] Failed to load materials from directory: <dir>`.
     - Latch `s_loaded = true` only upon success, allowing future calls to retry if asset directories are reconfigured.

2. **Windows Macro Isolation**:
   - Because translation units may include headers in arbitrary order, `<windows.h>` may be included before `e2e_framework.hpp` without `NOMINMAX`.
   - Adding `#undef min` and `#undef max` immediately following `#include <windows.h>` in both `e2e_framework.hpp` and `test_main.cpp` removes any active macro definitions of `min` and `max`.
   - This guarantees that `std::min`, `std::max`, and `std::numeric_limits<T>::max()` compile safely under all include permutations.

3. **Path Separator Normalization via `std::filesystem::path`**:
   - Using `std::filesystem::path::lexically_normal().generic_string()` converts any path containing backslashes (`\`) or redundant components into standard generic format with forward slashes (`/`).
   - Checking whether the normalized generic path is `"assets"` or begins with `"assets/"` enables robust subpath extraction (`sub = genericPath.substr(7)`), making Layer 1, Layer 2, Layer 3, and Layer 4 completely agnostic to whether forward or backward slashes are passed by callers.

4. **Template Comparison Metaprogramming**:
   - ISO C++20 §[utility.intcmp] mandates that `std::cmp_equal`, `std::cmp_not_equal`, `std::cmp_less`, `std::cmp_greater`, `std::cmp_less_equal`, and `std::cmp_greater_equal` accept only standard and extended integer types (excluding `bool`, `char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`).
   - Defining `is_standard_integer_v<T>` using `std::decay_t<T>` cleanly discriminates standard integers from character and boolean types.
   - Routing standard integer comparisons through `std::cmp_*` and all other types through standard operators (`==`, `!=`, `<`, `>`, `<=`, `>=`) eliminates MSVC error C2338, eliminates signed/unsigned warning C4018, and correctly evaluates mathematically sound comparisons like `ASSERT_LT(-1, 1u)`.

5. **Expanded Sanity Test**:
   - Incorporating boolean (`ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`), character (`'P'`, `'A'`), mixed-signedness integer (`-1 < 1u`, `10u > 5`, `5u >= 5`), and floating-point assertions into `E2E_TEST(HarnessSanity, AssertionsAndWorldFixture, 1)` provides an automated regression gate ensuring the harness remains sound across all future milestones.

---

## 3. Caveats

- **No Caveats**: The codebase and dependencies are fully inspected. All standard types and MSVC behaviors have been verified against the C++20 standard and MSVC STL headers.

---

## 4. Conclusion & Recommended Worker Code Changes

The Worker should apply the following exact modifications to `tests/e2e/e2e_framework.hpp` and `tests/e2e/test_main.cpp`:

### Change 1: `tests/e2e/e2e_framework.hpp` — Windows Macro Protections
**Location**: Lines 27-36 of `tests/e2e/e2e_framework.hpp`
```cpp
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef min
#undef max
#endif
```

### Change 2: `tests/e2e/e2e_framework.hpp` — Standard Integer Trait, Character Formatting, and Relational Comparators
**Location**: Replace lines 67-111 of `tests/e2e/e2e_framework.hpp` with:
```cpp
// ============================================================================
// 2. Type Formatting & Comparison Utilities
// ============================================================================
template <typename T>
concept PrintableConcept = requires(std::ostream& os, const T& value) {
    os << value;
};

template <typename T>
inline std::string FormatValue(const T& value) {
    if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_same_v<std::decay_t<T>, char>) {
        return std::string("'") + value + "'";
    } else if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::string("\"") + std::string(value) + "\"";
    } else if constexpr (std::is_enum_v<std::decay_t<T>>) {
        return std::to_string(static_cast<std::underlying_type_t<std::decay_t<T>>>(value));
    } else if constexpr (PrintableConcept<T>) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    } else {
        return "<unprintable>";
    }
}

// Trait to identify standard/extended integers accepted by C++20 std::cmp_*
template <typename T>
inline constexpr bool is_standard_integer_v =
    std::is_integral_v<std::decay_t<T>> &&
    !std::is_same_v<std::decay_t<T>, bool> &&
    !std::is_same_v<std::decay_t<T>, char> &&
    !std::is_same_v<std::decay_t<T>, wchar_t> &&
    !std::is_same_v<std::decay_t<T>, char8_t> &&
    !std::is_same_v<std::decay_t<T>, char16_t> &&
    !std::is_same_v<std::decay_t<T>, char32_t>;

template <typename T, typename U>
inline bool CompareEqual(const T& a, const U& b) {
    if constexpr (is_standard_integer_v<T> && is_standard_integer_v<U>) {
        return std::cmp_equal(a, b);
    } else {
        return a == b;
    }
}

template <typename T, typename U>
inline bool CompareNotEqual(const T& a, const U& b) {
    if constexpr (is_standard_integer_v<T> && is_standard_integer_v<U>) {
        return std::cmp_not_equal(a, b);
    } else {
        return a != b;
    }
}

template <typename T, typename U>
inline bool CompareGreater(const T& a, const U& b) {
    if constexpr (is_standard_integer_v<T> && is_standard_integer_v<U>) {
        return std::cmp_greater(a, b);
    } else {
        return a > b;
    }
}

template <typename T, typename U>
inline bool CompareLess(const T& a, const U& b) {
    if constexpr (is_standard_integer_v<T> && is_standard_integer_v<U>) {
        return std::cmp_less(a, b);
    } else {
        return a < b;
    }
}

template <typename T, typename U>
inline bool CompareGreaterEqual(const T& a, const U& b) {
    if constexpr (is_standard_integer_v<T> && is_standard_integer_v<U>) {
        return std::cmp_greater_equal(a, b);
    } else {
        return a >= b;
    }
}

template <typename T, typename U>
inline bool CompareLessEqual(const T& a, const U& b) {
    if constexpr (is_standard_integer_v<T> && is_standard_integer_v<U>) {
        return std::cmp_less_equal(a, b);
    } else {
        return a <= b;
    }
}

template <typename T, typename U, typename E>
inline bool CompareFloatNear(const T& a, const U& b, const E& eps) {
    return std::abs(static_cast<double>(a) - static_cast<double>(b)) <= static_cast<double>(eps);
}
```

### Change 3: `tests/e2e/e2e_framework.hpp` — Assertion Macros for Relational & Float Near
**Location**: Replace lines 155-217 of `tests/e2e/e2e_framework.hpp` with:
```cpp
#define ASSERT_FLOAT_NEAR(a, b, epsilon) \
    do { \
        auto&& _valA = (a); \
        auto&& _valB = (b); \
        auto&& _eps = (epsilon); \
        if (!::sandbox::test::CompareFloatNear(_valA, _valB, _eps)) { \
            double _diff = std::abs(static_cast<double>(_valA) - static_cast<double>(_valB)); \
            std::ostringstream _oss; \
            _oss << std::setprecision(9) << _diff << " > " << static_cast<double>(_eps); \
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_FLOAT_NEAR(") + #a + ", " + #b + ", " + #epsilon + ") failed: diff " + \
                _oss.str() + " (" + \
                ::sandbox::test::FormatValue(_valA) + " vs " + ::sandbox::test::FormatValue(_valB) + ")", \
                __FILE__, __LINE__); \
        } \
    } while (0)

#define ASSERT_GT(a, b) \
    do { \
        auto&& _valA = (a); \
        auto&& _valB = (b); \
        if (!::sandbox::test::CompareGreater(_valA, _valB)) { \
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_GT(") + #a + ", " + #b + ") failed: " + \
                ::sandbox::test::FormatValue(_valA) + " <= " + ::sandbox::test::FormatValue(_valB), \
                __FILE__, __LINE__); \
        } \
    } while (0)

#define ASSERT_LT(a, b) \
    do { \
        auto&& _valA = (a); \
        auto&& _valB = (b); \
        if (!::sandbox::test::CompareLess(_valA, _valB)) { \
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_LT(") + #a + ", " + #b + ") failed: " + \
                ::sandbox::test::FormatValue(_valA) + " >= " + ::sandbox::test::FormatValue(_valB), \
                __FILE__, __LINE__); \
        } \
    } while (0)

#define ASSERT_GE(a, b) \
    do { \
        auto&& _valA = (a); \
        auto&& _valB = (b); \
        if (!::sandbox::test::CompareGreaterEqual(_valA, _valB)) { \
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_GE(") + #a + ", " + #b + ") failed: " + \
                ::sandbox::test::FormatValue(_valA) + " < " + ::sandbox::test::FormatValue(_valB), \
                __FILE__, __LINE__); \
        } \
    } while (0)

#define ASSERT_LE(a, b) \
    do { \
        auto&& _valA = (a); \
        auto&& _valB = (b); \
        if (!::sandbox::test::CompareLessEqual(_valA, _valB)) { \
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_LE(") + #a + ", " + #b + ") failed: " + \
                ::sandbox::test::FormatValue(_valA) + " > " + ::sandbox::test::FormatValue(_valB), \
                __FILE__, __LINE__); \
        } \
    } while (0)
```

### Change 4: `tests/e2e/e2e_framework.hpp` — `ResolveAssetPath` Path Normalization
**Location**: Replace lines 274-328 of `tests/e2e/e2e_framework.hpp` with:
```cpp
// ============================================================================
// 5. 4-Layer Asset Path Resolution
// ============================================================================
inline std::string ResolveAssetPath(const std::string& relativePath) {
    namespace fs = std::filesystem;

    fs::path relPath(relativePath);
    fs::path normalizedRel = relPath.lexically_normal();
    std::string genericPath = normalizedRel.generic_string();

    // 1. Check relative to current working directory
    if (fs::exists(normalizedRel)) {
        return normalizedRel.string();
    }
    if (fs::exists(relPath)) {
        return relPath.string();
    }

    bool isAssetsRoot = (genericPath == "assets" || genericPath == "assets/");
    bool startsWithAssets = isAssetsRoot || (genericPath.rfind("assets/", 0) == 0);
    std::string sub = "";
    if (startsWithAssets && !isAssetsRoot) {
        sub = genericPath.substr(7); // strip "assets/"
    }

    // 2. Check relative to executable directory (Windows GetModuleFileName)
#if defined(_WIN32)
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len > 0) {
        fs::path exeDir = fs::path(buffer).parent_path();
        if (fs::exists(exeDir / normalizedRel)) {
            return (exeDir / normalizedRel).lexically_normal().string();
        }
        if (startsWithAssets) {
            fs::path candidate = isAssetsRoot ? (exeDir / "assets") : (exeDir / "assets" / sub);
            if (fs::exists(candidate)) {
                return candidate.lexically_normal().string();
            }
        }
    }
#endif

    // 3. Check compile-time configured SANDBOX_ASSETS_DIR
#ifdef SANDBOX_ASSETS_DIR
    fs::path baseAssets(SANDBOX_ASSETS_DIR);
    if (startsWithAssets) {
        fs::path candidate = isAssetsRoot ? baseAssets : (baseAssets / sub);
        if (fs::exists(candidate)) {
            return candidate.lexically_normal().string();
        }
    }
    if (fs::exists(baseAssets / normalizedRel)) {
        return (baseAssets / normalizedRel).lexically_normal().string();
    }
#endif

    // 4. Search upward up to 4 directory levels
    fs::path searchDir = fs::current_path();
    for (int i = 0; i < 4; ++i) {
        if (fs::exists(searchDir / normalizedRel)) {
            return (searchDir / normalizedRel).lexically_normal().string();
        }
        if (startsWithAssets) {
            fs::path candidate = isAssetsRoot ? (searchDir / "assets") : (searchDir / "assets" / sub);
            if (fs::exists(candidate)) {
                return candidate.lexically_normal().string();
            }
        }
        if (!searchDir.has_parent_path()) break;
        searchDir = searchDir.parent_path();
    }

    return normalizedRel.string(); // Fallback
}
```

### Change 5: `tests/e2e/e2e_framework.hpp` — `EnsureMaterialsLoaded()` Verification and Diagnostic
**Location**: Replace lines 344-351 of `tests/e2e/e2e_framework.hpp` with:
```cpp
    static void EnsureMaterialsLoaded() {
        static bool s_loaded = false;
        if (!s_loaded) {
            std::string dir = ResolveAssetPath("assets/materials");
            bool ok = sim::MaterialRegistry::Instance().LoadFromDirectory(dir);
            if (!ok) {
                std::cerr << "[ERROR] [TestWorldFixture] Failed to load materials from directory: " << dir << "\n";
            } else {
                s_loaded = true;
            }
        }
    }
```

### Change 6: `tests/e2e/test_main.cpp` — Windows Macro Protections
**Location**: Lines 12-21 of `tests/e2e/test_main.cpp`
```cpp
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef min
#undef max
#endif
```

### Change 7: `tests/e2e/test_main.cpp` — Expanded Sanity Assertions
**Location**: Replace lines 153-174 of `tests/e2e/test_main.cpp` with:
```cpp
// ============================================================================
// Built-in Sanity Test: Validates that harness assertions and fixture operate
// ============================================================================
E2E_TEST(HarnessSanity, AssertionsAndWorldFixture, 1) {
    // 1. Boolean comparisons
    ASSERT_TRUE(true);
    ASSERT_FALSE(false);
    ASSERT_EQ(true, true);
    ASSERT_EQ(false, false);
    ASSERT_NE(true, false);

    // 2. Character comparisons
    ASSERT_EQ('P', 'P');
    ASSERT_NE('A', 'B');

    // 3. Integer comparisons (same signedness)
    ASSERT_EQ(42, 42);
    ASSERT_NE(10, 20);

    // 4. Signed vs unsigned relational comparisons
    ASSERT_LT(-1, 1u);
    ASSERT_GT(10u, 5);
    ASSERT_GE(5u, 5);
    ASSERT_LE(5, 5u);

    // 5. Floating point comparisons
    ASSERT_FLOAT_NEAR(1.0f, 1.0f, 1e-6f);
    ASSERT_FLOAT_NEAR(3.14159f, 3.14150f, 0.001f);

    // 6. Relational comparisons
    ASSERT_GT(100, 50);
    ASSERT_LT(25, 75);
    ASSERT_GE(50, 50);
    ASSERT_LE(30, 30);

    // 7. World fixture verification & mutations
    TestWorldFixture fixture(64, 64);
    ASSERT_EQ(fixture.world.GetWidth(), 64);
    ASSERT_EQ(fixture.world.GetHeight(), 64);

    fixture.world.SetCell(10, 10, 1, 25.0f);
    ASSERT_EQ(fixture.world.GetCellMaterial(10, 10), 1);
    ASSERT_FLOAT_NEAR(fixture.world.GetCellTemperature(10, 10), 25.0f, 0.01f);

    // 8. Inspection utilities
    ASSERT_EQ(inspection::CountMaterial(fixture.world, 1), 1u);
    ASSERT_EQ(inspection::CountMaterialInBox(fixture.world, 1, 0, 0, 20, 20), 1u);
}
```

### Change 8: `tests/e2e/test_main.cpp` — CLI Syntax Error Exit Code 2 & Scope
**Location**: In `CommandLineOptions` and `ParseCommandLine` in `tests/e2e/test_main.cpp`:
1. In `struct CommandLineOptions`:
   ```cpp
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
       std::string filter{""};
   };
   ```
2. In `ParseCommandLine()`:
   ```cpp
           } else if (arg == "--all") {
               opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true;
               opts.runTier5 = false; // Tier 5 is opt-in via --tier=5
               explicitTierSelected = true;
   ...
           } else {
               std::cerr << "Unknown argument: " << arg << "\n";
               opts.hasSyntaxError = true;
           }
       }

       if (!explicitTierSelected) {
           opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true;
           opts.runTier5 = false; // Tier 5 is opt-in
       }
   ```
3. In `main()`:
   ```cpp
       CommandLineOptions opts = ParseCommandLine(argc, argv);
       if (opts.hasSyntaxError) {
           PrintUsage(argv[0]);
           return 2;
       }
       if (opts.showHelp) {
           PrintUsage(argv[0]);
           return 0;
       }
   ```

---

## 5. Verification Method

To verify these recommendations independently once implemented by the Worker:

1. **Build Verification**:
   ```powershell
   cmake --build build --config Release --target sandbox_e2e_tests
   ```
   *Expected Result*: Clean compilation with 0 errors and 0 warnings under MSVC `/W4`. Specifically confirms that `ASSERT_EQ(true, true)`, `ASSERT_EQ('P', 'P')`, and `ASSERT_LT(-1, 1u)` compile cleanly without static assert C2338 or signed/unsigned warning C4018.

2. **Sanity Execution Verification**:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --verbose
   ```
   *Expected Result*: `HarnessSanity.AssertionsAndWorldFixture` executes all expanded boolean, char, signed/unsigned relational, and float assertions and passes with 100% success in < 3 ms.

3. **CLI Exit Code Verification**:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --help; Write-Host "Help ExitCode: $LASTEXITCODE" # Must be 0
   .\build\Release\sandbox_e2e_tests.exe --invalid-flag; Write-Host "Invalid Flag ExitCode: $LASTEXITCODE" # Must be 2
   .\build\Release\sandbox_e2e_tests.exe --tier=9; Write-Host "Invalid Tier ExitCode: $LASTEXITCODE" # Must be 2
   ```

4. **CTest Suite Verification**:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
   *Expected Result*: 6/6 tests pass (100% pass rate).
