# Handoff Report: Milestone M_TEST_1 (Test Framework & Runner Harness Adversarial Review)

- **Agent**: `challenger_mtest1_1` (Test Framework Challenger 1)
- **Role**: critic, specialist
- **Milestone**: M_TEST_1 (Test Infra & Runner Harness)
- **Date**: 2026-11-11
- **Verdict**: `CHANGES_REQUESTED`

---

## 1. Observation

Direct empirical observations from inspecting `tests/e2e/e2e_framework.hpp` and `tests/e2e/test_main.cpp`, compiling targeted challenge test cases, and analyzing runtime outputs with MSVC 19.44 (Visual Studio 2022 BuildTools) under CMake 4.3.2:

### 1.1 Fatal Compilation Failure: `ASSERT_EQ` and `ASSERT_NE` on `bool` and `char` Types
In `c:\Users\Drilla\Desktop\Sandbox\tests\e2e\e2e_framework.hpp`, lines 89–105:
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

When invoking `ASSERT_EQ(true, true)`, `ASSERT_EQ(false, false)`, `ASSERT_NE(true, false)`, or `ASSERT_EQ('a', 'a')`:
- **Command**:
  ```powershell
  cmake --build build --config Release --target sandbox_e2e_tests
  ```
- **Verbatim MSBuild Error Output**:
  ```text
  C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\include\utility(905,46): error C2338: static_assert failed: 'The integer comparison functions only accept standard and extended integer types.' [C:\Users\Drilla\Desktop\Sandbox\build\tests\sandbox_e2e_tests.vcxproj]
    (компиляция исходного файла "../../tests/e2e/tier_empirical_stress.cpp")
        C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\include\utility(905,46):
        контекст создания экземпляра шаблона (сначала самый старый)
            C:\Users\Drilla\Desktop\Sandbox\tests\e2e\tier_empirical_stress.cpp(10,5):
            выполняется компиляция ссылки на экземпляр шаблон функции "bool sandbox::test::CompareEqual<bool,bool>(const T &,const U &)"
            with
            [
                T=bool,
                U=bool
            ]
            C:\Users\Drilla\Desktop\Sandbox\tests\e2e\e2e_framework.hpp(92,21):
            выполняется компиляция ссылки на экземпляр шаблон функции "bool std::cmp_equal<T,U>(const _Ty1,const _Ty2) noexcept"
            with
            [
                T=bool,
                U=bool,
                _Ty1=bool,
                _Ty2=bool
            ]
  ```
- **Same Error on `char` (`ASSERT_EQ('a', 'a')`)**:
  ```text
  C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\include\utility(905,46): error C2338: static_assert failed: 'The integer comparison functions only accept standard and extended integer types.' [C:\Users\Drilla\Desktop\Sandbox\build\tests\sandbox_e2e_tests.vcxproj]
  ```

### 1.2 Signed vs. Unsigned Inequality Inversion in `ASSERT_LT` / `ASSERT_GT`
In `tests/e2e/e2e_framework.hpp`, lines 170–217:
```cpp
#define ASSERT_GT(a, b) ... if (!(_valA > _valB)) ...
#define ASSERT_LT(a, b) ... if (!(_valA < _valB)) ...
#define ASSERT_GE(a, b) ... if (!(_valA >= _valB)) ...
#define ASSERT_LE(a, b) ... if (!(_valA <= _valB)) ...
```
When comparing `int s = -1; unsigned int u = 1;` with `ASSERT_LT(s, u)`:
- **Verbatim Compiler Warning**:
  ```text
  C:\Users\Drilla\Desktop\Sandbox\tests\e2e\tier_empirical_stress.cpp(9,5): warning C4018: <: несоответствие типов со знаком и без знака
  ```
- **Runtime Failure**:
  ```text
  [  FAILED  ] [EmpiricalStress.InequalitySignedUnsigned] (Tier 5, 0.05 ms)
               C:\Users\Drilla\Desktop\Sandbox\tests\e2e\tier_empirical_stress.cpp:9: ASSERT_LT(s, u) failed: -1 >= 1
  ```
Because raw relational operators undergo C++ integer promotion, `-1` becomes `4294967295u`, failing mathematically valid assertions like `-1 < 1u`.

### 1.3 Truncated Precision Display in `ASSERT_FLOAT_NEAR`
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
When testing boundary floating differences, e.g. `float a = 1.0f; float b = 1.1f; float eps = 0.1f;`:
- In binary32 float, `1.1f - 1.0f = 0.10000002384...`, while `eps = 0.10000000149...`.
- `_diff > _eps` evaluates to true, throwing an assertion failure.
- `std::to_string` formats both numbers with 6 digits of decimal precision, producing:
  ```text
  [  FAILED  ] [EmpiricalStress.FloatNearBoundaries] (Tier 5, 0.06 ms)
               ASSERT_FLOAT_NEAR(a, b, eps) failed: diff 0.100000 > 0.100000 (1 vs 1.1)
  ```
The report bewilders developers by claiming `0.100000 > 0.100000`.

### 1.4 Robust Exception Trapping in Test Runner
In `tests/e2e/test_main.cpp`, lines 240–254:
- When a test throws `AssertionFailureException`, the runner catches it cleanly, increments `totalFailed`, records failure diagnostics, and continues running remaining tests without aborting or crashing.
- When a test throws `std::exception` (e.g. `std::runtime_error`), the runner catches it, records `Unhandled std::exception: <msg>`, and increments `totalCrashed`.
- When a test throws a non-standard exception (`catch (...)`), the runner catches it, records `Unhandled non-standard exception / crash`, and increments `totalCrashed`.
- The runner summarizes all failures at termination and returns non-zero exit code 1.

---

## 2. Logic Chain

1. **Root Cause Analysis of C2338 Error**:
   - In ISO C++20 §[utility.intcmp], `std::cmp_equal` and `std::cmp_not_equal` specify:
     > *Mandates*: Both `T` and `U` are standard integer types or extended integer types (neither is `bool`, `char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
   - However, in C++ type traits (§[meta.unary.cat]), `std::is_integral_v<bool>` and `std::is_integral_v<char>` are unconditionally `true`.
   - In `e2e_framework.hpp`, lines 91 and 100, the condition `if constexpr (std::is_integral_v<std::decay_t<T>> && std::is_integral_v<std::decay_t<U>>)` routes `bool` and `char` comparisons into `std::cmp_equal`/`std::cmp_not_equal`.
   - MSVC STL enforces the standard mandate via `static_assert`, causing an immediate compilation error C2338 whenever a test compares `bool` (e.g. `ASSERT_EQ(world.IsDirty(), true)` or `ASSERT_EQ(flag, false)`) or `char` (e.g. `ASSERT_EQ(header.magic[0], 'P')`).

2. **Downstream Blast Radius**:
   - In upcoming milestone M_TEST_2 (Tier 1 Feature Coverage), tests must verify chunk state flags, boolean returns from `world.SetCell(...)`, and magic byte headers (`SaveHeader.magic`).
   - If uncorrected, every test suite asserting boolean or character values will fail compilation, blocking M_TEST_2 completely.

3. **Inconsistency in Relational Comparisons**:
   - `ASSERT_EQ` and `ASSERT_NE` attempted safe signed/unsigned handling, but `ASSERT_GT`, `ASSERT_LT`, `ASSERT_GE`, and `ASSERT_LE` use raw relational operators (`<`, `>`, `<=`, `>=`), causing sign mismatch warnings (`warning C4018`) and mathematically inverted comparisons (e.g. `ASSERT_LT(-1, 1u)` failing).

4. **Clarity of Floating Point Failure Output**:
   - While floating point representation differences are an expected reality of IEEE-754 arithmetic, formatting `_diff` and `_eps` with `std::to_string` masks the 7th-decimal difference, generating confusing reports like `0.100000 > 0.100000`.

5. **Empirically Verified Solution**:
   - A standard integer helper trait cleanly separates standard integers from `bool`, `char`, and wide char types:
     ```cpp
     template <typename T>
     inline constexpr bool is_standard_integer_v = std::is_integral_v<T> &&
         !std::is_same_v<T, bool> &&
         !std::is_same_v<T, char> &&
         !std::is_same_v<T, wchar_t> &&
         !std::is_same_v<T, char8_t> &&
         !std::is_same_v<T, char16_t> &&
         !std::is_same_v<T, char32_t>;
     ```
   - Empirically tested and verified: replacing `std::is_integral_v` with `is_standard_integer_v` allows `ASSERT_EQ(true, true)` and `ASSERT_EQ('a', 'a')` to compile cleanly and pass with 0 errors and 0 warnings.
   - Similarly, adding safe inequality comparators (`std::cmp_greater`, `std::cmp_less`, etc.) for standard integers resolves warning C4018 and logic inversions.

---

## 3. Caveats

1. **Floating Point Rounding**: Tests using `ASSERT_FLOAT_NEAR` should choose an epsilon that accounts for binary float rounding errors (e.g. `0.1001f` rather than exact `0.1f` when comparing fractional decimal increments like `1.1f - 1.0f`).
2. **Current Baseline**: The current baseline repository builds and passes 6/6 CTest tests only because the existing sanity test in `test_main.cpp` happened to compare integers (`ASSERT_EQ(42, 42)`) rather than booleans or chars.

---

## 4. Conclusion

**Verdict: `CHANGES_REQUESTED`**

Milestone M_TEST_1 contains a critical compile-time defect in `tests/e2e/e2e_framework.hpp` that will block downstream test tiers:
1. `CompareEqual` and `CompareNotEqual` violate C++20 `std::cmp_equal` mandates by passing `bool` and `char` types, triggering MSVC error C2338.
2. `ASSERT_GT`, `ASSERT_LT`, `ASSERT_GE`, and `ASSERT_LE` do not safely handle mixed signed/unsigned comparisons, causing compiler warnings C4018 and runtime assertion failures.
3. `ASSERT_FLOAT_NEAR` diagnostics use `std::to_string`, truncating precision and producing misleading failure output (`0.100000 > 0.100000`).

The test runner harness (`tests/e2e/test_main.cpp`) itself is robust and cleanly traps exceptions without crashing. However, `tests/e2e/e2e_framework.hpp` requires the straightforward adjustments identified above before Milestone M_TEST_2 can commence.

---

## 5. Verification Method

### 5.1 Reproducing the C2338 Static Assertion Failure
1. In `tests/e2e/test_main.cpp`, add the following line inside `HarnessSanity.AssertionsAndWorldFixture` (line 156):
   ```cpp
   ASSERT_EQ(true, true);
   ASSERT_EQ('a', 'a');
   ```
2. Run build:
   ```powershell
   cmake --build build --config Release --target sandbox_e2e_tests
   ```
3. *Expected Failure*: MSBuild terminates with exit code 1 and error `error C2338: static_assert failed: 'The integer comparison functions only accept standard and extended integer types.'`

### 5.2 Verifying the Proposed Fix
1. In `tests/e2e/e2e_framework.hpp`, introduce `is_standard_integer_v` trait:
   ```cpp
   template <typename T>
   inline constexpr bool is_standard_integer_v = std::is_integral_v<T> &&
       !std::is_same_v<T, bool> &&
       !std::is_same_v<T, char> &&
       !std::is_same_v<T, wchar_t> &&
       !std::is_same_v<T, char8_t> &&
       !std::is_same_v<T, char16_t> &&
       !std::is_same_v<T, char32_t>;
   ```
2. Update `CompareEqual`, `CompareNotEqual`, and inequality comparators to check `is_standard_integer_v`.
3. In `ASSERT_FLOAT_NEAR`, format `_diff` and `_eps` with `std::ostringstream` and `std::setprecision(9)`.
4. Re-run:
   ```powershell
   cmake --build build --config Release --target sandbox_e2e_tests
   ctest --test-dir build -C Release --output-on-failure
   ```
5. *Expected Result*: 0 compiler errors, 0 warnings, 100% test pass.
