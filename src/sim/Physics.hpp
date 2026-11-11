#pragma once

#include "sim/World.hpp"
#include "sim/MaterialRegistry.hpp"
#include "core/Math.hpp"
#include "core/Types.hpp"

namespace sandbox::sim {

class Physics {
private:
    core::FastRNG m_rng{1337u};
    core::u64     m_tickCount{0};

public:
    Physics() noexcept = default;
    explicit Physics(core::u32 seed) noexcept : m_rng(seed) {}

    // Main simulation step for all cellular automata
    void Step(World& world) noexcept;

    // Single chunk step (for multithreaded checkerboard phases)
    void StepChunk(World& world, Chunk& chunk) noexcept;

    [[nodiscard]] core::u64 GetTickCount() const noexcept { return m_tickCount; }
    void Reset() noexcept { m_tickCount = 0; }

private:
    void UpdateCell(World& world, core::i32 x, core::i32 y, core::u8 currentParity) noexcept;
    void UpdatePowder(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept;
    void UpdateLiquid(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept;
    void UpdateGas(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept;
    void UpdateFire(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept;

    // Symmetric mutual density displacement rule
    [[nodiscard]] bool CanDisplace(const Material& sourceMat, MaterialId targetMatId) const noexcept;
};

} // namespace sandbox::sim
