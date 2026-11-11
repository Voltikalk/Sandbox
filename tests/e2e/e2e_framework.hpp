#pragma once

#include "core/Types.hpp"
#include "core/Math.hpp"
#include "sim/Chunk.hpp"
#include "sim/World.hpp"
#include "sim/Material.hpp"
#include "sim/MaterialRegistry.hpp"
#include "sim/Physics.hpp"

#include <string>
#include <string_view>
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
#include <exception>

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

// ============================================================================
// 4. Test Registry & Auto-Registration
// ============================================================================
struct TestCase {
    std::string suite;
    std::string name;
    int tier{1}; // 1, 2, 3, 4, 5
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
            bool ok = sim::MaterialRegistry::Instance().LoadFromDirectory(dir);
            if (!ok) {
                std::cerr << "[ERROR] [TestWorldFixture] Failed to load materials from: " << dir << "\n";
            } else {
                s_loaded = true;
            }
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

using WorldFixture = TestWorldFixture;

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

inline void Ticks(sim::World& world, sim::Physics& physics, int count) {
    for (int i = 0; i < count; ++i) {
        physics.Step(world);
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

template <typename Predicate>
inline bool Until(sim::World& world, sim::Physics& physics, Predicate&& predicate, int maxTicks) {
    for (int i = 0; i < maxTicks; ++i) {
        if (predicate(world)) {
            return true;
        }
        physics.Step(world);
    }
    return predicate(world);
}

inline bool UntilSettled(sim::World& world, sim::Physics& physics, int maxTicks = 500) {
    for (int i = 0; i < maxTicks; ++i) {
        if (world.GetTotalActiveCells() == 0) {
            return true;
        }
        physics.Step(world);
    }
    return world.GetTotalActiveCells() == 0;
}

} // namespace step

} // namespace sandbox::test
