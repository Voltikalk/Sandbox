#pragma once

#include "core/Types.hpp"
#include "core/Math.hpp"
#include "sim/Material.hpp"
#include "sim/World.hpp"
#include "sim/Chunk.hpp"
#include "sim/MaterialRegistry.hpp"
#include <string>

namespace sandbox::sim {

struct ReactionResult {
    MaterialId productA{EMPTY_MATERIAL_ID};
    MaterialId productB{EMPTY_MATERIAL_ID};
    core::f32  heatDelta{0.0f};
    core::f32  probability{1.0f};
    bool       explode{false};
    core::f32  explosionPower{0.0f};
    core::i32  explosionRadius{0};
    bool       hasReaction{false};
};

using ReactionRule = ReactionResult;

class ReactionSystem {
public:
    static constexpr size_t MAX_MATERIALS = 256;

    static ReactionSystem& Instance() {
        static ReactionSystem instance;
        return instance;
    }

    ReactionSystem() { Clear(); }
    ReactionSystem(const ReactionSystem&) = delete;
    ReactionSystem& operator=(const ReactionSystem&) = delete;

    bool LoadFromFile(const std::string& jsonPath, const MaterialRegistry& registry);
    void RegisterReaction(MaterialId a, MaterialId b, const ReactionRule& rule);
    void Clear() noexcept;

    [[nodiscard]] inline const ReactionResult* GetReaction(MaterialId a, MaterialId b) const noexcept {
        if (a >= MAX_MATERIALS || b >= MAX_MATERIALS) return nullptr;
        return m_table[a][b].hasReaction ? &m_table[a][b] : nullptr;
    }

    void Step(World& world) noexcept;
    void StepChunk(World& world, Chunk& chunk) noexcept;

    [[nodiscard]] size_t GetRuleCount() const noexcept { return m_registeredRulesCount; }

private:
    ReactionResult m_table[MAX_MATERIALS][MAX_MATERIALS]{};
    size_t m_registeredRulesCount{0};
    core::FastRNG m_rng{4242u};
    core::u64 m_stepCount{0};
};

} // namespace sandbox::sim
