#include "sim/ReactionSystem.hpp"
#include "sim/ExplosionSystem.hpp"
#include "core/Log.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace sandbox::sim {

using json = nlohmann::json;

void ReactionSystem::Clear() noexcept {
    for (size_t i = 0; i < MAX_MATERIALS; ++i) {
        for (size_t j = 0; j < MAX_MATERIALS; ++j) {
            m_table[i][j] = ReactionResult{};
        }
    }
    m_registeredRulesCount = 0;
}

void ReactionSystem::RegisterReaction(MaterialId a, MaterialId b, const ReactionRule& rule) {
    if (a >= MAX_MATERIALS || b >= MAX_MATERIALS) return;

    // Direct entry: cell A turns into productA, cell B turns into productB
    ReactionResult r1 = rule;
    r1.hasReaction = true;
    m_table[a][b] = r1;

    // Symmetric inverse entry: cell B turns into productB, cell A turns into productA
    ReactionResult r2 = rule;
    r2.productA = rule.productB;
    r2.productB = rule.productA;
    r2.hasReaction = true;
    m_table[b][a] = r2;

    ++m_registeredRulesCount;
}

bool ReactionSystem::LoadFromFile(const std::string& jsonPath, const MaterialRegistry& registry) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::string fallback = "../" + jsonPath;
        file.open(fallback);
        if (!file.is_open()) {
            LOG_WARN("ReactionSystem", "Failed to open reactions file: ", jsonPath);
            return false;
        }
    }

    try {
        json root;
        file >> root;

        if (!root.contains("reactions") || !root["reactions"].is_array()) {
            LOG_WARN("ReactionSystem", "reactions.json does not contain a 'reactions' array");
            return false;
        }

        Clear();

        for (const auto& item : root["reactions"]) {
            std::string reactAName = item.value("reactant_a", item.value("reactantA", ""));
            std::string reactBName = item.value("reactant_b", item.value("reactantB", ""));
            std::string prodAName = item.value("product_a", item.value("productA", ""));
            std::string prodBName = item.value("product_b", item.value("productB", ""));

            if (reactAName.empty() || reactBName.empty() || prodAName.empty() || prodBName.empty()) {
                continue;
            }

            MaterialId idA = registry.FindByName(reactAName);
            MaterialId idB = registry.FindByName(reactBName);
            MaterialId idPA = registry.FindByName(prodAName);
            MaterialId idPB = registry.FindByName(prodBName);

            if (idA == EMPTY_MATERIAL_ID && reactAName != "air") continue;
            if (idB == EMPTY_MATERIAL_ID && reactBName != "air") continue;
            if (idPA == EMPTY_MATERIAL_ID && prodAName != "air") continue;
            if (idPB == EMPTY_MATERIAL_ID && prodBName != "air") continue;

            ReactionRule rule;
            rule.productA = idPA;
            rule.productB = idPB;
            rule.heatDelta = item.value("heat_delta", item.value("heatDelta", 0.0f));
            rule.probability = item.value("probability", 1.0f);
            rule.explode = item.value("explode", false);
            rule.explosionPower = item.value("explosion_power", item.value("explosionPower", 0.0f));
            rule.explosionRadius = item.value("explosion_radius", item.value("explosionRadius", 0));
            rule.hasReaction = true;

            RegisterReaction(idA, idB, rule);
        }

        LOG_INFO("ReactionSystem", "Loaded ", m_registeredRulesCount, " reaction rules.");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("ReactionSystem", "JSON parsing error in ", jsonPath, ": ", e.what());
        return false;
    }
}

void ReactionSystem::Step(World& world) noexcept {
    size_t chunkCount = world.GetChunkCount();
    for (size_t i = 0; i < chunkCount; ++i) {
        Chunk* chunk = world.GetChunkByIndex(i);
        if (chunk && !chunk->isSleeping && chunk->activeCellCount > 0) {
            StepChunk(world, *chunk);
        }
    }
    ++m_stepCount;
}

void ReactionSystem::StepChunk(World& world, Chunk& chunk) noexcept {
    core::i32 startX = chunk.chunkX * Chunk::SIZE;
    core::i32 startY = chunk.chunkY * Chunk::SIZE;

    static constexpr core::i32 DX[4] = {0, 0, -1, 1};
    static constexpr core::i32 DY[4] = {-1, 1, 0, 0};

    const auto& reg = MaterialRegistry::Instance();

    for (core::i32 ly = 0; ly < Chunk::SIZE; ++ly) {
        core::i32 rowOffset = ly << 6;
        core::i32 wy = startY + ly;

        for (core::i32 lx = 0; lx < Chunk::SIZE; ++lx) {
            core::i32 idx = rowOffset | lx;
            MaterialId matA = chunk.material[idx];
            if (matA == EMPTY_MATERIAL_ID) continue;

            core::i32 wx = startX + lx;
            core::u32 startD = m_rng.NextU32() & 3u;

            for (core::u32 d = 0; d < 4; ++d) {
                core::u32 dir = (startD + d) & 3u;
                core::i32 nwx = wx + DX[dir];
                core::i32 nwy = wy + DY[dir];

                if (!world.IsInBounds(nwx, nwy)) continue;

                MaterialId matB = world.GetCellMaterial(nwx, nwy);
                if (matB == EMPTY_MATERIAL_ID) continue;

                const ReactionResult* rx = GetReaction(matA, matB);
                if (!rx) continue;

                // Probability roll
                if (rx->probability < 1.0f && m_rng.NextFloat() > rx->probability) {
                    continue;
                }

                const auto& matProdA = reg.Get(rx->productA);
                const auto& matProdB = reg.Get(rx->productB);

                core::f32 tempA = chunk.temperature[idx] + rx->heatDelta;
                core::f32 tempB = world.GetCellTemperature(nwx, nwy) + rx->heatDelta;

                world.SetCell(wx, wy, rx->productA, tempA, FLAG_NONE, matProdA.lifetime);
                world.SetCell(nwx, nwy, rx->productB, tempB, FLAG_NONE, matProdB.lifetime);

                if (rx->explode) {
                    ExplosionSystem::Instance().QueueExplosion(nwx, nwy, rx->explosionPower, rx->explosionRadius);
                }

                break; // One reaction per cell per tick
            }
        }
    }
}

} // namespace sandbox::sim
