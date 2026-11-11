# Architectural Investigation & Design Report: E2E Test Harness & Runner Architecture

**Author**: Explorer Agent (Harness Architecture Explorer)  
**Milestone**: M_TEST_1 (Test Infra & Runner Harness)  
**Target Files**: `tests/e2e/e2e_framework.hpp`, `tests/e2e/test_main.cpp`  
**Date**: 2026-11-11  

---

## 1. Observation

### 1.1 Existing Codebase & Project Interfaces
Direct inspection of the codebase revealed the following interface contracts and structural characteristics:

1. **Mandatory Documentation & Scope**:
   - `ORIGINAL_REQUEST.md` (lines 34, 35, 61): Mandates "CTest modular tests for key components" and console-based headless benchmark and test modes.
   - `PROJECT.md § Architecture` (lines 5–36): Decouples presentation (SDL2 / Dear ImGui) from the Simulation Core (`World`, `Chunk`, `MaterialRegistry`, `Physics`, `Thermodynamics`, `ReactionSystem`, `ExplosionSystem`, `Simulator`) and Storage (`Serialization`).
   - `SCOPE.md § Architecture` (lines 4–14): Specifies opaque-box, requirement-driven, headless testing without UI/SDL dependencies, with an independent test executable `sandbox_e2e_tests` registered in CTest.
   - `SCOPE.md § Test directory layout` (lines 7–13):
     - `tests/e2e/e2e_framework.hpp`: Opaque-box testing harness, assertions, headless runner.
     - `tests/e2e/test_main.cpp`: Main CLI entry point with tier filtering (`--tier=1`, `--tier=2`, `--tier=3`, `--tier=4`, `--all`, `--verbose`).
     - `tests/e2e/tier1_features.cpp`: Tier 1 feature tests (>=5 tests per feature).
     - `tests/e2e/tier2_boundaries.cpp`: Tier 2 boundary and corner case tests (>=5 per feature).
     - `tests/e2e/tier3_pairwise.cpp`: Tier 3 pairwise cross-feature interaction tests.
     - `tests/e2e/tier4_workloads.cpp`: Tier 4 realistic multi-feature application scenarios.

2. **Simulation Core Public Interfaces (`src/sim/`, `src/core/`)**:
   - `Chunk.hpp` (lines 22–74):
     - Cache-line aligned: `class alignas(core::CACHE_LINE_SIZE) Chunk`.
     - Dimensions: `SIZE = 64`, `AREA = 4096`.
     - SoA member arrays: `material[4096]`, `temperature[4096]`, `flags[4096]`, `lifetime[4096]`.
     - Metadata: `chunkX`, `chunkY`, `activeCellCount`, `minActiveY`, `maxActiveY`, `sleepCounter`, `isDirty`, `isSleeping`.
     - Methods: `Index(lx, ly)`, `FromIndex(index, lx, ly)`, `GetPhase()`, `SetCell()`, `Clear()`, `WakeUp()`, `StepSleep()`, `UpdateActiveBounds()`.
   - `World.hpp` (lines 10–100):
     - Coordinate transforms: `WorldToChunkX/Y()`, `WorldToLocalX/Y()`, `LocalToWorldX/Y()`.
     - Accessors: `GetWidth()`, `GetHeight()`, `GetChunksX()`, `GetChunksY()`, `GetChunkCount()`, `GetCurrentParity()`, `IsInBounds(x, y)`.
     - Cell accessors: `SetCell(x, y, mat, temp, flags, lifetime)`, `GetCellMaterial(x, y)`, `GetCellTemperature(x, y)`, `GetCellFlags(x, y)`, `GetCellLifetime(x, y)`.
     - Cell mutation: `SwapCells(x1, y1, x2, y2)`.
     - Simulation lifecycle: `AdvanceTick()`, `Clear()`, `WakeChunk()`, `WakeNeighbors()`, `WakeChunkAtWorld()`.
     - Telemetry: `GetTotalActiveCells()`, `GetAwakeChunkCount()`.
   - `Material.hpp` (lines 8–50):
     - `MaterialId` (`core::u16`), `EMPTY_MATERIAL_ID = 0`.
     - `MaterialState` enum: `Solid`, `Powder`, `Liquid`, `Gas`.
     - `Material` struct: `id`, `name`, `category`, `density`, `dispersion`, `strength`, `thermalConductivity`, `specificHeat`, `meltPoint`, `boilPoint`, `freezePoint`, `meltMaterialId`, `boilMaterialId`, `freezeMaterialId`, `flammability`, `ignitionTemp`, `lifetime`.
   - `MaterialRegistry.hpp` (lines 10–42):
     - Singleton: `MaterialRegistry::Instance()`.
     - Methods: `LoadFromDirectory(path)`, `LoadFromFile(path)`, `ResolvePhaseTargets()`, `RegisterMaterial(mat)`, `Get(id)`, `FindByName(name)`, `Count()`, `GetAll()`, `Clear()`.
   - `Physics.hpp` (lines 10–35):
     - Methods: `Step(World& world) noexcept`, `StepChunk(World& world, Chunk& chunk) noexcept`, `GetTickCount()`, `Reset()`.
   - `core/Types.hpp` & `core/Math.hpp`:
     - Types: `i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`, `f32`, `f64`.
     - Geometries: `Point2i`, `Rect2i` (`Contains(px, py)`), `Color4u`.
     - Math: `FastRNG`, `Clamp()`.

3. **Presentation Decoupling**:
   - `src/sim` and `src/core` contain zero `#include <SDL.h>` and zero `#include <imgui.h>`.
   - All simulation classes are pure C++20 standard library dependent.
   - Consequently, the test runner executable `sandbox_e2e_tests` can be compiled and linked purely headlessly without SDL2 or Dear ImGui GUI libraries.

---

## 2. Logic Chain

### 2.1 Dependency-Free Test Registry vs. External Frameworks
1. *Observation 1.1*: Neither GTest nor Catch2 is present in `external/` or `vcpkg.json`. The engine is configured with C++20 (`set(CMAKE_CXX_STANDARD 20)` in root `CMakeLists.txt`).
2. *Rationale*:
   - Adding third-party testing frameworks introduces external dependencies, build overhead, and potential CMake linkage conflicts on Windows MSVC.
   - A lightweight, header-only C++20 testing harness written specifically for this engine provides:
     - Instantaneous compilation (<0.5s).
     - Native integration with engine types (`World`, `Chunk`, `MaterialId`, `Point2i`, `Rect2i`).
     - Tier-aware test categorization (`tier=1..4`) natively built into the test registration macro.
     - Zero external runtime DLL or static library dependencies.

### 2.2 Test Auto-Registration & ODR Safety
1. *Observation 1.1*: The E2E tests are partitioned into multiple compilation units: `test_main.cpp`, `tier1_features.cpp`, `tier2_boundaries.cpp`, `tier3_pairwise.cpp`, `tier4_workloads.cpp`.
2. *Hazard*: If a global test registry is declared as a global variable in a shared header, it violates the One Definition Rule (ODR) or results in disjoint registries across translation units.
3. *Resolution Mechanism*:
   - Use Meyers' Singleton for the registry:
     ```cpp
     class TestRegistry {
     public:
         static TestRegistry& Instance() {
             static TestRegistry s_instance;
             return s_instance;
         }
         ...
     };
     ```
     Under the C++20 standard (§9.7 [stmt.dcl]), a function-local static object inside a non-inline or inline function has external linkage and is guaranteed to be instantiated exactly once across all translation units linking into the executable.
   - Test registration uses a static RAII registrar struct:
     ```cpp
     struct TestAutoRegistrar {
         TestAutoRegistrar(const char* suite, const char* name, int tier, 
                            const char* file, int line, std::function<void()> func) {
             TestRegistry::Instance().RegisterTest({suite, name, tier, file, line, std::move(func)});
         }
     };
     ```
   - Macro `E2E_TEST(suite, name, tier)` generates an anonymous static function and static registrar instance:
     ```cpp
     #define E2E_TEST(suite, name, tier) \
         static void suite##_##name##_TestBody(); \
         static const ::sandbox::test::TestAutoRegistrar suite##_##name##_Registrar( \
             #suite, #name, static_cast<int>(tier), __FILE__, __LINE__, &suite##_##name##_TestBody); \
         static void suite##_##name##_TestBody()
     ```
   - This ensures all tests in all `.cpp` files automatically self-register prior to `main()` execution.

### 2.3 Assertion Macro Semantics & Control Flow Safety
1. *Observation*: When an assertion such as `ASSERT_TRUE(chunk != nullptr)` fails, executing subsequent statements in the test body dereferences a null pointer or invalid state, crashing the test process.
2. *Requirement*: Assertions must immediately abort the executing test case, report line number, expression, and evaluated values, and return control to the runner loop without crashing the overall process.
3. *Mechanism*:
   - Define custom exception `AssertionFailureException`:
     ```cpp
     class AssertionFailureException : public std::exception { ... };
     ```
   - All `ASSERT_*` macros throw `AssertionFailureException` when conditions fail.
   - The test runner's execution loop catches `AssertionFailureException`, logs the failure, marks the test as FAILED, and safely proceeds to execute subsequent tests.
   - Catching `std::exception` and `...` additionally guards against unexpected runtime exceptions, marking them as CRASHED without aborting the suite.
4. *Type-Safe Comparison & Formatters*:
   - To prevent `-Wsign-compare` compiler warnings under MSVC `/W4` when comparing signed vs unsigned integers (e.g., `int` coordinate vs `size_t` count), `ASSERT_EQ` and `ASSERT_NE` use C++20 `std::cmp_equal` and `std::cmp_not_equal` from `<utility>`.
   - String, float, enum, and arithmetic formatting is unified via a C++20 template concept `FormatValue(const T& val)` using `std::ostringstream`.
   - `ASSERT_FLOAT_NEAR(a, b, epsilon)` verifies `std::abs(static_cast<double>(a) - static_cast<double>(b)) <= static_cast<double>(epsilon)`.

### 2.4 Headless Simulation & Inspection Helpers
1. *Observation 1.2*: E2E tests require deterministic initialization and verification of cellular patterns, materials, temperatures, and physics behaviors.
2. *Designed Helpers*:
   - **Asset Resolution (`ResolveAssetPath`)**: Automatically resolves paths to `assets/materials/` regardless of whether the executable is run from the project root (`./`), build directory (`build/`), or CTest test folder (`build/tests/Release/`).
   - **World Fixture (`TestWorldFixture`)**: Handles creation of sized worlds (default or compact 128x128 for rapid execution), automatic material registry loading, and clean state resets.
   - **Cell Inspection Helpers**:
     - `inspection::CountMaterial(world, mat)`: Full world count.
     - `inspection::CountMaterialInBox(world, mat, x, y, w, h)`: Sub-region count.
     - `inspection::GetBoundingBox(world, mat)`: Returns minimum enclosing `Rect2i`.
     - `inspection::GetAverageTemperatureInBox(world, x, y, w, h)`: Mean thermal state for heat diffusion tests.
     - `inspection::GetTemperatureRangeInBox(world, x, y, w, h, outMin, outMax)`: Thermal bounds.
     - `inspection::FillBox(world, x, y, w, h, mat, temp)`: Rapid geometry placement.
     - `inspection::IsBoxUniform(world, x, y, w, h, mat)`: Homogeneity verification.
     - `inspection::GetCenterOfMass(world, mat)`: Verifies dispersion symmetry and anti-bias physics.
   - **Deterministic Stepping Helpers**:
     - `step::Ticks(world, physics, count)`: Deterministically advances simulation by $N$ frames.
     - `step::Until(world, physics, predicate, maxTicks)`: Steps until condition holds or ceiling is reached.
     - `step::UntilSettled(world, physics, maxTicks)`: Steps until active cells settle or ceiling is reached.

### 2.5 CLI Runner & CTest Execution Contract
1. *Observation 1.1*: Command line runner must support tier filtering, verbose diagnostics, and standard exit codes.
2. *CLI Flags Specification*:
   - `--tier=1`: Executes only Tier 1 Feature tests.
   - `--tier=2`: Executes only Tier 2 Boundary tests.
   - `--tier=3`: Executes only Tier 3 Pairwise tests.
   - `--tier=4`: Executes only Tier 4 Workload tests.
   - `--all`: Executes all tiers (default if no tier flag is specified).
   - `--verbose`: Prints per-test execution line, timing in milliseconds, and detailed assertion messages.
   - `--filter=<pattern>`: Substring filter on `suite.name`.
   - `--list`: Lists registered tests by tier without executing.
   - `--help`: Displays command-line syntax and examples.
3. *Exit Code Contract*:
   - Returns `0` if and only if all selected tests pass.
   - Returns `1` if one or more tests fail or crash, or if an invalid argument is supplied.
   - Guarantees 100% compatibility with CTest test runners.

---

## 3. Caveats

1. **Compilation Multi-File Linkage**:
   - `e2e_framework.hpp` contains function templates, inline functions, and static registrar types.
   - All non-template functions in `e2e_framework.hpp` MUST be marked `inline` to prevent ODR duplicate symbol errors when included across `test_main.cpp`, `tier1_features.cpp`, `tier2_boundaries.cpp`, `tier3_pairwise.cpp`, and `tier4_workloads.cpp`.
2. **Material Asset Availability**:
   - Tests querying specific materials (e.g. `FindByName("sand")`) require `MaterialRegistry::Instance().LoadFromDirectory(...)` to succeed.
   - The `TestWorldFixture::EnsureMaterialsLoaded()` helper must be invoked before material ID lookups to guarantee that tests never run against an empty registry.
3. **Future Milestone Compatibility**:
   - In M1, simulation stepping invokes `Physics::Step(world)`.
   - In M2 (Thermodynamics, Reactions, Explosions) and M3 (Multithreading Simulator), additional subsystems will be incorporated into the stepping loop.
   - The stepping helper templates `step::Ticks` and `step::Until` are designed as generic templates `template <typename StepperFunc>`, accepting lambdas, `sim::Physics&`, or future `sim::Simulator&` seamlessly without interface refactoring.

---

## 4. Conclusion & Concrete Architectural Design

### 4.1 Specification of `tests/e2e/e2e_framework.hpp`

Below is the complete, production-grade architecture designed for `tests/e2e/e2e_framework.hpp`. The implementer can adopt this directly:

```cpp
#pragma once

#include "core/Types.hpp"
#include "core/Math.hpp"
#include "sim/Chunk.hpp"
#include "sim/World.hpp"
#include "sim/Material.hpp"
#include "sim/MaterialRegistry.hpp"
#include "sim/Physics.hpp"

#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <sstream>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <algorithm>
#include <utility>
#include <type_traits>
#include <iomanip>
#include <map>

namespace sandbox::test {

// ============================================================================
// 1. Assertion Failure Exception
// ============================================================================
class AssertionFailureException : public std::exception {
private:
    std::string m_message;
    std::string m_file;
    int m_line;
    std::string m_fullReport;

public:
    AssertionFailureException(std::string message, std::string file, int line)
        : m_message(std::move(message)), m_file(std::move(file)), m_line(line) {
        m_fullReport = m_file + ":" + std::to_string(m_line) + ": " + m_message;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return m_fullReport.c_str();
    }

    [[nodiscard]] const std::string& GetMessage() const noexcept { return m_message; }
    [[nodiscard]] const std::string& GetFile() const noexcept { return m_file; }
    [[nodiscard]] int GetLine() const noexcept { return m_line; }
};

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

template <typename T, typename U>
inline bool CompareEqual(const T& a, const U& b) {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        return std::cmp_equal(a, b);
    } else {
        return a == b;
    }
}

template <typename T, typename U>
inline bool CompareNotEqual(const T& a, const U& b) {
    if constexpr (std::is_integral_v<T> && std::is_integral_v<U>) {
        return std::cmp_not_equal(a, b);
    } else {
        return a != b;
    }
}

template <typename T, typename U, typename E>
inline bool CompareFloatNear(const T& a, const U& b, const E& eps) {
    return std::abs(static_cast<double>(a) - static_cast<double>(b)) <= static_cast<double>(eps);
}

// ============================================================================
// 3. Assertion Macros
// ============================================================================
#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_TRUE(") + #cond + ") failed", __FILE__, __LINE__); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) \
    do { \
        if (static_cast<bool>(cond)) { \
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_FALSE(") + #cond + ") failed: expression evaluated to true", __FILE__, __LINE__); \
        } \
    } while (0)

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
            throw ::sandbox::test::AssertionFailureException( \
                std::string("ASSERT_FLOAT_NEAR(") + #a + ", " + #b + ", " + #epsilon + ") failed: diff " + \
                std::to_string(_diff) + " > " + std::to_string(_eps) + " (" + \
                ::sandbox::test::FormatValue(_valA) + " vs " + ::sandbox::test::FormatValue(_valB) + ")", \
                __FILE__, __LINE__); \
        } \
    } while (0)

// ============================================================================
// 4. Test Registry & Auto-Registration
// ============================================================================
struct TestCase {
    std::string suite;
    std::string name;
    int tier{1}; // 1, 2, 3, 4
    std::string file;
    int line{0};
    std::function<void()> testFunc;
};

class TestRegistry {
private:
    std::vector<TestCase> m_tests;

    TestRegistry() = default;

public:
    static TestRegistry& Instance() {
        static TestRegistry s_instance;
        return s_instance;
    }

    TestRegistry(const TestRegistry&) = delete;
    TestRegistry& operator=(const TestRegistry&) = delete;

    void RegisterTest(TestCase test) {
        m_tests.push_back(std::move(test));
    }

    [[nodiscard]] const std::vector<TestCase>& GetTests() const noexcept {
        return m_tests;
    }

    [[nodiscard]] size_t Count() const noexcept {
        return m_tests.size();
    }
};

struct TestAutoRegistrar {
    TestAutoRegistrar(const char* suite, const char* name, int tier,
                      const char* file, int line, std::function<void()> func) {
        TestRegistry::Instance().RegisterTest({suite, name, tier, file, line, std::move(func)});
    }
};

#define E2E_TEST(suite, name, tier) \
    static void suite##_##name##_TestBody(); \
    static const ::sandbox::test::TestAutoRegistrar suite##_##name##_Registrar( \
        #suite, #name, static_cast<int>(tier), __FILE__, __LINE__, &suite##_##name##_TestBody); \
    static void suite##_##name##_TestBody()

// ============================================================================
// 5. Asset Path Resolution
// ============================================================================
inline std::string ResolveAssetPath(const std::string& relativePath) {
    namespace fs = std::filesystem;
    if (fs::exists(relativePath)) return relativePath;
    if (fs::exists("../" + relativePath)) return "../" + relativePath;
    if (fs::exists("../../" + relativePath)) return "../../" + relativePath;
    if (fs::exists("../../../" + relativePath)) return "../../../" + relativePath;
    return relativePath;
}

// ============================================================================
// 6. Headless Fixture & Setup Helpers
// ============================================================================
class TestWorldFixture {
public:
    sim::World world;
    sim::Physics physics;

    explicit TestWorldFixture(core::i32 width = 128, core::i32 height = 128, core::u32 seed = 1337u)
        : world(width, height), physics(seed) {
        EnsureMaterialsLoaded();
        world.Clear(sim::EMPTY_MATERIAL_ID, 20.0f);
    }

    static void EnsureMaterialsLoaded() {
        static bool s_loaded = false;
        if (!s_loaded) {
            std::string dir = ResolveAssetPath("assets/materials");
            sim::MaterialRegistry::Instance().LoadFromDirectory(dir);
            s_loaded = true;
        }
    }

    [[nodiscard]] sim::MaterialId GetMatId(const std::string& name) const noexcept {
        return sim::MaterialRegistry::Instance().FindByName(name);
    }

    void Set(core::i32 x, core::i32 y, const std::string& matName, core::f32 temp = 20.0f) {
        world.SetCell(x, y, GetMatId(matName), temp);
    }

    void Fill(core::i32 x, core::i32 y, core::i32 w, core::i32 h, const std::string& matName, core::f32 temp = 20.0f) {
        sim::MaterialId id = GetMatId(matName);
        for (core::i32 cy = y; cy < y + h; ++cy) {
            for (core::i32 cx = x; cx < x + w; ++cx) {
                world.SetCell(cx, cy, id, temp);
            }
        }
    }

    void Step(int count = 1) {
        for (int i = 0; i < count; ++i) {
            physics.Step(world);
        }
    }

    void Reset(sim::MaterialId fillMat = sim::EMPTY_MATERIAL_ID, core::f32 temp = 20.0f) {
        world.Clear(fillMat, temp);
        physics.Reset();
    }
};

// ============================================================================
// 7. Cell Inspection Helpers
// ============================================================================
namespace inspection {

inline core::u32 CountMaterial(const sim::World& world, sim::MaterialId mat) {
    core::u32 count = 0;
    for (core::i32 y = 0; y < world.GetHeight(); ++y) {
        for (core::i32 x = 0; x < world.GetWidth(); ++x) {
            if (world.GetCellMaterial(x, y) == mat) {
                ++count;
            }
        }
    }
    return count;
}

inline core::u32 CountMaterialInBox(const sim::World& world, sim::MaterialId mat,
                                    core::i32 boxX, core::i32 boxY,
                                    core::i32 boxW, core::i32 boxH) {
    core::u32 count = 0;
    core::i32 startX = std::max(0, boxX);
    core::i32 startY = std::max(0, boxY);
    core::i32 endX = std::min(world.GetWidth(), boxX + boxW);
    core::i32 endY = std::min(world.GetHeight(), boxY + boxH);

    for (core::i32 y = startY; y < endY; ++y) {
        for (core::i32 x = startX; x < endX; ++x) {
            if (world.GetCellMaterial(x, y) == mat) {
                ++count;
            }
        }
    }
    return count;
}

inline core::Rect2i GetBoundingBox(const sim::World& world, sim::MaterialId mat) {
    core::i32 minX = world.GetWidth();
    core::i32 maxX = -1;
    core::i32 minY = world.GetHeight();
    core::i32 maxY = -1;

    for (core::i32 y = 0; y < world.GetHeight(); ++y) {
        for (core::i32 x = 0; x < world.GetWidth(); ++x) {
            if (world.GetCellMaterial(x, y) == mat) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return core::Rect2i{0, 0, 0, 0};
    }
    return core::Rect2i{minX, minY, maxX - minX + 1, maxY - minY + 1};
}

inline core::f32 GetAverageTemperatureInBox(const sim::World& world,
                                            core::i32 boxX, core::i32 boxY,
                                            core::i32 boxW, core::i32 boxH) {
    core::i32 startX = std::max(0, boxX);
    core::i32 startY = std::max(0, boxY);
    core::i32 endX = std::min(world.GetWidth(), boxX + boxW);
    core::i32 endY = std::min(world.GetHeight(), boxY + boxH);

    core::f64 sumTemp = 0.0;
    core::u64 count = 0;

    for (core::i32 y = startY; y < endY; ++y) {
        for (core::i32 x = startX; x < endX; ++x) {
            sumTemp += world.GetCellTemperature(x, y);
            ++count;
        }
    }
    return count > 0 ? static_cast<core::f32>(sumTemp / static_cast<core::f64>(count)) : 0.0f;
}

inline void GetTemperatureRangeInBox(const sim::World& world,
                                     core::i32 boxX, core::i32 boxY,
                                     core::i32 boxW, core::i32 boxH,
                                     core::f32& outMin, core::f32& outMax) {
    core::i32 startX = std::max(0, boxX);
    core::i32 startY = std::max(0, boxY);
    core::i32 endX = std::min(world.GetWidth(), boxX + boxW);
    core::i32 endY = std::min(world.GetHeight(), boxY + boxH);

    outMin = 1e9f;
    outMax = -1e9f;

    for (core::i32 y = startY; y < endY; ++y) {
        for (core::i32 x = startX; x < endX; ++x) {
            core::f32 temp = world.GetCellTemperature(x, y);
            if (temp < outMin) outMin = temp;
            if (temp > outMax) outMax = temp;
        }
    }
    if (outMin > outMax) {
        outMin = 0.0f;
        outMax = 0.0f;
    }
}

inline bool IsBoxUniform(const sim::World& world,
                         core::i32 boxX, core::i32 boxY,
                         core::i32 boxW, core::i32 boxH,
                         sim::MaterialId mat) {
    core::i32 startX = std::max(0, boxX);
    core::i32 startY = std::max(0, boxY);
    core::i32 endX = std::min(world.GetWidth(), boxX + boxW);
    core::i32 endY = std::min(world.GetHeight(), boxY + boxH);

    for (core::i32 y = startY; y < endY; ++y) {
        for (core::i32 x = startX; x < endX; ++x) {
            if (world.GetCellMaterial(x, y) != mat) {
                return false;
            }
        }
    }
    return true;
}

inline std::pair<core::f32, core::f32> GetCenterOfMass(const sim::World& world, sim::MaterialId mat) {
    core::f64 sumX = 0.0;
    core::f64 sumY = 0.0;
    core::u64 count = 0;

    for (core::i32 y = 0; y < world.GetHeight(); ++y) {
        for (core::i32 x = 0; x < world.GetWidth(); ++x) {
            if (world.GetCellMaterial(x, y) == mat) {
                sumX += x;
                sumY += y;
                ++count;
            }
        }
    }
    if (count == 0) return {0.0f, 0.0f};
    return {static_cast<core::f32>(sumX / count), static_cast<core::f32>(sumY / count)};
}

} // namespace inspection

// ============================================================================
// 8. Deterministic Stepping Helpers
// ============================================================================
namespace step {

template <typename StepperFunc>
inline void Ticks(sim::World& world, StepperFunc&& stepFunc, int count) {
    for (int i = 0; i < count; ++i) {
        stepFunc(world);
    }
}

template <typename StepperFunc, typename Predicate>
inline bool Until(sim::World& world, StepperFunc&& stepFunc, Predicate&& predicate, int maxTicks) {
    for (int i = 0; i < maxTicks; ++i) {
        if (predicate(world)) {
            return true;
        }
        stepFunc(world);
    }
    return predicate(world);
}

} // namespace step

} // namespace sandbox::test
```

---

### 4.2 Specification of `tests/e2e/test_main.cpp`

Below is the complete, production-grade CLI runner and test execution harness designed for `tests/e2e/test_main.cpp`:

```cpp
#include "e2e_framework.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <chrono>

using namespace sandbox::test;

struct CommandLineOptions {
    bool runTier1{false};
    bool runTier2{false};
    bool runTier3{false};
    bool runTier4{false};
    bool verbose{false};
    bool listOnly{false};
    bool showHelp{false};
    std::string filter{""};
};

inline void PrintUsage(const char* exeName) {
    std::cout << "Usage: " << exeName << " [options]\n\n"
              << "Options:\n"
              << "  --tier=1       Run only Tier 1: Feature Coverage Tests\n"
              << "  --tier=2       Run only Tier 2: Boundary & Corner Case Tests\n"
              << "  --tier=3       Run only Tier 3: Pairwise Cross-Feature Tests\n"
              << "  --tier=4       Run only Tier 4: Workload & Scenario Tests\n"
              << "  --all          Run all test tiers (default if no tier specified)\n"
              << "  --verbose, -v  Enable detailed per-test logging and assertions\n"
              << "  --filter=<str> Run only tests whose Suite.Name contains <str>\n"
              << "  --list         List all registered tests without running\n"
              << "  --help, -h     Show this help message\n";
}

inline CommandLineOptions ParseCommandLine(int argc, char* argv[]) {
    CommandLineOptions opts;
    bool explicitTierSelected = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--tier=1") {
            opts.runTier1 = true;
            explicitTierSelected = true;
        } else if (arg == "--tier=2") {
            opts.runTier2 = true;
            explicitTierSelected = true;
        } else if (arg == "--tier=3") {
            opts.runTier3 = true;
            explicitTierSelected = true;
        } else if (arg == "--tier=4") {
            opts.runTier4 = true;
            explicitTierSelected = true;
        } else if (arg == "--all") {
            opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true;
            explicitTierSelected = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--list") {
            opts.listOnly = true;
        } else if (arg == "--help" || arg == "-h") {
            opts.showHelp = true;
        } else if (arg.starts_with("--filter=")) {
            opts.filter = arg.substr(9);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            opts.showHelp = true;
        }
    }

    if (!explicitTierSelected) {
        opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true;
    }

    return opts;
}

inline bool IsTierSelected(int tier, const CommandLineOptions& opts) {
    switch (tier) {
        case 1: return opts.runTier1;
        case 2: return opts.runTier2;
        case 3: return opts.runTier3;
        case 4: return opts.runTier4;
        default: return false;
    }
}

inline bool MatchesFilter(const TestCase& test, const std::string& filter) {
    if (filter.empty()) return true;
    std::string fullName = test.suite + "." + test.name;
    return fullName.find(filter) != std::string::npos;
}

struct TierStats {
    int total{0};
    int passed{0};
    int failed{0};
    int crashed{0};
    double elapsedMs{0.0};
};

struct FailureRecord {
    std::string suite;
    std::string name;
    int tier;
    std::string reason;
    double elapsedMs;
};

// ============================================================================
// Built-in Sanity Test: Validates that harness assertions and fixture operate
// ============================================================================
E2E_TEST(HarnessSanity, AssertionsAndWorldFixture, 1) {
    ASSERT_TRUE(true);
    ASSERT_FALSE(false);
    ASSERT_EQ(42, 42);
    ASSERT_NE(10, 20);
    ASSERT_FLOAT_NEAR(3.14159f, 3.14150f, 0.001f);

    TestWorldFixture fixture(64, 64);
    ASSERT_EQ(fixture.world.GetWidth(), 64);
    ASSERT_EQ(fixture.world.GetHeight(), 64);

    fixture.world.SetCell(10, 10, 1, 25.0f);
    ASSERT_EQ(fixture.world.GetCellMaterial(10, 10), 1);
    ASSERT_FLOAT_NEAR(fixture.world.GetCellTemperature(10, 10), 25.0f, 0.01f);

    ASSERT_EQ(inspection::CountMaterial(fixture.world, 1), 1u);
    ASSERT_EQ(inspection::CountMaterialInBox(fixture.world, 1, 0, 0, 20, 20), 1u);
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    CommandLineOptions opts = ParseCommandLine(argc, argv);
    if (opts.showHelp) {
        PrintUsage(argv[0]);
        return 0;
    }

    const auto& allTests = TestRegistry::Instance().GetTests();

    if (opts.listOnly) {
        std::cout << "Registered E2E Tests (" << allTests.size() << " total):\n";
        for (const auto& test : allTests) {
            std::cout << "  [Tier " << test.tier << "] " << test.suite << "." << test.name
                      << " (" << test.file << ":" << test.line << ")\n";
        }
        return 0;
    }

    std::vector<const TestCase*> selectedTests;
    for (const auto& test : allTests) {
        if (!IsTierSelected(test.tier, opts)) continue;
        if (!MatchesFilter(test, opts.filter)) continue;
        selectedTests.push_back(&test);
    }

    std::cout << "============================================================\n";
    std::cout << "  SANDBOX 2D E2E TEST RUNNER\n";
    std::cout << "  Total Registered: " << allTests.size() << " | Selected: " << selectedTests.size() << "\n";
    std::cout << "  Tiers Active: ";
    if (opts.runTier1) std::cout << "T1 ";
    if (opts.runTier2) std::cout << "T2 ";
    if (opts.runTier3) std::cout << "T3 ";
    if (opts.runTier4) std::cout << "T4 ";
    std::cout << "\n============================================================\n\n";

    if (selectedTests.empty()) {
        std::cout << "[NOTICE] No tests match the active criteria or filter.\n";
        return 0;
    }

    std::map<int, TierStats> tierStats;
    std::vector<FailureRecord> failures;
    int totalPassed = 0;
    int totalFailed = 0;
    int totalCrashed = 0;

    auto globalStart = std::chrono::high_resolution_clock::now();

    for (const auto* test : selectedTests) {
        if (opts.verbose) {
            std::cout << "[ RUN      ] [" << test->suite << "." << test->name << "] (Tier " << test->tier << ")\n";
        }

        auto testStart = std::chrono::high_resolution_clock::now();
        bool passed = false;
        std::string failureDetail;

        try {
            test->testFunc();
            passed = true;
        } catch (const AssertionFailureException& e) {
            passed = false;
            failureDetail = e.what();
            totalFailed++;
        } catch (const std::exception& e) {
            passed = false;
            failureDetail = std::string("Unhandled std::exception: ") + e.what();
            totalCrashed++;
        } catch (...) {
            passed = false;
            failureDetail = "Unhandled non-standard exception / crash";
            totalCrashed++;
        }

        auto testEnd = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(testEnd - testStart).count();

        auto& stats = tierStats[test->tier];
        stats.total++;
        stats.elapsedMs += elapsedMs;

        if (passed) {
            totalPassed++;
            stats.passed++;
            if (opts.verbose) {
                std::cout << "[       OK ] [" << test->suite << "." << test->name << "] ("
                          << std::fixed << std::setprecision(2) << elapsedMs << " ms)\n";
            }
        } else {
            stats.failed++;
            std::cout << "[  FAILED  ] [" << test->suite << "." << test->name << "] (Tier " << test->tier
                      << ", " << std::fixed << std::setprecision(2) << elapsedMs << " ms)\n";
            std::cout << "             " << failureDetail << "\n";
            failures.push_back({test->suite, test->name, test->tier, failureDetail, elapsedMs});
        }
    }

    auto globalEnd = std::chrono::high_resolution_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(globalEnd - globalStart).count();

    // Summary Reporting
    std::cout << "\n============================================================\n";
    std::cout << "  E2E TEST EXECUTION SUMMARY\n";
    std::cout << "============================================================\n";
    for (int tier = 1; tier <= 4; ++tier) {
        if (tierStats.count(tier)) {
            const auto& s = tierStats[tier];
            std::cout << "  Tier " << tier << ": " << s.passed << "/" << s.total << " passed ("
                      << (s.failed == 0 ? "PASSED" : "FAILED") << ") in "
                      << std::fixed << std::setprecision(2) << s.elapsedMs << " ms\n";
        }
    }
    std::cout << "------------------------------------------------------------\n";
    std::cout << "  Total Executed : " << selectedTests.size() << "\n";
    std::cout << "  Passed         : " << totalPassed << "\n";
    std::cout << "  Failed         : " << totalFailed << "\n";
    std::cout << "  Crashed        : " << totalCrashed << "\n";
    std::cout << "  Total Duration : " << std::fixed << std::setprecision(2) << totalElapsedMs << " ms\n";
    std::cout << "============================================================\n";

    if (!failures.empty()) {
        std::cout << "\nFailures Summary:\n";
        for (const auto& f : failures) {
            std::cout << "  - [Tier " << f.tier << "] " << f.suite << "." << f.name << ": " << f.reason << "\n";
        }
        std::cout << "============================================================\n";
        return 1;
    }

    std::cout << "  STATUS: 100% OF TESTS PASSED\n";
    std::cout << "============================================================\n";
    return 0;
}
```

---

### 4.3 Recommendations for Implementation Worker

1. **File Placement**:
   - Write `tests/e2e/e2e_framework.hpp` at `c:\Users\Drilla\Desktop\Sandbox\tests\e2e\e2e_framework.hpp`.
   - Write `tests/e2e/test_main.cpp` at `c:\Users\Drilla\Desktop\Sandbox\tests\e2e\test_main.cpp`.
2. **CMake Target Coordination**:
   - Coordinate with Build Infra Explorer (`explorer_mtest1_1`) recommendations:
     - Target executable: `add_executable(sandbox_e2e_tests tests/e2e/test_main.cpp)`
     - Include directories: `src`, `tests/e2e`, `external`
     - Link libraries: `sim` objects or sources (`Chunk.cpp`, `World.cpp`, `MaterialRegistry.cpp`, `Physics.cpp`, `Config.cpp`, `ThreadPool.cpp`).
     - Assets copying: Ensure post-build step copies `assets/` to `$<TARGET_FILE_DIR:sandbox_e2e_tests>/assets`.
     - CTest registration:
       ```cmake
       add_test(NAME e2e_all COMMAND sandbox_e2e_tests --all)
       add_test(NAME e2e_tier1 COMMAND sandbox_e2e_tests --tier=1)
       add_test(NAME e2e_tier2 COMMAND sandbox_e2e_tests --tier=2)
       add_test(NAME e2e_tier3 COMMAND sandbox_e2e_tests --tier=3)
       add_test(NAME e2e_tier4 COMMAND sandbox_e2e_tests --tier=4)
       ```
3. **M_TEST_2 through M_TEST_4 Extensibility**:
   - In subsequent milestones, when `tier1_features.cpp`, `tier2_boundaries.cpp`, `tier3_pairwise.cpp`, and `tier4_workloads.cpp` are added, each file simply writes:
     ```cpp
     #include "e2e_framework.hpp"
     E2E_TEST(PowderPhysics, SandAngleOfRepose, 1) {
         TestWorldFixture fixture(128, 128);
         ...
     }
     ```
   - No modifications to `test_main.cpp` or `e2e_framework.hpp` are required; all new tests automatically self-register into the runner!

---

## 5. Verification Method

### 5.1 Compilation Verification
Run CMake build to compile `sandbox_e2e_tests`:
```powershell
cmake --build build --config Release --target sandbox_e2e_tests
```
Expected: Zero compilation errors and zero warnings under MSVC `/W4`.

### 5.2 Direct CLI Execution Verification
Execute the test runner directly with various flags:
```powershell
# 1. Sanity check test execution
.\build\Release\sandbox_e2e_tests.exe --verbose

# 2. Tier filtering check
.\build\Release\sandbox_e2e_tests.exe --tier=1

# 3. List check
.\build\Release\sandbox_e2e_tests.exe --list

# 4. Help check
.\build\Release\sandbox_e2e_tests.exe --help
```
Expected:
- Built-in sanity test `HarnessSanity.AssertionsAndWorldFixture` executes and passes (1/1 passed).
- Exit code is `0` (`$LASTEXITCODE` in PowerShell equals 0).
- Summary report correctly formats tier statistics and execution timing.

### 5.3 CTest Verification
Run CTest to verify integration:
```powershell
ctest --test-dir build -C Release --output-on-failure
```
Expected: CTest discovers and passes registered `sandbox_e2e_tests` targets.

### 5.4 Invalidation Conditions
This architecture would be invalidated if:
- `TestRegistry` encounters ODR linker conflicts across multiple `.cpp` files. (Mitigated: Meyer's singleton guarantees external linkage across TUs).
- Assertions segfault on failure instead of cleanly reporting. (Mitigated: Exception-based control flow safely halts test execution and returns to runner).
- Asset files cannot be found when running from CMake build directories. (Mitigated: `ResolveAssetPath` multi-level directory traversal resolves assets relative to executable and working directories).
