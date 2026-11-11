#pragma once

#include "Types.hpp"
#include <cmath>

namespace sandbox::core {

// Быстрый детерминированный генератор псевдослучайных чисел (Xorshift32)
class FastRNG {
private:
    u32 m_state{2463534242u};

public:
    constexpr explicit FastRNG(u32 seed = 2463534242u) noexcept
        : m_state(seed == 0 ? 2463534242u : seed) {}

    constexpr u32 NextU32() noexcept {
        u32 x = m_state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        m_state = x;
        return x;
    }

    // Случайное целое в диапазоне [minVal, maxVal]
    constexpr i32 NextInt(i32 minVal, i32 maxVal) noexcept {
        if (minVal >= maxVal) return minVal;
        u32 range = static_cast<u32>(maxVal - minVal + 1);
        return minVal + static_cast<i32>(NextU32() % range);
    }

    // Случайное вещественное в диапазоне [0.0, 1.0)
    constexpr f32 NextFloat() noexcept {
        return static_cast<f32>(NextU32() & 0x00FFFFFFu) / static_cast<f32>(0x01000000u);
    }

    // Случайный булев флаг с заданной вероятностью [0.0, 1.0]
    constexpr bool Chance(f32 probability) noexcept {
        return NextFloat() < probability;
    }
};

// Хэш координат для стабильного вариативного визуального шума пикселей
[[nodiscard]] constexpr u32 CoordinateHash(i32 x, i32 y, u32 seed = 1337u) noexcept {
    u32 h = seed ^ (static_cast<u32>(x) * 374761393u) ^ (static_cast<u32>(y) * 668265263u);
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Получить вариацию яркости/оттенка [-variation, +variation] на основе стабильного хэша координат
[[nodiscard]] constexpr i32 CellNoiseVariation(i32 x, i32 y, i32 maxVariation, u32 seed = 1337u) noexcept {
    if (maxVariation <= 0) return 0;
    u32 h = CoordinateHash(x, y, seed);
    i32 range = maxVariation * 2 + 1;
    return static_cast<i32>(h % static_cast<u32>(range)) - maxVariation;
}

// Быстрое евклидово расстояние в квадрате
[[nodiscard]] constexpr i32 DistanceSquared(i32 x1, i32 y1, i32 x2, i32 y2) noexcept {
    i32 dx = x2 - x1;
    i32 dy = y2 - y1;
    return dx * dx + dy * dy;
}

// Быстрый целочисленный знак (-1, 0, +1)
template <typename T>
[[nodiscard]] constexpr i32 Sign(T val) noexcept {
    return (T(0) < val) - (val < T(0));
}

} // namespace sandbox::core

