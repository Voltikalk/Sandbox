#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>

namespace sandbox::core {

// Фиксированные целочисленные и вещественные типы
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using f32 = float;
using f64 = double;

// Константы выравнивания процессора
inline constexpr size_t CACHE_LINE_SIZE = 64;

// Размеры чанков симуляции
inline constexpr i32 CHUNK_SIZE = 64;
inline constexpr i32 CHUNK_AREA = CHUNK_SIZE * CHUNK_SIZE; // 4096

// Точка в двумерном пространстве
struct Point2i {
    i32 x{0};
    i32 y{0};

    constexpr bool operator==(const Point2i& other) const noexcept {
        return x == other.x && y == other.y;
    }
    constexpr bool operator!=(const Point2i& other) const noexcept {
        return !(*this == other);
    }
    constexpr Point2i operator+(const Point2i& other) const noexcept {
        return {x + other.x, y + other.y};
    }
    constexpr Point2i operator-(const Point2i& other) const noexcept {
        return {x - other.x, y - other.y};
    }
};

// Прямоугольник (AABB)
struct Rect2i {
    i32 x{0};
    i32 y{0};
    i32 width{0};
    i32 height{0};

    [[nodiscard]] constexpr bool Contains(i32 px, i32 py) const noexcept {
        return px >= x && px < (x + width) && py >= y && py < (y + height);
    }
};

// 32-битный RGBA цвет
struct alignas(4) Color4u {
    u8 r{0};
    u8 g{0};
    u8 b{0};
    u8 a{255};

    constexpr Color4u() noexcept = default;
    constexpr Color4u(u8 red, u8 green, u8 blue, u8 alpha = 255) noexcept
        : r(red), g(green), b(blue), a(alpha) {}

    // Упаковка в 32-битный u32 формата RGBA8888 (Little-Endian: ABGR в памяти)
    [[nodiscard]] constexpr u32 ToU32() const noexcept {
        return static_cast<u32>(r) |
              (static_cast<u32>(g) << 8) |
              (static_cast<u32>(b) << 16) |
              (static_cast<u32>(a) << 24);
    }

    [[nodiscard]] static constexpr Color4u FromU32(u32 val) noexcept {
        return Color4u(
            static_cast<u8>(val & 0xFF),
            static_cast<u8>((val >> 8) & 0xFF),
            static_cast<u8>((val >> 16) & 0xFF),
            static_cast<u8>((val >> 24) & 0xFF)
        );
    }

    // Линейная интерполяция цветов
    [[nodiscard]] static constexpr Color4u Lerp(Color4u c1, Color4u c2, f32 t) noexcept {
        t = std::clamp(t, 0.0f, 1.0f);
        return Color4u(
            static_cast<u8>(c1.r + static_cast<f32>(c2.r - c1.r) * t),
            static_cast<u8>(c1.g + static_cast<f32>(c2.g - c1.g) * t),
            static_cast<u8>(c1.b + static_cast<f32>(c2.b - c1.b) * t),
            static_cast<u8>(c1.a + static_cast<f32>(c2.a - c1.a) * t)
        );
    }
};

} // namespace sandbox::core

