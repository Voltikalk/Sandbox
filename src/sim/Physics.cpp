#include "sim/Physics.hpp"
#include "sim/ExplosionSystem.hpp"
#include <algorithm>

namespace sandbox::sim {

void Physics::Step(World& world) noexcept {
    core::u8 currentParity = world.GetCurrentParity();
    core::i32 width = world.GetWidth();
    core::i32 height = world.GetHeight();

    // Bottom-to-top traversal prevents multi-cell cascading in a single tick
    for (core::i32 y = height - 1; y >= 0; --y) {
        // Alternating horizontal scan direction eliminates directional bias
        bool leftToRight = ((y + static_cast<core::i32>(m_tickCount)) & 1) != 0;

        if (leftToRight) {
            for (core::i32 x = 0; x < width; ++x) {
                UpdateCell(world, x, y, currentParity);
            }
        } else {
            for (core::i32 x = width - 1; x >= 0; --x) {
                UpdateCell(world, x, y, currentParity);
            }
        }
    }

    ++m_tickCount;
}

void Physics::StepChunk(World& world, Chunk& chunk) noexcept {
    if (chunk.isSleeping || chunk.activeCellCount == 0) return;

    core::u8 currentParity = world.GetCurrentParity();
    core::i32 startX = chunk.chunkX * Chunk::SIZE;
    core::i32 startY = chunk.chunkY * Chunk::SIZE;

    core::i32 minY = std::max(0, chunk.minActiveY);
    core::i32 maxY = std::min(Chunk::SIZE - 1, chunk.maxActiveY);

    for (core::i32 ly = maxY; ly >= minY; --ly) {
        core::i32 wy = startY + ly;
        bool leftToRight = ((wy + static_cast<core::i32>(m_tickCount)) & 1) != 0;

        if (leftToRight) {
            for (core::i32 lx = 0; lx < Chunk::SIZE; ++lx) {
                UpdateCell(world, startX + lx, wy, currentParity);
            }
        } else {
            for (core::i32 lx = Chunk::SIZE - 1; lx >= 0; --lx) {
                UpdateCell(world, startX + lx, wy, currentParity);
            }
        }
    }
}

void Physics::UpdateCell(World& world, core::i32 x, core::i32 y, core::u8 currentParity) noexcept {
    MaterialId matId = world.GetCellMaterial(x, y);
    if (matId == EMPTY_MATERIAL_ID) return;

    // Check parity bit: skip if already updated during this tick
    core::u8 flags = world.GetCellFlags(x, y);
    if ((flags & FLAG_PARITY) == currentParity) return;

    // Mark as visited in this tick so resting cells toggle parity properly
    world.SetCellFlags(x, y, (flags & ~FLAG_PARITY) | currentParity);

    const Material& mat = MaterialRegistry::Instance().Get(matId);

    if (mat.name == "fire") {
        UpdateFire(world, x, y, mat);
    } else if (mat.state == MaterialState::Powder) {
        UpdatePowder(world, x, y, mat);
    } else if (mat.state == MaterialState::Liquid) {
        UpdateLiquid(world, x, y, mat);
    } else if (mat.state == MaterialState::Gas) {
        UpdateGas(world, x, y, mat);
    }
}

bool Physics::CanDisplace(const Material& sourceMat, MaterialId targetMatId) const noexcept {
    // Air (empty) is always displaceable
    if (targetMatId == EMPTY_MATERIAL_ID) return true;

    const Material& targetMat = MaterialRegistry::Instance().Get(targetMatId);

    // Solids are rigid and cannot be displaced
    if (targetMat.state == MaterialState::Solid) return false;

    // Powders displace liquids with strictly lower density, and gases
    if (sourceMat.state == MaterialState::Powder) {
        if (targetMat.state == MaterialState::Liquid) {
            return sourceMat.density > targetMat.density;
        }
        if (targetMat.state == MaterialState::Gas) return true;
        return false; // Powders do not displace other powders (ensures stable slope)
    }

    // Liquids displace lighter liquids, and gases
    if (sourceMat.state == MaterialState::Liquid) {
        if (targetMat.state == MaterialState::Liquid) {
            return sourceMat.density > targetMat.density;
        }
        if (targetMat.state == MaterialState::Gas) return true;
        return false; // Liquids do not displace powders or solids
    }

    // Gases displace heavier gases upward (buoyancy)
    if (sourceMat.state == MaterialState::Gas) {
        if (targetMat.state == MaterialState::Gas) {
            return sourceMat.density < targetMat.density;
        }
        return false; // Gases cannot displace liquids, powders, or solids
    }

    return false;
}

void Physics::UpdatePowder(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept {
    core::i32 downY = y + 1;
    if (downY >= world.GetHeight()) return;

    // 1. Direct downward gravity
    MaterialId downMat = world.GetCellMaterial(x, downY);
    if (CanDisplace(mat, downMat)) {
        world.SwapCells(x, y, x, downY);
        return;
    }

    // 2. Diagonal sliding with randomized direction to prevent slope skewing
    core::i32 dir = (m_rng.NextU32() & 1u) ? 1 : -1;
    core::i32 dirs[2] = { dir, -dir };

    for (core::i32 i = 0; i < 2; ++i) {
        core::i32 diagX = x + dirs[i];
        if (!world.IsInBounds(diagX, downY)) continue;

        // Diagonal corner leak guard: do not squeeze if both adjacent orthos are solid
        MaterialId sideMat = world.GetCellMaterial(diagX, y);
        const auto& sideMatRef = MaterialRegistry::Instance().Get(sideMat);
        const auto& downMatRef = MaterialRegistry::Instance().Get(downMat);
        if (sideMatRef.state == MaterialState::Solid && downMatRef.state == MaterialState::Solid) {
            continue;
        }

        MaterialId diagMat = world.GetCellMaterial(diagX, downY);
        if (CanDisplace(mat, diagMat)) {
            world.SwapCells(x, y, diagX, downY);
            return;
        }
    }
}

void Physics::UpdateLiquid(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept {
    core::i32 downY = y + 1;

    // 1. Direct downward gravity
    if (downY < world.GetHeight()) {
        MaterialId downMat = world.GetCellMaterial(x, downY);
        if (CanDisplace(mat, downMat)) {
            world.SwapCells(x, y, x, downY);
            return;
        }

        // 2. Diagonal downward sliding
        core::i32 dir = (m_rng.NextU32() & 1u) ? 1 : -1;
        core::i32 dirs[2] = { dir, -dir };

        for (core::i32 i = 0; i < 2; ++i) {
            core::i32 diagX = x + dirs[i];
            if (!world.IsInBounds(diagX, downY)) continue;

            MaterialId sideMat = world.GetCellMaterial(diagX, y);
            const auto& sideMatRef = MaterialRegistry::Instance().Get(sideMat);
            const auto& downMatRef = MaterialRegistry::Instance().Get(downMat);
            if (sideMatRef.state == MaterialState::Solid && downMatRef.state == MaterialState::Solid) {
                continue;
            }

            MaterialId diagMat = world.GetCellMaterial(diagX, downY);
            if (CanDisplace(mat, diagMat)) {
                world.SwapCells(x, y, diagX, downY);
                return;
            }
        }
    }

    // 3. Lateral Dispersion with Cliff Drop-off Priority
    core::i32 dispersion = std::clamp(mat.dispersion, 1, 10);
    core::i32 primaryDir = (m_rng.NextU32() & 1u) ? 1 : -1;
    core::i32 dirs[2] = { primaryDir, -primaryDir };

    for (core::i32 dIdx = 0; dIdx < 2; ++dIdx) {
        core::i32 currentDir = dirs[dIdx];
        core::i32 bestX = x;

        for (core::i32 step = 1; step <= dispersion; ++step) {
            core::i32 targetX = x + currentDir * step;
            if (!world.IsInBounds(targetX, y)) break;

            MaterialId targetMat = world.GetCellMaterial(targetX, y);
            if (!CanDisplace(mat, targetMat)) break; // Raycast obstructed

            bestX = targetX;

            // CLIFF DROP-OFF CHECK:
            // Check support cell directly beneath the candidate
            core::i32 belowTargetY = y + 1;
            if (belowTargetY < world.GetHeight()) {
                MaterialId belowMat = world.GetCellMaterial(targetX, belowTargetY);
                if (CanDisplace(mat, belowMat)) {
                    // Cliff / hole discovered! Halt lateral expansion immediately
                    break;
                }
            }
        }

        if (bestX != x) {
            world.SwapCells(x, y, bestX, y);
            return;
        }
    }
}

void Physics::UpdateGas(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept {
    // 1. Lifetime decay
    if (mat.lifetime > 0) {
        core::u8 life = world.GetCellLifetime(x, y);
        if (life > 0) {
            --life;
            world.SetCellLifetime(x, y, life);
            if (life == 0) {
                // Dissipate into Air
                world.SetCell(x, y, EMPTY_MATERIAL_ID, world.GetCellTemperature(x, y));
                return;
            }
        }
    }

    core::i32 upY = y - 1;
    if (upY >= 0) {
        // 2. Brownian turbulent jitter (25% chance to check diagonals first)
        bool tryDiagonalFirst = (m_rng.NextFloat() < 0.25f);
        core::i32 dir = (m_rng.NextU32() & 1u) ? 1 : -1;
        core::i32 dirs[2] = { dir, -dir };

        if (tryDiagonalFirst) {
            for (core::i32 i = 0; i < 2; ++i) {
                core::i32 diagX = x + dirs[i];
                if (!world.IsInBounds(diagX, upY)) continue;

                MaterialId sideMat = world.GetCellMaterial(diagX, y);
                MaterialId upMat = world.GetCellMaterial(x, upY);
                const auto& sideMatRef = MaterialRegistry::Instance().Get(sideMat);
                const auto& upMatRef = MaterialRegistry::Instance().Get(upMat);
                if (sideMatRef.state == MaterialState::Solid && upMatRef.state == MaterialState::Solid) {
                    continue; // Corner leak guard
                }

                MaterialId diagMat = world.GetCellMaterial(diagX, upY);
                if (CanDisplace(mat, diagMat)) {
                    world.SwapCells(x, y, diagX, upY);
                    return;
                }
            }
        }

        // 3. Direct upward movement (buoyancy)
        MaterialId upMat = world.GetCellMaterial(x, upY);
        if (CanDisplace(mat, upMat)) {
            world.SwapCells(x, y, x, upY);
            return;
        }

        // 4. Diagonal upward if direct upward blocked
        if (!tryDiagonalFirst) {
            for (core::i32 i = 0; i < 2; ++i) {
                core::i32 diagX = x + dirs[i];
                if (!world.IsInBounds(diagX, upY)) continue;

                MaterialId sideMat = world.GetCellMaterial(diagX, y);
                const auto& sideMatRef = MaterialRegistry::Instance().Get(sideMat);
                const auto& upMatRef = MaterialRegistry::Instance().Get(upMat);
                if (sideMatRef.state == MaterialState::Solid && upMatRef.state == MaterialState::Solid) {
                    continue; // Corner leak guard
                }

                MaterialId diagMat = world.GetCellMaterial(diagX, upY);
                if (CanDisplace(mat, diagMat)) {
                    world.SwapCells(x, y, diagX, upY);
                    return;
                }
            }
        }
    }

    // 5. Lateral chimney dispersion under flat ceilings/roofs
    core::i32 dispersion = std::clamp(mat.dispersion, 1, 10);
    core::i32 primaryDir = (m_rng.NextU32() & 1u) ? 1 : -1;
    core::i32 dirs[2] = { primaryDir, -primaryDir };

    for (core::i32 dIdx = 0; dIdx < 2; ++dIdx) {
        core::i32 currentDir = dirs[dIdx];
        core::i32 bestX = x;

        for (core::i32 step = 1; step <= dispersion; ++step) {
            core::i32 targetX = x + currentDir * step;
            if (!world.IsInBounds(targetX, y)) break;

            MaterialId targetMat = world.GetCellMaterial(targetX, y);
            if (!CanDisplace(mat, targetMat)) break; // Obstructed

            bestX = targetX;

            // CHIMNEY / VENT PRIORITY:
            // Check if cell above candidate is open
            core::i32 aboveTargetY = y - 1;
            if (aboveTargetY >= 0) {
                MaterialId aboveMat = world.GetCellMaterial(targetX, aboveTargetY);
                if (CanDisplace(mat, aboveMat)) {
                    // Vent discovered! Stop lateral sweep so gas can escape upward next tick
                    break;
                }
            }
        }

        if (bestX != x) {
            world.SwapCells(x, y, bestX, y);
            return;
        }
    }
}

void Physics::UpdateFire(World& world, core::i32 x, core::i32 y, const Material& /*mat*/) noexcept {
    const auto& reg = MaterialRegistry::Instance();
    core::f32 curTemp = world.GetCellTemperature(x, y);

    // 1. Maintain high core temperature (400 - 800 C)
    if (curTemp < 400.0f) {
        core::f32 targetTemp = 400.0f + m_rng.NextFloat() * 400.0f;
        world.SetCellTemperature(x, y, targetTemp);
        curTemp = targetTemp;
    }

    // 2. Lifetime decay and product emission
    core::u8 life = world.GetCellLifetime(x, y);
    if (life > 0) {
        --life;
        world.SetCellLifetime(x, y, life);
        if (life == 0) {
            // Emits products: 65% Smoke, 15% Ash, 20% Air
            core::f32 roll = m_rng.NextFloat();
            if (roll < 0.65f) {
                MaterialId smokeId = reg.FindByName("smoke");
                const auto& smokeMat = reg.Get(smokeId);
                world.SetCell(x, y, smokeId, curTemp, FLAG_NONE, smokeMat.lifetime > 0 ? smokeMat.lifetime : 75);
            } else if (roll < 0.80f) {
                MaterialId ashId = reg.FindByName("ash");
                world.SetCell(x, y, ashId, curTemp, FLAG_NONE, 0);
            } else {
                world.SetCell(x, y, EMPTY_MATERIAL_ID, curTemp, FLAG_NONE, 0);
            }
            return;
        }
    }

    // 3. Neighbor ignition check (4-neighborhood)
    static constexpr core::i32 DX[4] = {0, 0, -1, 1};
    static constexpr core::i32 DY[4] = {-1, 1, 0, 0};

    MaterialId tntId = reg.FindByName("tnt");
    MaterialId gunpowderId = reg.FindByName("gunpowder");
    MaterialId fireId = reg.FindByName("fire");

    for (core::i32 d = 0; d < 4; ++d) {
        core::i32 nx = x + DX[d];
        core::i32 ny = y + DY[d];
        if (!world.IsInBounds(nx, ny)) continue;

        MaterialId nMatId = world.GetCellMaterial(nx, ny);
        if (nMatId == EMPTY_MATERIAL_ID) continue;

        const auto& nMat = reg.Get(nMatId);
        if (nMat.flammability > 0.0f) {
            // Heat neighbor
            core::f32 nTemp = world.GetCellTemperature(nx, ny) + 40.0f + m_rng.NextFloat() * 40.0f;
            world.SetCellTemperature(nx, ny, nTemp);

            if (nTemp >= nMat.ignitionTemp) {
                // Roll ignition based on material flammability
                if (m_rng.NextFloat() < nMat.flammability) {
                    if (nMatId == gunpowderId) {
                        world.SetCell(nx, ny, EMPTY_MATERIAL_ID);
                        ExplosionSystem::Instance().QueueExplosion(nx, ny, 50.0f, 15, 1200.0f);
                    } else if (nMatId == tntId) {
                        world.SetCell(nx, ny, EMPTY_MATERIAL_ID);
                        ExplosionSystem::Instance().QueueExplosion(nx, ny, 150.0f, 35, 2000.0f);
                    } else {
                        // Flammable solid / liquid (Wood, Oil)
                        world.SetCell(nx, ny, fireId, 600.0f, FLAG_BURNING | world.GetCurrentParity(), 30);
                    }
                }
            }
        }
    }

    // 4. Upward flame flicker
    core::i32 upY = y - 1;
    if (upY >= 0) {
        core::i32 dir = (m_rng.NextU32() & 1u) ? 1 : -1;
        core::i32 fx = (m_rng.NextFloat() < 0.6f) ? x : (x + dir);
        if (world.IsInBounds(fx, upY) && world.GetCellMaterial(fx, upY) == EMPTY_MATERIAL_ID) {
            world.SwapCells(x, y, fx, upY);
            return;
        }
    }
}

} // namespace sandbox::sim
