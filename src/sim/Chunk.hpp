#pragma once

#include "core/Types.hpp"
#include "sim/Material.hpp"
#include <algorithm>
#include <memory>

namespace sandbox::sim {

// Cell bit flags (u8)
enum CellFlags : core::u8 {
    FLAG_NONE     = 0x00,
    FLAG_PARITY   = 0x01, // Double-update prevention bit (matches current tick parity)
    FLAG_SLEEPING = 0x02, // Settled particle at rest
    FLAG_BURNING  = 0x04, // Particle actively burning
    FLAG_FALLING  = 0x08  // Dynamic falling particle
};

// 64x64 Structure-of-Arrays Chunk (alignas(64))
class alignas(core::CACHE_LINE_SIZE) Chunk {
public:
    static constexpr core::i32 SIZE = 64;
    static constexpr core::i32 AREA = SIZE * SIZE; // 4096 cells
    static constexpr core::u8  SLEEP_HYSTERESIS = 4; // Ticks of inactivity before sleep

    // --- Core SoA Arrays (32,768 bytes total = 32 KB L1 Data Cache resident) ---
    alignas(core::CACHE_LINE_SIZE) MaterialId material[AREA]{0};     // 8,192 bytes (128 cache lines)
    alignas(core::CACHE_LINE_SIZE) core::f32  temperature[AREA]{};  // 16,384 bytes (256 cache lines)
    alignas(core::CACHE_LINE_SIZE) core::u8   flags[AREA]{0};        // 4,096 bytes (64 cache lines)
    alignas(core::CACHE_LINE_SIZE) core::u8   lifetime[AREA]{0};     // 4,096 bytes (64 cache lines)

    // --- Chunk Metadata ---
    core::i32 chunkX{0};
    core::i32 chunkY{0};
    core::u32 activeCellCount{0};
    core::i32 minActiveY{SIZE}; // 64 (empty state: minActiveY > maxActiveY)
    core::i32 maxActiveY{-1};   // -1
    core::u8  sleepCounter{SLEEP_HYSTERESIS};
    bool      isDirty{true};
    bool      isSleeping{false};

    // Constructors
    Chunk() noexcept;
    Chunk(core::i32 cx, core::i32 cy) noexcept;

    // Fast 1D index from local coordinates: (ly << 6) | lx
    [[nodiscard]] static constexpr core::i32 Index(core::i32 lx, core::i32 ly) noexcept {
        return (ly << 6) | lx;
    }

    // Inverse: get local coordinates from 1D index
    static constexpr void FromIndex(core::i32 index, core::i32& lx, core::i32& ly) noexcept {
        lx = index & 63;
        ly = index >> 6;
    }

    // Checkerboard phase index: (cx & 1) | ((cy & 1) << 1) in [0..3]
    [[nodiscard]] constexpr core::u32 GetPhase() const noexcept {
        return (static_cast<core::u32>(chunkX) & 1u) |
               ((static_cast<core::u32>(chunkY) & 1u) << 1u);
    }

    // Active bounds management
    void ExpandActiveY(core::i32 ly) noexcept {
        if (minActiveY > maxActiveY) {
            minActiveY = ly;
            maxActiveY = ly;
        } else {
            if (ly < minActiveY) minActiveY = ly;
            if (ly > maxActiveY) maxActiveY = ly;
        }
    }

    void ResetActiveY() noexcept {
        minActiveY = SIZE; // 64
        maxActiveY = -1;
    }

    void ExpandActiveBounds(core::i32 ly) noexcept { ExpandActiveY(ly); }
    void ResetActiveBounds() noexcept { ResetActiveY(); }

    // Cell accessors
    void SetCell(core::i32 lx, core::i32 ly, MaterialId mat, core::f32 temp = 20.0f,
                 core::u8 flag = FLAG_NONE, core::u8 life = 0) noexcept;

    // State management
    void Clear(MaterialId fillMat = EMPTY_MATERIAL_ID, core::f32 defaultTemp = 20.0f) noexcept;
    void WakeUp() noexcept;
    void StepSleep() noexcept;
    void UpdateActiveBounds() noexcept;
};

// Static assertions ensuring exact memory layout and alignment contracts
static_assert(sizeof(Chunk::material) == 8192, "Material array must be 8192 bytes");
static_assert(sizeof(Chunk::temperature) == 16384, "Temperature array must be 16384 bytes");
static_assert(sizeof(Chunk::flags) == 4096, "Flags array must be 4096 bytes");
static_assert(sizeof(Chunk::lifetime) == 4096, "Lifetime array must be 4096 bytes");
static_assert(alignof(Chunk) == 64, "Chunk class must be 64-byte cache line aligned");

} // namespace sandbox::sim
