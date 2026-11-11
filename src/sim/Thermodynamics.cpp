#include "sim/Thermodynamics.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace sandbox::sim {

Thermodynamics::Thermodynamics() noexcept {
    InitLookupTable(MaterialRegistry::Instance());
}

void Thermodynamics::InitLookupTable(const MaterialRegistry& registry) noexcept {
    size_t count = registry.Count();
    for (size_t i = 0; i < 256; ++i) {
        core::f32 k1 = (i < count) ? registry.Get(static_cast<MaterialId>(i)).thermalConductivity : 0.02f;
        for (size_t j = 0; j < 256; ++j) {
            core::f32 k2 = (j < count) ? registry.Get(static_cast<MaterialId>(j)).thermalConductivity : 0.02f;
            m_contactConductivity[i][j] = CalculateContactConductivity(k1, k2);
        }
    }
    m_lastRegistryCount = count;
    m_tableInitialized = (count > 0);
}

void Thermodynamics::Step(World& world) noexcept {
    const auto& reg = MaterialRegistry::Instance();
    if (!m_tableInitialized || reg.Count() != m_lastRegistryCount) {
        InitLookupTable(reg);
    }

    size_t chunkCount = world.GetChunkCount();
    for (size_t i = 0; i < chunkCount; ++i) {
        Chunk* chunk = world.GetChunkByIndex(i);
        if (chunk && !chunk->isSleeping) {
            StepChunk(world, *chunk);
        }
    }
}

void Thermodynamics::StepChunk(World& world, Chunk& chunk) noexcept {
    const auto& reg = MaterialRegistry::Instance();
    if (!m_tableInitialized || reg.Count() != m_lastRegistryCount) {
        InitLookupTable(reg);
    }

    // 16 KB scratch buffer fitting in L1 cache
    alignas(core::CACHE_LINE_SIZE) core::f32 nextTemp[Chunk::AREA];
    std::memcpy(nextTemp, chunk.temperature, sizeof(nextTemp));

    core::i32 cx = chunk.chunkX;
    core::i32 cy = chunk.chunkY;
    core::i32 startX = cx * Chunk::SIZE;
    core::i32 startY = cy * Chunk::SIZE;

    bool chunkHasThermalActivity = false;

    // 4-neighborhood offsets: {up, down, left, right}
    static constexpr core::i32 DX[4] = {0, 0, -1, 1};
    static constexpr core::i32 DY[4] = {-1, 1, 0, 0};

    for (core::i32 ly = 0; ly < Chunk::SIZE; ++ly) {
        core::i32 rowOffset = ly << 6;
        core::i32 wy = startY + ly;

        for (core::i32 lx = 0; lx < Chunk::SIZE; ++lx) {
            core::i32 idx = rowOffset | lx;
            MaterialId mat0 = chunk.material[idx];
            core::f32 t0 = chunk.temperature[idx];
            core::i32 wx = startX + lx;

            core::f32 deltaT = 0.0f;

            for (core::i32 d = 0; d < 4; ++d) {
                core::i32 nlx = lx + DX[d];
                core::i32 nly = ly + DY[d];

                MaterialId nMat;
                core::f32 nTemp;

                if (nlx >= 0 && nlx < Chunk::SIZE && nly >= 0 && nly < Chunk::SIZE) {
                    // Intra-chunk: direct L1 cache access
                    core::i32 nIdx = (nly << 6) | nlx;
                    nMat = chunk.material[nIdx];
                    nTemp = chunk.temperature[nIdx];
                } else {
                    // Inter-chunk: world lookup
                    core::i32 nwx = wx + DX[d];
                    core::i32 nwy = wy + DY[d];
                    if (!world.IsInBounds(nwx, nwy)) {
                        continue; // Adiabatic boundary: zero flux
                    }
                    nMat = world.GetCellMaterial(nwx, nwy);
                    nTemp = world.GetCellTemperature(nwx, nwy);
                }

                core::f32 k_contact = GetContactConductivity(mat0, nMat);
                core::f32 transfer = MAX_TRANSFER_PER_NEIGHBOR * k_contact * (nTemp - t0);
                deltaT += transfer;

                // If cross-boundary transfer is significant, wake neighbor chunk
                if ((nlx < 0 || nlx >= Chunk::SIZE || nly < 0 || nly >= Chunk::SIZE) && std::abs(transfer) > 0.05f) {
                    world.WakeNeighborsForCell(cx, cy, lx, ly);
                }
            }

            // Ambient cooling/warming towards T_AMBIENT = 20.0 C
            core::f32 ambientRate = (mat0 == EMPTY_MATERIAL_ID) ? AMBIENT_COOLING_AIR : AMBIENT_COOLING_SOLID;
            deltaT += ambientRate * (T_AMBIENT - t0);

            core::f32 newTemp = t0 + deltaT;

            // Equilibrium snapping to ambient
            if (std::abs(newTemp - T_AMBIENT) < THERMAL_EQUILIBRIUM_THRESHOLD) {
                newTemp = T_AMBIENT;
            } else {
                chunkHasThermalActivity = true;
            }

            // Phase transition check
            CheckPhaseTransition(world, chunk, lx, ly, wx, wy, newTemp);

            nextTemp[idx] = newTemp;
        }
    }

    // Write back updated temperatures
    std::memcpy(chunk.temperature, nextTemp, sizeof(nextTemp));

    if (chunkHasThermalActivity) {
        chunk.WakeUp();
    }
}

bool Thermodynamics::CheckPhaseTransition(World& world, Chunk& chunk, core::i32 lx, core::i32 ly,
                                          core::i32 wx, core::i32 wy, core::f32& currentTemp) noexcept {
    core::i32 idx = Chunk::Index(lx, ly);
    MaterialId matId = chunk.material[idx];
    if (matId == EMPTY_MATERIAL_ID) return false;

    const Material& mat = MaterialRegistry::Instance().Get(matId);

    // 1. Boiling / Vaporization (e.g. Water -> Steam)
    if (mat.boilMaterialId != EMPTY_MATERIAL_ID && currentTemp >= mat.boilPoint + LATENT_HEAT_OFFSET) {
        MaterialId newId = mat.boilMaterialId;
        const Material& newMat = MaterialRegistry::Instance().Get(newId);
        currentTemp -= LATENT_HEAT_OFFSET; // Latent heat absorbed
        world.SetCell(wx, wy, newId, currentTemp, FLAG_NONE, newMat.lifetime);
        return true;
    }

    // 2. Melting / Liquefaction (e.g. Ice -> Water, Stone -> Lava, Iron -> Lava)
    if (mat.meltMaterialId != EMPTY_MATERIAL_ID && currentTemp >= mat.meltPoint + LATENT_HEAT_OFFSET) {
        MaterialId newId = mat.meltMaterialId;
        const Material& newMat = MaterialRegistry::Instance().Get(newId);
        currentTemp -= LATENT_HEAT_OFFSET; // Latent heat absorbed
        world.SetCell(wx, wy, newId, currentTemp, FLAG_NONE, newMat.lifetime);
        return true;
    }

    // 3. Freezing / Condensation / Solidification (e.g. Water -> Ice, Steam -> Water, Lava -> Stone)
    if (mat.freezeMaterialId != EMPTY_MATERIAL_ID && currentTemp <= mat.freezePoint - LATENT_HEAT_OFFSET) {
        MaterialId newId = mat.freezeMaterialId;
        const Material& newMat = MaterialRegistry::Instance().Get(newId);
        currentTemp += LATENT_HEAT_OFFSET; // Latent heat released
        world.SetCell(wx, wy, newId, currentTemp, FLAG_NONE, newMat.lifetime);
        return true;
    }

    return false;
}

} // namespace sandbox::sim
