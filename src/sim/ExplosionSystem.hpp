#pragma once

#include "core/Types.hpp"
#include "core/Math.hpp"
#include "sim/World.hpp"
#include "sim/MaterialRegistry.hpp"
#include <vector>

namespace sandbox::sim {

struct ExplosionEvent {
    core::i32 epicX{0};
    core::i32 epicY{0};
    core::f32 power{100.0f};           // Base blast energy (e.g. 50.0 to 200.0)
    core::i32 radius{20};              // Blast radius in cells
    core::f32 maxTemperature{1500.0f}; // Peak fireball temperature in °C
    core::u32 depth{0};                // Cascade recursion guard
};

struct CellBlastDamage {
    core::f32 energy{0.0f};
    core::f32 temperatureDelta{0.0f};
    bool visited{false};
};

class ExplosionSystem {
public:
    static constexpr core::u32 MAX_CHAIN_DEPTH = 32;
    static constexpr core::i32 MAX_EXPLOSION_RADIUS = 64;

    static ExplosionSystem& Instance() {
        static ExplosionSystem instance;
        return instance;
    }

    ExplosionSystem() noexcept = default;
    explicit ExplosionSystem(core::u32 seed) noexcept : m_rng(seed) {}

    // Queue an explosion for execution in the current tick
    void QueueExplosion(const ExplosionEvent& event);
    void QueueExplosion(core::i32 x, core::i32 y, core::f32 power, core::i32 radius, core::f32 maxTemp = 1500.0f);

    // Process all pending queued explosions and their chain reactions
    void Update(World& world) noexcept;

    // Direct synchronous explosion invocation
    void TriggerExplosion(World& world, const ExplosionEvent& event) noexcept;

    void ClearQueue() noexcept { m_queue.clear(); }
    [[nodiscard]] size_t GetQueueSize() const noexcept { return m_queue.size(); }

private:
    std::vector<ExplosionEvent> m_queue;
    core::FastRNG m_rng{4242u};

    void ExecuteExplosionInternal(World& world, const ExplosionEvent& event, std::vector<ExplosionEvent>& secondaryQueue) noexcept;

    void CastBresenhamRay(World& world,
                          core::i32 x0, core::i32 y0,
                          core::i32 x1, core::i32 y1,
                          core::f32 initialEnergy,
                          core::i32 radius,
                          core::f32 maxTemp,
                          std::vector<CellBlastDamage>& damageMap,
                          core::i32 boxDim) noexcept;
};

} // namespace sandbox::sim
