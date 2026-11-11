#pragma once

#include "sim/World.hpp"
#include "sim/Chunk.hpp"
#include "sim/MaterialRegistry.hpp"
#include "core/Types.hpp"

namespace sandbox::sim {

class Thermodynamics {
public:
    static constexpr core::f32 T_AMBIENT = 20.0f;
    static constexpr core::f32 EPSILON_CONDUCTIVITY = 1e-6f;
    static constexpr core::f32 MAX_TRANSFER_PER_NEIGHBOR = 0.25f;
    static constexpr core::f32 LATENT_HEAT_OFFSET = 3.0f;
    static constexpr core::f32 AMBIENT_COOLING_SOLID = 0.001f;
    static constexpr core::f32 AMBIENT_COOLING_AIR = 0.005f;
    static constexpr core::f32 THERMAL_EQUILIBRIUM_THRESHOLD = 0.05f;

private:
    // Precomputed 256x256 harmonic conductivity lookup table for zero-division O(1) thermal transfer
    alignas(core::CACHE_LINE_SIZE) core::f32 m_contactConductivity[256][256]{};
    size_t m_lastRegistryCount{0};
    bool m_tableInitialized{false};

public:
    Thermodynamics() noexcept;

    // Initialize/rebuild the fast harmonic conductivity lookup table from MaterialRegistry
    void InitLookupTable(const MaterialRegistry& registry) noexcept;

    // Full world thermal step
    void Step(World& world) noexcept;

    // Single chunk thermal step (compatible with 4-phase checkerboard parallel dispatch)
    void StepChunk(World& world, Chunk& chunk) noexcept;

    // Harmonic contact conductivity: k_contact = (2 * k1 * k2) / (k1 + k2 + epsilon)
    [[nodiscard]] static constexpr core::f32 CalculateContactConductivity(core::f32 k1, core::f32 k2) noexcept {
        return (2.0f * k1 * k2) / (k1 + k2 + EPSILON_CONDUCTIVITY);
    }

    // Direct lookup from precomputed table
    [[nodiscard]] core::f32 GetContactConductivity(MaterialId id1, MaterialId id2) noexcept {
        if (!m_tableInitialized) {
            InitLookupTable(MaterialRegistry::Instance());
        }
        return m_contactConductivity[id1 & 0xFF][id2 & 0xFF];
    }

private:
    // Performs phase transition check on cell; returns true if material changed
    bool CheckPhaseTransition(World& world, Chunk& chunk, core::i32 lx, core::i32 ly,
                              core::i32 wx, core::i32 wy, core::f32& currentTemp) noexcept;
};

} // namespace sandbox::sim
