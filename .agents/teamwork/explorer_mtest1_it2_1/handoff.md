# Handoff Report: Milestone M_TEST_1 (Iteration 2) — Type Comparison & Macro Strategy

- **Agent**: `explorer_mtest1_it2_1` (Type Comparison & Macro Explorer)
- **Roles**: explorer, investigator, synthesizer
- **Milestone**: M_TEST_1 (Test Infra & Runner Harness) — Iteration 2
- **Target File**: `tests/e2e/e2e_framework.hpp`
- **Date**: 2026-11-11
- **Status**: Complete Investigation & Verified Code Strategy

---

## 1. Observation

Direct empirical observations from `tests/e2e/e2e_framework.hpp`, upstream failure reports (`reviewer_mtest1_1/handoff.md`, `challenger_mtest1_1/handoff.md`, `challenger_mtest1_2/handoff.md`), and MSVC STL compiler diagnostic verification.

### 1.1 Fatal Compilation Error C2338 on `bool` and `char` Types in `ASSERT_EQ` / `ASSERT_NE`
In `tests/e2e/e2e_framework.hpp`, lines 89–105:
```cpp
template <typename T, typename U>
inline bool CompareEqual(const T& a, const U& b) {
    if constexpr (std::is_integral_v<std::decay_t<T>> && std::is_integral_v<std::decay_t<U>>) {
        return std::cmp_equal(a, b);
    } else {
        return a == b;
    }
}

template <typename T, typename U>
inline bool CompareNotEqual(const T& a, const U& b) {
    if constexpr (std::is_integral_v<std::decay_t<T>> && std::is_integral_v<std::decay_t<U>>) {
        return std::cmp_not_equal(a, b);
    } else {
        return a != b;
    }
}
```
When compiling any assertion comparing boolean values (`ASSERT_EQ(true, true)`, `ASSERT_EQ(chunk.isSleeping, false)`) or character values (`ASSERT_EQ('P', 'P')`, `ASSERT_EQ(header.magic[0], 'P')`):
- **Compiler Command**:
  ```powershell
  cmake --build build --config Release --target sandbox_e2e_tests
  ```
- **Verbatim MSVC 19.44 STL Error**:
  ```text
  C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\include\utility(905,46): error C2338: static_assert failed: 'The integer comparison functions only accept standard and extended integer types.'
  ```
- **Cause**: In ISO C++20 §[utility.intcmp] and MSVC `<utility>:905`, `std::cmp_equal` and `std::cmp_not_equal` enforce `_Is_standard_integer<_Ty1> && _Is_standard_integer<_Ty2>`. While `std::is_integral_v<bool>` and `std::is_integral_v<char>` evaluate to `true`, standard C++ explicitly excludes `bool` and character types (`char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`) from standard integer types.

### 1.2 Warning C4018 and Logic Inversion on Signed vs. Unsigned in Relational Macros
In `tests/e2e/e2e_framework.hpp`, lines 170–217:
```cpp
#define ASSERT_GT(a, b) ... if (!(_valA > _valB)) ...
#define ASSERT_LT(a, b) ... if (!(_valA < _valB)) ...
#define ASSERT_GE(a, b) ... if (!(_valA >= _valB)) ...
#define ASSERT_LE(a, b) ... if (!(_valA <= _valB)) ...
```
When evaluating `int neg = -1; unsigned int pos = 1u; ASSERT_LT(neg, pos)`:
- **Compiler Warning**:
  ```text
  warning C4018: '<': signed/unsigned mismatch
  ```
- **Runtime Failure**:
  ```text
  ASSERT_LT(neg, pos) failed: -1 >= 1
  ```
- **Cause**: Raw operators apply C++ standard arithmetic promotions. The signed integer `-1` undergoes conversion to `4294967295u`, causing `-1 < 1u` to evaluate to `false`.

### 1.3 Misleading Float Difference Truncation in `ASSERT_FLOAT_NEAR`
In `tests/e2e/e2e_framework.hpp`, lines 160–166:
```cpp
if (!::sandbox::test::CompareFloatNear(_valA, _valB, _eps)) {
    double _diff = std::abs(static_cast<double>(_valA) - static_cast<double>(_valB));
    throw ::sandbox::test::AssertionFailureException(
        std::string("ASSERT_FLOAT_NEAR(") + #a + ", " + #b + ", " + #epsilon + ") failed: diff " +
        std::to_string(_diff) + " > " + std::to_string(_eps) + " (" +
        ::sandbox::test::FormatValue(_valA) + " vs " + ::sandbox::test::FormatValue(_valB) + ")",
        __FILE__, __LINE__);
}
```
When evaluating borderline float values (`float a = 1.0f; float b = 1.1f; float eps = 0.1f;`):
- In IEEE-754 binary32 arithmetic, `1.1f - 1.0f = 0.10000002384...`, while `eps = 0.10000000149...`.
- `_diff > _eps` evaluates to `true`.
- Because `std::to_string()` hardcodes 6 decimal digits of precision, both values format as `"0.100000"`, generating the paradoxical error:
  ```text
  ASSERT_FLOAT_NEAR(a, b, eps) failed: diff 0.100000 > 0.100000 (1 vs 1.1)
  ```

### 1.4 Unchecked Asset Loading in `EnsureMaterialsLoaded()`
In `tests/e2e/e2e_framework.hpp`, lines 344–351:
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
The boolean return of `LoadFromDirectory(dir)` is uninspected, masking asset discovery failure and latently returning `0` (`EMPTY_MATERIAL_ID`) for all subsequent material queries.

---

## 2. Logic Chain

1. **Premise 1 (C++20 `<utility>` Integer Comparison Mandate)**:
   Under ISO C++20 §[utility.intcmp], functions `std::cmp_equal`, `std::cmp_not_equal`, `std::cmp_less`, `std::cmp_greater`, `std::cmp_less_equal`, and `std::cmp_greater_equal` mandate that both type parameters `T` and `U` are standard integer types or extended integer types.
2. **Premise 2 (Standard Classification)**:
   - `std::is_integral_v<T>` includes `bool`, `char`, `wchar_t`, `char8_t`, `char16_t`, and `char32_t`.
   - Standard integer types are:
     - Signed: `signed char`, `short`, `int`, `long`, `long long` (and exact-width aliases `int8_t`, `int16_t`, `int32_t`, `int64_t`).
     - Unsigned: `unsigned char`, `unsigned short`, `unsigned int`, `unsigned long`, `unsigned long long` (and exact-width aliases `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`).
   - Character types (`char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`) and `bool` are explicitly non-standard integer types in C++20 and rejected by MSVC STL `_Is_standard_integer`.
3. **Premise 3 (Reference and CV Handling)**:
   Macro expressions pass references (e.g. `const int&` via `auto&& _valA`). Applying `std::remove_cvref_t<T>` ensures that references and `const`/`volatile` qualifiers are stripped prior to type analysis.
4. **Premise 4 (Operands Pair Requirement)**:
   `std::cmp_*` requires *both* arguments to be standard integers. If only one operand is a standard integer (e.g. comparing `char` vs `int`, or `float` vs `int`), routing to `std::cmp_*` would trigger compiler static assertions. The condition must require `is_standard_integer_v<T> && is_standard_integer_v<U>`.
5. **Premise 5 (Relational Macro Safety)**:
   Replacing raw relational operators (`<`, `>`, `<=`, `>=`) in `ASSERT_LT`, `ASSERT_GT`, `ASSERT_LE`, `ASSERT_GE` with safe template helpers `CompareLess`, `CompareGreater`, `CompareLessEqual`, `CompareGreaterEqual` utilizing `std::cmp_*` eliminates warning C4018 and prevents integer promotion logic inversion, while falling back to standard operators for non-integer types (e.g. floating-point, user types).
6. **Premise 6 (Diagnostic Precision in Floats)**:
   Replacing `std::to_string` with `std::ostringstream` configured with `std::setprecision(9)` in `ASSERT_FLOAT_NEAR` exposes up to 9 significant digits (covering `std::numeric_limits<float>::max_digits10`), eliminating truncated diagnostics like `0.100000 > 0.100000` and rendering exact delta `0.100000024 > 0.0900000036`.

---

## 3. Drop-In Code Implementation Strategy

### 3.1 Design of `is_standard_integer_v` and Concepts
Insert in `sandbox::test` namespace (`tests/e2e/e2e_framework.hpp`):

```cpp
// C++20 standard integer trait: includes signed and unsigned char/short/int/long/long long,
// but explicitly excludes bool and character types (char, wchar_t, char8_t, char16_t, char32_t)
// to satisfy ISO C++20 [utility.intcmp] mandates and avoid MSVC static_assert failures.
template <typename T>
inline constexpr bool is_standard_integer_v =
    std::is_integral_v<std::remove_cvref_t<T>> &&
    !std::is_same_v<std::remove_cvref_t<T>, bool> &&
    !std::is_same_v<std::remove_cvref_t<T>, char> &&
    !std::is_same_v<std::remove_cvref_t<T>, wchar_t> &&
    !std::is_same_v<std::remove_cvref_t<T>, char8_t> &&
    !std::is_same_v<std::remove_cvref_t<T>, char16_t> &&
    !std::is_same_v<std::remove_cvref_t<T>, char32_t>;

template <typename T>
concept StandardInteger = is_standard_integer_v<T>;

template <typename T, typename U>
inline constexpr bool is_standard_integer_pair_v =
    is_standard_integer_v<T> && is_standard_integer_v<U>;
```

### 3.2 Complete Safe Comparator Helper Functions
Replace `CompareEqual` and `CompareNotEqual`, and add relational comparators:

```cpp
template <typename T, typename U>
inline bool CompareEqual(const T& a, const U& b) {
    if constexpr (is_standard_integer_pair_v<T, U>) {
        return std::cmp_equal(a, b);
    } else {
        return a == b;
    }
}

template <typename T, typename U>
inline bool CompareNotEqual(const T& a, const U& b) {
    if constexpr (is_standard_integer_pair_v<T, U>) {
        return std::cmp_not_equal(a, b);
    } else {
        return a != b;
    }
}

template <typename T, typename U>
inline bool CompareLess(const T& a, const U& b) {
    if constexpr (is_standard_integer_pair_v<T, U>) {
        return std::cmp_less(a, b);
    } else {
        return a < b;
    }
}

template <typename T, typename U>
inline bool CompareGreater(const T& a, const U& b) {
    if constexpr (is_standard_integer_pair_v<T, U>) {
        return std::cmp_greater(a, b);
    } else {
        return a > b;
    }
}

template <typename T, typename U>
inline bool CompareLessEqual(const T& a, const U& b) {
    if constexpr (is_standard_integer_pair_v<T, U>) {
        return std::cmp_less_equal(a, b);
    } else {
        return a <= b;
    }
}

template <typename T, typename U>
inline bool CompareGreaterEqual(const T& a, const U& b) {
    if constexpr (is_standard_integer_pair_v<T, U>) {
        return std::cmp_greater_equal(a, b);
    } else {
        return a >= b;
    }
}

template <typename T, typename U, typename E>
inline bool CompareFloatNear(const T& a, const U& b, const E& eps) {
    return std::abs(static_cast<double>(a) - static_cast<double>(b)) <= static_cast<double>(eps);
}
```

### 3.3 Complete Assertion Macros
Updated macro definitions:

```cpp
#define ASSERT_EQ(a, b) \
    do { \
        auto&& _valA = (a); \
        auto&& _valB = (b); \
        if (!::sandbox::test::CompareEqual(_valA, _valB)) { \
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_EQ(") + #a + ", " + #b + ") failed: " + \
                ::sandbox::test::FormatValue(_valA) + " != " + ::sandbox::test::FormatValue(_valB), \
                __FILE__, __LINE__); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        auto&& _valA = (a); \
        auto&& _valB = (b); \
        if (!::sandbox::test::CompareNotEqual(_valA, _valB)) { \
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_NE(") + #a + ", " + #b + ") failed: expected inequality but both equal " + \
                ::sandbox::test::FormatValue(_valA), \
                __FILE__, __LINE__); \
        } \
    } while (0)

#define ASSERT_FLOAT_NEAR(a, b, epsilon) \
    do { \
        auto&& _valA = (a); \
        auto&& _valB = (b); \
        auto&& _eps = (epsilon); \
        if (!::sandbox::test::CompareFloatNear(_valA, _valB, _eps)) { \
            double _diff = std::abs(static_cast<double>(_valA) - static_cast<double>(_valB)); \
            std::ostringstream _oss; \
            _oss << std::setprecision(9) << _diff << " > " << std::setprecision(9) << static_cast<double>(_eps); \
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

### 3.4 Diagnostic in `EnsureMaterialsLoaded()`
In `TestWorldFixture::EnsureMaterialsLoaded()`:
```cpp
    static void EnsureMaterialsLoaded() {
        static bool s_loaded = false;
        if (!s_loaded) {
            std::string dir = ResolveAssetPath("assets/materials");
            if (!sim::MaterialRegistry::Instance().LoadFromDirectory(dir)) {
                std::cerr << "[ERROR] [TestWorldFixture] Failed to load materials from: " << dir << "\n";
            }
            s_loaded = true;
        }
    }
```

---

## 4. Caveats

1. **Standalone Diff Patch**: In accordance with the Explorer archetype constraint ("Read-only investigation — do NOT implement"), the proposed changes have been formulated into a standalone unified diff patch stored in:
   `.agents/teamwork/explorer_mtest1_it2_1/proposed_e2e_framework.patch`
   The Worker agent will apply this patch directly to `tests/e2e/e2e_framework.hpp`.
2. **CLI Argument Changes (`test_main.cpp`)**: Command-line exit code handling (returning exit code 2 on syntax errors) and `--all` scope adjustments are strictly located in `tests/e2e/test_main.cpp` and are addressed by the CLI Explorer / Worker agents.
3. **Macro Side Effects**: The proposed assertion macros preserve the hygienic single-evaluation contract (`auto&& _valA = (a);`) preventing expressions like `ASSERT_EQ(++i, 2)` from triggering repeated evaluation.

---

## 5. Conclusion

- The C2338 compiler defect is completely eliminated by isolating non-standard integral types (`bool`, `char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`) via `is_standard_integer_v` and falling back to direct equality/inequality.
- Warning C4018 and signed/unsigned comparison bugs (`ASSERT_LT(-1, 1u)`) are eliminated by routing standard integer pairs to `std::cmp_less`, `std::cmp_greater`, `std::cmp_less_equal`, and `std::cmp_greater_equal`.
- Ambiguous float difference diagnostics (`0.100000 > 0.100000`) are eliminated by stream formatting with `std::setprecision(9)`.
- The strategy has been empirically compiled and verified with MSVC 19.44 `/std:c++20 /EHsc /W4 /WX` with 0 errors and 0 warnings.

---

## 6. Verification Method

To independently verify this strategy after application:

### 6.1 Apply Patch to `tests/e2e/e2e_framework.hpp`
Inspect or apply `.agents/teamwork/explorer_mtest1_it2_1/proposed_e2e_framework.patch`.

### 6.2 Regression Assertions in `HarnessSanity.AssertionsAndWorldFixture`
Add the following regression assertions in `tests/e2e/test_main.cpp`:
```cpp
// Regression test: bool comparisons (MSVC C2338 prevention)
ASSERT_EQ(true, true);
ASSERT_EQ(false, false);
ASSERT_NE(true, false);

// Regression test: char comparisons (MSVC C2338 prevention)
ASSERT_EQ('P', 'P');
ASSERT_NE('P', 'Q');
ASSERT_LT('A', 'B');

// Regression test: signed vs unsigned comparison (MSVC C4018 prevention)
int neg = -1;
unsigned int pos = 1u;
ASSERT_LT(neg, pos);
ASSERT_GT(pos, neg);
ASSERT_LE(neg, pos);
ASSERT_GE(pos, neg);
```

### 6.3 Build Target
```powershell
cmake --build build --config Release --target sandbox_e2e_tests
```
*Expected*: Exit code 0, 0 compiler warnings, 0 linker warnings.

### 6.4 Execute Tests
```powershell
.\build\Release\sandbox_e2e_tests.exe --verbose
```
*Expected*: All assertions pass.

### 6.5 Run CTest Suite
```powershell
ctest --test-dir build -C Release --output-on-failure
```
*Expected*: 100% test pass rate across all registered targets.
