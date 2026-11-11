# Handoff Report: E2E Framework & Macro Architecture Design

**State Folder**: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\sub_orch_test_infra\explorer_2`  
**Milestone**: M_TEST_1 (Test Infra & Runner Harness)  
**Target Header**: `tests/e2e/e2e_framework.hpp`

---

## 1. Observation

### 1.1 Existing Workspace Configuration & Build Flags
- **Root `CMakeLists.txt` (lines 4-17)**:
  ```cmake
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  # Статическая линковка рантайма MSVC (/MT в Release, /MTd в Debug)
  if(MSVC)
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
      add_compile_options(/W4 /permissive- /utf-8 /MP)
  else()
      add_compile_options(-Wall -Wextra -Wpedantic)
  ...
  ```
  - C++20 is strictly required (`set(CMAKE_CXX_STANDARD 20)`).
  - MSVC compilation uses `/W4` (Level 4 warnings), `/permissive-` (strict standard conformance), and `/utf-8`.
  - Comparing signed and unsigned integers without casts produces warning `C4389: '==': signed/unsigned mismatch` or `C4018` under `/W4`.
  - C++ exceptions are enabled by default under MSVC CMake configurations (`/EHsc`).

### 1.2 Interface Contracts Defined in `PROJECT.md` (lines 151-285)
- **`Chunk`**:
  ```cpp
  namespace sandbox::sim {
  using MaterialId = core::u16;
  inline constexpr MaterialId EMPTY_MATERIAL_ID = 0;

  enum CellFlags : core::u8 {
      FLAG_NONE     = 0x00,
      FLAG_PARITY   = 0x01,
      FLAG_SLEEPING = 0x02,
      FLAG_BURNING  = 0x04,
      FLAG_FALLING  = 0x08
  };

  class alignas(core::CACHE_LINE_SIZE) Chunk {
  public:
      static constexpr core::i32 SIZE = 64;
      static constexpr core::i32 AREA = SIZE * SIZE;

      alignas(core::CACHE_LINE_SIZE) MaterialId material[AREA]{0};
      alignas(core::CACHE_LINE_SIZE) core::f32  temperature[AREA]{20.0f};
      alignas(core::CACHE_LINE_SIZE) core::u8   flags[AREA]{0};
      alignas(core::CACHE_LINE_SIZE) core::u8   lifetime[AREA]{0};

      core::i32 chunkX{0};
      core::i32 chunkY{0};
      core::u32 activeCellCount{0};
      bool isDirty{true};
      bool isSleeping{false};

      inline static constexpr core::i32 Index(core::i32 lx, core::i32 ly) noexcept {
          return (ly << 6) | lx;
      }
  };
  ```
- **`World`**:
  ```cpp
  class World {
  public:
      World(core::i32 width, core::i32 height);
      Chunk* GetChunk(core::i32 cx, core::i32 cy);
      const Chunk* GetChunk(core::i32 cx, core::i32 cy) const;
      bool SetCell(core::i32 x, core::i32 y, MaterialId mat, core::f32 temp = 20.0f);
      MaterialId GetCellMaterial(core::i32 x, core::i32 y) const;
      core::f32 GetCellTemperature(core::i32 x, core::i32 y) const;
      void SwapCells(core::i32 x1, core::i32 y1, core::i32 x2, core::i32 y2);
      void WakeNeighbors(core::i32 cx, core::i32 cy);
  };
  ```
- **`MaterialRegistry`**:
  ```cpp
  enum class MaterialState : core::u8 { Solid, Powder, Liquid, Gas };
  struct Material {
      MaterialId id{0};
      std::string name;
      std::string category;
      core::Color4u color{255, 255, 255, 255};
      ...
  };
  class MaterialRegistry {
  public:
      static MaterialRegistry& Instance();
      bool LoadFromDirectory(const std::string& directoryPath);
      MaterialId RegisterMaterial(Material mat);
      const Material& Get(MaterialId id) const;
      MaterialId FindByName(const std::string& name) const;
      size_t Count() const;
  };
  ```

### 1.3 Scope and Directory Constraints (`SCOPE.md` lines 1-14)
- Test files layout:
  - `tests/e2e/e2e_framework.hpp` (Opaque-box testing harness, assertions, headless runner)
  - `tests/e2e/test_main.cpp` (Main CLI entry point with tier filtering)
  - `tests/e2e/tier1_features.cpp` (Tier 1: >=5 tests per feature)
  - `tests/e2e/tier2_boundaries.cpp` (Tier 2: >=5 boundary and corner cases)
  - `tests/e2e/tier3_pairwise.cpp` (Tier 3: Pairwise interactions)
  - `tests/e2e/tier4_workloads.cpp` (Tier 4: Multi-feature scenarios)
- No SDL or ImGui rendering dependencies; simulation runs 100% headlessly.
- `src/sim` directory is currently empty as Milestone M1 is in progress concurrently.

---

## 2. Logic Chain

1. **Decoupling from In-Flight Simulation Code**:
   Because `src/sim/` is currently being implemented in parallel under M1, any test framework that unconditionally depends on headers like `sim/World.hpp` would fail compilation in M_TEST_1. Therefore, `tests/e2e/e2e_framework.hpp` must define the canonical interface contract (matching `PROJECT.md § Interface Contracts`) and provide a lightweight in-memory mock/fallback world fixture when real simulation headers are not yet linked (`#if __has_include("sim/World.hpp")`). This enables M_TEST_1 to compile cleanly, execute self-tests, and exit 0 immediately.

2. **Zero-Overhead Static Registration**:
   To allow modular test definition across files (`tier1_features.cpp`, `tier2_boundaries.cpp`, etc.) without manual test list maintenance:
   - A Meyer's singleton `TestRegistry::Instance()` provides thread-safe static initialization in C++20.
   - A macro `E2E_TEST(suite, name, tier)` generates a unique static function and a static registrar instance (`TestRegistrar`) using token pasting (`__LINE__`).
   - The registrar registers test metadata (suite, name, tier, function pointer, file, line) into `TestRegistry` before `main()` executes.

3. **Assertion Safety & RAII Cleanliness**:
   - In automated test suites, executing subsequent assertions after a fatal invariant violation causes crashes (null pointer dereferences, heap corruption).
   - An assertion failure must immediately abort the current test without crashing the entire runner process.
   - Throwing a custom `AssertionFailureException` unrolls the stack, invokes destructors for all local objects (freeing memory and resources), and is caught by the test runner loop.
   - To prevent multiple-evaluation side effects (e.g. `ASSERT_EQ(world.GetCellMaterial(x, y), mat)`), macro operands must be captured in `const auto&` local bindings before evaluation.
   - To eliminate MSVC `/W4` signed/unsigned warnings (`C4389`, `C4018`), C++20 standard library utility functions `std::cmp_equal`, `std::cmp_not_equal`, `std::cmp_less`, `std::cmp_less_equal`, `std::cmp_greater`, `std::cmp_greater_equal` from `<utility>` must be used for integral comparisons.

4. **Floating-Point Tolerances**:
   Floating-point arithmetic in thermodynamics and density calculations contains representation inaccuracies. Direct equality checks fail unpredictably. `ASSERT_FLOAT_NEAR(val, exp, eps)` checks `std::abs(val - exp) <= eps` and prints the absolute difference on failure.

5. **Simulation Test Fixtures & Conservation Helpers**:
   Physics cellular automata and thermodynamic simulations require specialized test assertions:
   - Mass conservation: `CountMaterial(mat)` verifies that particles are neither created nor destroyed during movement, density swaps, or dispersion.
   - Energy conservation: `TotalThermalEnergy()` aggregates cell temperature and specific heat to verify heat diffusion conservation.
   - Center-of-mass tracking: `CenterOfMass(mat)` tracks the bulk motion of powder piles falling or gases rising.
   - Bounds-safe cell manipulation: `SetCell`, `GetCellMaterial`, `FillRect`, `FillCircle`.
   - Simulation step control: `Step(ticks)` with standard $\Delta t = 1.0f / 60.0f$, with custom hook support (`SetStepHook`) for chaos testing and mocks.

6. **Self-Contained Smoke Test Suite**:
   To verify Milestone M_TEST_1 acceptance criteria ("Verify that the harness compiles cleanly and executes with exit code 0"), `e2e_framework.hpp` contains a built-in suite of 5 smoke tests (`SmokeTest_BasicAssertions`, `SmokeTest_ComparisonAssertions`, `SmokeTest_FloatNearAssertions`, `SmokeTest_RegistryEnumeration`, `SmokeTest_HeadlessSimFixture`). This guarantees that compiling `sandbox_e2e_tests` produces a working executable that validates its own infrastructure out of the box.

---

## 3. Caveats

1. **Concurrent `src/sim` Header Names**:
   The design assumes future headers will reside at `sim/World.hpp`, `sim/MaterialRegistry.hpp`, and `sim/ReactionSystem.hpp` in accordance with `PROJECT.md § Code Layout`. Conditional inclusion `#if __has_include("sim/World.hpp")` is used so that the framework functions in both standalone mock mode and real-engine integration mode.
2. **Deterministic PRNG**:
   Certain simulation features (e.g., Brownian motion of gases, diagonal powder sliding) may use random numbers. For tests to remain 100% deterministic, fixtures should reset PRNG seeds if random generators are exposed.
3. **Multi-threading Test Safety**:
   Individual test cases are executed sequentially by the runner. If parallel test execution is introduced in the future, `TestRegistry` registration is completed pre-`main` (read-only during test execution), making test enumeration thread-safe.

---

## 4. Conclusion & Technical Design

The `tests/e2e/e2e_framework.hpp` header is designed as a standalone, zero-dependency C++20 testing framework tailored specifically for high-performance cellular simulation verification.

### 4.1 Complete Macro Suite Specification

| Macro | Description | Failure Message Format |
|---|---|---|
| `ASSERT_TRUE(cond)` | Verifies condition is true | `Expected true, got false` |
| `ASSERT_FALSE(cond)` | Verifies condition is false | `Expected false, got true` |
| `ASSERT_EQ(val, exp)` | Checks equality (`std::cmp_equal` for ints, `operator==` for others) | `val == exp (Actual: X, Expected: Y)` |
| `ASSERT_NE(val, exp)` | Checks inequality (`std::cmp_not_equal` for ints) | `val != exp (Actual: X, Expected: Y)` |
| `ASSERT_LT(val, exp)` | Checks strictly less than (`std::cmp_less`) | `val < exp (Actual: X, Expected: Y)` |
| `ASSERT_LE(val, exp)` | Checks less than or equal (`std::cmp_less_equal`) | `val <= exp (Actual: X, Expected: Y)` |
| `ASSERT_GT(val, exp)` | Checks strictly greater than (`std::cmp_greater`) | `val > exp (Actual: X, Expected: Y)` |
| `ASSERT_GE(val, exp)` | Checks greater than or equal (`std::cmp_greater_equal`)| `val >= exp (Actual: X, Expected: Y)` |
| `ASSERT_FLOAT_NEAR(v, e, eps)` | Checks `std::abs(v - e) <= eps` | `\|v - e\| <= eps (Actual: X, Diff: D, Expected: Y +/- eps)` |
| `E2E_TEST(suite, name, tier)` | Declares and statically registers an E2E test case | Pre-main registration via `TestRegistrar` |
| `E2E_TEST_DESC(suite, name, tier, desc)` | Declares test case with requirement description string | Pre-main registration with docstring |

### 4.2 Data Structures & Test Registry Architecture

```cpp
namespace sandbox::test {

using TestFunc = void (*)();

enum class TestTier : int {
    Smoke = 0,
    Tier1 = 1,
    Tier2 = 2,
    Tier3 = 3,
    Tier4 = 4,
    Tier5 = 5
};

struct TestFailure {
    std::string file;
    int line{0};
    std::string expression;
    std::string actualValue;
    std::string expectedValue;
    std::string message;
};

class AssertionFailureException : public std::exception {
public:
    explicit AssertionFailureException(TestFailure failure) : m_failure(std::move(failure)) {}
    const char* what() const noexcept override { return m_failure.message.c_str(); }
    const TestFailure& GetFailure() const noexcept { return m_failure; }
private:
    TestFailure m_failure;
};

struct TestMetadata {
    std::string suite;
    std::string name;
    int tier{1};
    TestFunc func{nullptr};
    std::string file;
    int line{0};
    std::string description;

    [[nodiscard]] std::string FullName() const {
        return suite + "." + name;
    }
};

class TestRegistry {
public:
    static TestRegistry& Instance() {
        static TestRegistry s_registry;
        return s_registry;
    }

    void Register(const TestMetadata& meta) {
        m_tests.push_back(meta);
    }

    [[nodiscard]] const std::vector<TestMetadata>& GetAllTests() const noexcept {
        return m_tests;
    }

    [[nodiscard]] std::vector<TestMetadata> GetTestsByTier(int tier) const {
        std::vector<TestMetadata> filtered;
        for (const auto& t : m_tests) {
            if (t.tier == tier) filtered.push_back(t);
        }
        return filtered;
    }

    [[nodiscard]] size_t Count() const noexcept {
        return m_tests.size();
    }

private:
    std::vector<TestMetadata> m_tests;
};

struct TestRegistrar {
    TestRegistrar(const char* suite, const char* name, int tier,
                  TestFunc func, const char* file, int line, const char* desc = "") {
        TestMetadata meta;
        meta.suite = suite;
        meta.name = name;
        meta.tier = tier;
        meta.func = func;
        meta.file = file;
        meta.line = line;
        meta.description = desc;
        TestRegistry::Instance().Register(meta);
    }
};

} // namespace sandbox::test
```

### 4.3 Safe Stringification & MSVC Warning-Free Comparisons

```cpp
namespace sandbox::test {

template<typename T>
std::string Stringify(const T& val) {
    if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return val ? "true" : "false";
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        return "\"" + val + "\"";
    } else if constexpr (std::is_same_v<std::decay_t<T>, const char*>) {
        return val ? ("\"" + std::string(val) + "\"") : "nullptr";
    } else if constexpr (std::is_integral_v<std::decay_t<T>>) {
        return std::to_string(val);
    } else if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << val;
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << val;
        return oss.str();
    }
}

template<typename A, typename B>
constexpr bool CompareEq(const A& a, const B& b) {
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
        return std::cmp_equal(a, b);
    } else if constexpr (std::is_same_v<std::decay_t<A>, const char*> && std::is_same_v<std::decay_t<B>, const char*>) {
        if (a == nullptr && b == nullptr) return true;
        if (a == nullptr || b == nullptr) return false;
        return std::strcmp(a, b) == 0;
    } else {
        return a == b;
    }
}

template<typename A, typename B>
constexpr bool CompareNe(const A& a, const B& b) {
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
        return std::cmp_not_equal(a, b);
    } else {
        return !CompareEq(a, b);
    }
}

template<typename A, typename B>
constexpr bool CompareLt(const A& a, const B& b) {
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
        return std::cmp_less(a, b);
    } else {
        return a < b;
    }
}

template<typename A, typename B>
constexpr bool CompareLe(const A& a, const B& b) {
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
        return std::cmp_less_equal(a, b);
    } else {
        return a <= b;
    }
}

template<typename A, typename B>
constexpr bool CompareGt(const A& a, const B& b) {
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
        return std::cmp_greater(a, b);
    } else {
        return a > b;
    }
}

template<typename A, typename B>
constexpr bool CompareGe(const A& a, const B& b) {
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
        return std::cmp_greater_equal(a, b);
    } else {
        return a >= b;
    }
}

inline void FailAssertion(const char* file, int line, const char* expr,
                          const std::string& actual, const std::string& expected,
                          const char* msg) {
    TestFailure f;
    f.file = file ? file : "";
    f.line = line;
    f.expression = expr ? expr : "";
    f.actualValue = actual;
    f.expectedValue = expected;
    f.message = msg ? msg : "";
    throw AssertionFailureException(std::move(f));
}

} // namespace sandbox::test
```

### 4.4 Macro Definitions

```cpp
#define SANDBOX_CONCAT_INNER(a, b) a##b
#define SANDBOX_CONCAT(a, b) SANDBOX_CONCAT_INNER(a, b)

#define E2E_TEST_DESC(suite_name, test_name, tier_num, description_str) \
    static void SANDBOX_CONCAT(Test_##suite_name##_##test_name##_fn_, __LINE__)(); \
    static const ::sandbox::test::TestRegistrar SANDBOX_CONCAT(Test_##suite_name##_##test_name##_reg_, __LINE__)( \
        #suite_name, #test_name, tier_num, \
        &SANDBOX_CONCAT(Test_##suite_name##_##test_name##_fn_, __LINE__), \
        __FILE__, __LINE__, description_str \
    ); \
    static void SANDBOX_CONCAT(Test_##suite_name##_##test_name##_fn_, __LINE__)()

#define E2E_TEST(suite_name, test_name, tier_num) \
    E2E_TEST_DESC(suite_name, test_name, tier_num, "")

#define ASSERT_TRUE(cond) \
    do { \
        const auto& _val = (cond); \
        if (!_val) { \
            ::sandbox::test::FailAssertion(__FILE__, __LINE__, #cond, ::sandbox::test::Stringify(_val), "true", "Expected true, got false"); \
        } \
    } while (false)

#define ASSERT_FALSE(cond) \
    do { \
        const auto& _val = (cond); \
        if (!!_val) { \
            ::sandbox::test::FailAssertion(__FILE__, __LINE__, #cond, ::sandbox::test::Stringify(_val), "false", "Expected false, got true"); \
        } \
    } while (false)

#define ASSERT_EQ(val, exp) \
    do { \
        const auto& _actual = (val); \
        const auto& _expected = (exp); \
        if (!::sandbox::test::CompareEq(_actual, _expected)) { \
            ::sandbox::test::FailAssertion(__FILE__, __LINE__, #val " == " #exp, ::sandbox::test::Stringify(_actual), ::sandbox::test::Stringify(_expected), "Equality assertion failed"); \
        } \
    } while (false)

#define ASSERT_NE(val, exp) \
    do { \
        const auto& _actual = (val); \
        const auto& _expected = (exp); \
        if (!::sandbox::test::CompareNe(_actual, _expected)) { \
            ::sandbox::test::FailAssertion(__FILE__, __LINE__, #val " != " #exp, ::sandbox::test::Stringify(_actual), ::sandbox::test::Stringify(_expected), "Inequality assertion failed"); \
        } \
    } while (false)

#define ASSERT_LT(val, exp) \
    do { \
        const auto& _actual = (val); \
        const auto& _expected = (exp); \
        if (!::sandbox::test::CompareLt(_actual, _expected)) { \
            ::sandbox::test::FailAssertion(__FILE__, __LINE__, #val " < " #exp, ::sandbox::test::Stringify(_actual), ::sandbox::test::Stringify(_expected), "Less-than assertion failed"); \
        } \
    } while (false)

#define ASSERT_LE(val, exp) \
    do { \
        const auto& _actual = (val); \
        const auto& _expected = (exp); \
        if (!::sandbox::test::CompareLe(_actual, _expected)) { \
            ::sandbox::test::FailAssertion(__FILE__, __LINE__, #val " <= " #exp, ::sandbox::test::Stringify(_actual), ::sandbox::test::Stringify(_expected), "Less-or-equal assertion failed"); \
        } \
    } while (false)

#define ASSERT_GT(val, exp) \
    do { \
        const auto& _actual = (val); \
        const auto& _expected = (exp); \
        if (!::sandbox::test::CompareGt(_actual, _expected)) { \
            ::sandbox::test::FailAssertion(__FILE__, __LINE__, #val " > " #exp, ::sandbox::test::Stringify(_actual), ::sandbox::test::Stringify(_expected), "Greater-than assertion failed"); \
        } \
    } while (false)

#define ASSERT_GE(val, exp) \
    do { \
        const auto& _actual = (val); \
        const auto& _expected = (exp); \
        if (!::sandbox::test::CompareGe(_actual, _expected)) { \
            ::sandbox::test::FailAssertion(__FILE__, __LINE__, #val " >= " #exp, ::sandbox::test::Stringify(_actual), ::sandbox::test::Stringify(_expected), "Greater-or-equal assertion failed"); \
        } \
    } while (false)

#define ASSERT_FLOAT_NEAR(val, exp, eps) \
    do { \
        const auto _actual = static_cast<double>(val); \
        const auto _expected = static_cast<double>(exp); \
        const auto _epsilon = static_cast<double>(eps); \
        const auto _diff = std::abs(_actual - _expected); \
        if (_diff > _epsilon) { \
            ::sandbox::test::FailAssertion(__FILE__, __LINE__, "std::abs(" #val " - " #exp ") <= " #eps, \
                ::sandbox::test::Stringify(_actual) + " (diff: " + ::sandbox::test::Stringify(_diff) + ")", \
                ::sandbox::test::Stringify(_expected) + " +/- " + ::sandbox::test::Stringify(_epsilon), \
                "Float near assertion failed"); \
        } \
    } while (false)
```

### 4.5 Headless Simulation Fixture (`HeadlessSimFixture`)

```cpp
namespace sandbox::test {

struct Point2f {
    float x{0.0f};
    float y{0.0f};
};

class HeadlessSimFixture {
public:
    using StepHook = std::function<void(HeadlessSimFixture& fixture, float dt)>;

    explicit HeadlessSimFixture(int chunksX = 2, int chunksY = 2)
        : m_chunksX(chunksX), m_chunksY(chunksY),
          m_widthCells(chunksX * 64), m_heightCells(chunksY * 64) {
        m_cells.resize(static_cast<size_t>(m_widthCells * m_heightCells), 0);
        m_temperatures.resize(static_cast<size_t>(m_widthCells * m_heightCells), 20.0f);
    }

    [[nodiscard]] int GetWidthCells() const noexcept { return m_widthCells; }
    [[nodiscard]] int GetHeightCells() const noexcept { return m_heightCells; }
    [[nodiscard]] int GetChunksX() const noexcept { return m_chunksX; }
    [[nodiscard]] int GetChunksY() const noexcept { return m_chunksY; }

    [[nodiscard]] bool InBounds(int x, int y) const noexcept {
        return x >= 0 && x < m_widthCells && y >= 0 && y < m_heightCells;
    }

    bool SetCell(int x, int y, uint16_t mat, float temp = 20.0f) {
        if (!InBounds(x, y)) return false;
        size_t idx = static_cast<size_t>(y * m_widthCells + x);
        m_cells[idx] = mat;
        m_temperatures[idx] = temp;
        return true;
    }

    [[nodiscard]] uint16_t GetCellMaterial(int x, int y) const {
        if (!InBounds(x, y)) return 0;
        return m_cells[static_cast<size_t>(y * m_widthCells + x)];
    }

    [[nodiscard]] float GetCellTemperature(int x, int y) const {
        if (!InBounds(x, y)) return 20.0f;
        return m_temperatures[static_cast<size_t>(y * m_widthCells + x)];
    }

    void SwapCells(int x1, int y1, int x2, int y2) {
        if (!InBounds(x1, y1) || !InBounds(x2, y2)) return;
        size_t idx1 = static_cast<size_t>(y1 * m_widthCells + x1);
        size_t idx2 = static_cast<size_t>(y2 * m_widthCells + x2);
        std::swap(m_cells[idx1], m_cells[idx2]);
        std::swap(m_temperatures[idx1], m_temperatures[idx2]);
    }

    void FillRect(int rx, int ry, int rw, int rh, uint16_t mat, float temp = 20.0f) {
        for (int y = ry; y < ry + rh; ++y) {
            for (int x = rx; x < rx + rw; ++x) {
                SetCell(x, y, mat, temp);
            }
        }
    }

    void FillCircle(int cx, int cy, int radius, uint16_t mat, float temp = 20.0f) {
        int r2 = radius * radius;
        for (int y = cy - radius; y <= cy + radius; ++y) {
            for (int x = cx - radius; x <= cx + radius; ++x) {
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r2) {
                    SetCell(x, y, mat, temp);
                }
            }
        }
    }

    void Clear() {
        std::fill(m_cells.begin(), m_cells.end(), static_cast<uint16_t>(0));
        std::fill(m_temperatures.begin(), m_temperatures.end(), 20.0f);
    }

    void SetStepHook(StepHook hook) {
        m_stepHook = std::move(hook);
    }

    void Step(int ticks = 1) {
        const float dt = 1.0f / 60.0f;
        for (int t = 0; t < ticks; ++t) {
            if (m_stepHook) {
                m_stepHook(*this, dt);
            } else {
                DefaultMockStep();
            }
            m_totalTicksStepped++;
        }
    }

    [[nodiscard]] int GetTotalTicks() const noexcept { return m_totalTicksStepped; }

    [[nodiscard]] int CountMaterial(uint16_t mat) const {
        int count = 0;
        for (auto m : m_cells) {
            if (m == mat) ++count;
        }
        return count;
    }

    [[nodiscard]] int CountMaterialInRect(int rx, int ry, int rw, int rh, uint16_t mat) const {
        int count = 0;
        for (int y = ry; y < ry + rh; ++y) {
            for (int x = rx; x < rx + rw; ++x) {
                if (InBounds(x, y) && GetCellMaterial(x, y) == mat) {
                    ++count;
                }
            }
        }
        return count;
    }

    [[nodiscard]] float TotalThermalEnergy() const {
        float sum = 0.0f;
        for (auto t : m_temperatures) {
            sum += t;
        }
        return sum;
    }

    [[nodiscard]] Point2f CenterOfMass(uint16_t mat) const {
        double sumX = 0.0;
        double sumY = 0.0;
        int count = 0;
        for (int y = 0; y < m_heightCells; ++y) {
            for (int x = 0; x < m_widthCells; ++x) {
                if (GetCellMaterial(x, y) == mat) {
                    sumX += x;
                    sumY += y;
                    count++;
                }
            }
        }
        if (count == 0) return {0.0f, 0.0f};
        return {static_cast<float>(sumX / count), static_cast<float>(sumY / count)};
    }

    [[nodiscard]] bool IsMassConserved(uint16_t mat, int expectedCount) const {
        return CountMaterial(mat) == expectedCount;
    }

private:
    void DefaultMockStep() {
        // Simple falling powder downward mock step (bottom-up sweep)
        for (int y = m_heightCells - 2; y >= 0; --y) {
            for (int x = 0; x < m_widthCells; ++x) {
                uint16_t mat = GetCellMaterial(x, y);
                if (mat != 0 && GetCellMaterial(x, y + 1) == 0) {
                    SwapCells(x, y, x, y + 1);
                }
            }
        }
    }

    int m_chunksX{2};
    int m_chunksY{2};
    int m_widthCells{128};
    int m_heightCells{128};
    int m_totalTicksStepped{0};
    std::vector<uint16_t> m_cells;
    std::vector<float> m_temperatures;
    StepHook m_stepHook;
};

} // namespace sandbox::test
```

### 4.6 Built-in Smoke Test Suite Specification

The following 5 smoke tests are defined directly in or alongside `e2e_framework.hpp`:

1. **`FrameworkSmokeTest.AssertionsBasic` (Tier 1)**:
   - Validates `ASSERT_TRUE(true)` and `ASSERT_FALSE(false)`.
   - Validates `ASSERT_EQ(42, 42)` and `ASSERT_NE(42, 100)`.
   - Validates string equality: `ASSERT_EQ(std::string("sandbox"), "sandbox")`.
   - Confirms that assertion failures throw `AssertionFailureException` using a `try-catch` block.

2. **`FrameworkSmokeTest.AssertionsComparison` (Tier 1)**:
   - Validates `ASSERT_LT(10, 20)` and `ASSERT_LE(20, 20)`.
   - Validates `ASSERT_GT(50, 20)` and `ASSERT_GE(50, 50)`.
   - Validates mixed signed/unsigned comparisons without MSVC compiler warnings:
     `int a = -5; unsigned int b = 10; ASSERT_LT(a, b);`.

3. **`FrameworkSmokeTest.AssertionsFloatNear` (Tier 1)**:
   - Validates `ASSERT_FLOAT_NEAR(3.14159f, 3.14150f, 0.001f)`.
   - Validates small epsilon delta handling.

4. **`FrameworkSmokeTest.RegistryEnumeration` (Tier 1)**:
   - Validates `TestRegistry::Instance().Count() >= 5`.
   - Validates that `GetTestsByTier(1)` returns the registered smoke tests.
   - Validates metadata accuracy (`suite == "FrameworkSmokeTest"`, non-empty `file`, `line > 0`).

5. **`FrameworkSmokeTest.HeadlessSimFixture` (Tier 1)**:
   - Instantiates a `HeadlessSimFixture(2, 2)` (128x128 cells).
   - Validates `FillRect(10, 10, 20, 10, 1, 50.0f)`.
   - Validates `CountMaterial(1) == 200`.
   - Validates mass conservation: `ASSERT_TRUE(fixture.IsMassConserved(1, 200))`.
   - Steps simulation with `Step(5)` and verifies particles fall downward and conserve total count.

---

## 5. Verification Method

### 5.1 Verification Commands
The implementing worker agent can verify the framework with:
1. **Configure CMake**:
   ```powershell
   cmake -B build -S .
   ```
2. **Build Target**:
   ```powershell
   cmake --build build --config Release --target sandbox_e2e_tests
   ```
3. **Execute Runner**:
   ```powershell
   .\build\Release\sandbox_e2e_tests.exe --all --verbose
   ```
4. **CTest Registration**:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```

### 5.2 Invalidation Conditions
The design is invalidated if:
- Macro comparisons between `int` and `core::u32` cause MSVC warnings under `/W4`.
- Multiple calls to `ASSERT_EQ` evaluate expressions with side-effects more than once.
- A failed assertion causes program crash (SIGSEGV / std::terminate) instead of catching `AssertionFailureException`.
- Static test registration fails across multiple compilation units due to linker discarding.
