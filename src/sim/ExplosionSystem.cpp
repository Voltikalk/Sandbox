#include "sim/ExplosionSystem.hpp"
#include <cmath>
#include <algorithm>

namespace sandbox::sim {

void ExplosionSystem::QueueExplosion(const ExplosionEvent& event) {
    if (m_queue.size() < 1024) {
        m_queue.push_back(event);
    }
}

void ExplosionSystem::QueueExplosion(core::i32 x, core::i32 y, core::f32 power, core::i32 radius, core::f32 maxTemp) {
    QueueExplosion(ExplosionEvent{x, y, power, radius, maxTemp, 0});
}

void ExplosionSystem::TriggerExplosion(World& world, const ExplosionEvent& event) noexcept {
    std::vector<ExplosionEvent> secondaryQueue;
    ExecuteExplosionInternal(world, event, secondaryQueue);

    while (!secondaryQueue.empty()) {
        ExplosionEvent nextEvent = secondaryQueue.back();
        secondaryQueue.pop_back();
        if (nextEvent.depth < MAX_CHAIN_DEPTH) {
            ExecuteExplosionInternal(world, nextEvent, secondaryQueue);
        }
    }
}

void ExplosionSystem::Update(World& world) noexcept {
    if (m_queue.empty()) return;

    std::vector<ExplosionEvent> currentQueue = std::move(m_queue);
    m_queue.clear();

    for (const auto& ev : currentQueue) {
        TriggerExplosion(world, ev);
    }
}

void ExplosionSystem::ExecuteExplosionInternal(World& world, const ExplosionEvent& event, std::vector<ExplosionEvent>& secondaryQueue) noexcept {
    core::i32 R = std::clamp(event.radius, 1, MAX_EXPLOSION_RADIUS);
    core::i32 boxDim = 2 * R + 1;
    size_t bufferSize = static_cast<size_t>(boxDim) * static_cast<size_t>(boxDim);

    std::vector<CellBlastDamage> damageMap(bufferSize);

    core::i32 x0 = event.epicX;
    core::i32 y0 = event.epicY;

    // 1. Cast rays to all perimeter points of [-R, R] x [-R, R]
    // Top and Bottom horizontal edges
    for (core::i32 dx = -R; dx <= R; ++dx) {
        CastBresenhamRay(world, x0, y0, x0 + dx, y0 - R, event.power, R, event.maxTemperature, damageMap, boxDim);
        CastBresenhamRay(world, x0, y0, x0 + dx, y0 + R, event.power, R, event.maxTemperature, damageMap, boxDim);
    }
    // Left and Right vertical edges
    for (core::i32 dy = -R + 1; dy <= R - 1; ++dy) {
        CastBresenhamRay(world, x0, y0, x0 - R, y0 + dy, event.power, R, event.maxTemperature, damageMap, boxDim);
        CastBresenhamRay(world, x0, y0, x0 + R, y0 + dy, event.power, R, event.maxTemperature, damageMap, boxDim);
    }

    const auto& registry = MaterialRegistry::Instance();
    MaterialId fireId = registry.FindByName("fire");
    MaterialId smokeId = registry.FindByName("smoke");
    MaterialId ashId = registry.FindByName("ash");
    MaterialId sandId = registry.FindByName("sand");
    MaterialId tntId = registry.FindByName("tnt");
    MaterialId gunpowderId = registry.FindByName("gunpowder");

    // 2. Apply damage, thermal blast, and check chain reactions
    for (core::i32 dy = -R; dy <= R; ++dy) {
        for (core::i32 dx = -R; dx <= R; ++dx) {
            core::i32 localIdx = (dy + R) * boxDim + (dx + R);
            const auto& dmg = damageMap[localIdx];
            if (!dmg.visited || dmg.energy <= 0.0f) continue;

            core::i32 wx = x0 + dx;
            core::i32 wy = y0 + dy;
            if (!world.IsInBounds(wx, wy)) continue;

            MaterialId curMatId = world.GetCellMaterial(wx, wy);
            const Material& mat = registry.Get(curMatId);
            core::f32 curTemp = world.GetCellTemperature(wx, wy);

            // Thermal Flash
            core::f32 newTemp = curTemp + dmg.temperatureDelta;
            world.SetCell(wx, wy, curMatId, newTemp);

            // Chain Reaction Check (Thermal Ignition or Blast Detonation)
            bool detonated = false;
            if (curMatId == tntId && (newTemp >= mat.ignitionTemp || dmg.energy >= mat.strength)) {
                world.SetCell(wx, wy, EMPTY_MATERIAL_ID);
                secondaryQueue.push_back(ExplosionEvent{wx, wy, 120.0f, 25, 2000.0f, event.depth + 1});
                detonated = true;
            } else if (curMatId == gunpowderId && (newTemp >= mat.ignitionTemp || dmg.energy >= mat.strength)) {
                world.SetCell(wx, wy, EMPTY_MATERIAL_ID);
                secondaryQueue.push_back(ExplosionEvent{wx, wy, 60.0f, 15, 1200.0f, event.depth + 1});
                detonated = true;
            }

            if (detonated) continue;

            // Secondary Flammability Ignition (Wood, Oil)
            if (mat.flammability > 0.0f && newTemp >= mat.ignitionTemp) {
                world.SetCell(wx, wy, fireId, newTemp, FLAG_BURNING, 30);
                continue;
            }

            // Mechanical Destruction
            core::f32 dist = std::sqrt(static_cast<core::f32>(dx * dx + dy * dy));
            if (curMatId != EMPTY_MATERIAL_ID) {
                if (dmg.energy >= mat.strength) {
                    if (dist <= 0.4f * static_cast<core::f32>(R)) {
                        // Core pulverization
                        world.SetCell(wx, wy, EMPTY_MATERIAL_ID, newTemp);
                    } else {
                        // Debris generation
                        if (mat.name == "stone") {
                            MaterialId debrisMat = (m_rng.NextFloat() > 0.5f) ? sandId : ashId;
                            world.SetCell(wx, wy, debrisMat, newTemp);
                        } else if (mat.name == "wood") {
                            world.SetCell(wx, wy, ashId, newTemp);
                        } else {
                            world.SetCell(wx, wy, EMPTY_MATERIAL_ID, newTemp);
                        }
                    }
                }
            } else {
                // Air cell in fireball core
                if (dist <= 0.25f * static_cast<core::f32>(R)) {
                    world.SetCell(wx, wy, fireId, std::max(newTemp, 800.0f), FLAG_BURNING, 20);
                } else if (dist <= 0.6f * static_cast<core::f32>(R) && m_rng.NextFloat() < 0.25f) {
                    world.SetCell(wx, wy, smokeId, newTemp, FLAG_NONE, 40);
                }
            }
        }
    }
}

void ExplosionSystem::CastBresenhamRay(World& world,
                                       core::i32 x0, core::i32 y0,
                                       core::i32 x1, core::i32 y1,
                                       core::f32 initialEnergy,
                                       core::i32 radius,
                                       core::f32 maxTemp,
                                       std::vector<CellBlastDamage>& damageMap,
                                       core::i32 boxDim) noexcept {
    core::i32 dx = std::abs(x1 - x0);
    core::i32 dy = -std::abs(y1 - y0);
    core::i32 sx = (x0 < x1) ? 1 : -1;
    core::i32 sy = (y0 < y1) ? 1 : -1;
    core::i32 err = dx + dy;

    core::i32 curX = x0;
    core::i32 curY = y0;
    core::f32 rayEnergy = initialEnergy;
    core::f32 radF = static_cast<core::f32>(radius);
    const auto& registry = MaterialRegistry::Instance();

    while (true) {
        if (!world.IsInBounds(curX, curY)) break;

        core::i32 relX = curX - x0;
        core::i32 relY = curY - y0;
        core::f32 dist = std::sqrt(static_cast<core::f32>(relX * relX + relY * relY));
        if (dist > radF) break;

        core::f32 alpha = 1.0f - (dist / radF);
        core::f32 cellEnergy = rayEnergy * alpha;
        core::f32 flashTemp = maxTemp * alpha * (rayEnergy / initialEnergy);

        core::i32 localIdx = (relY + radius) * boxDim + (relX + radius);
        auto& cellDmg = damageMap[localIdx];
        cellDmg.energy = std::max(cellDmg.energy, cellEnergy);
        cellDmg.temperatureDelta = std::max(cellDmg.temperatureDelta, flashTemp);
        cellDmg.visited = true;

        // Skip attenuation on epicenter cell itself
        if (curX != x0 || curY != y0) {
            MaterialId matId = world.GetCellMaterial(curX, curY);
            const Material& mat = registry.Get(matId);
            core::f32 str = mat.strength;

            if (cellEnergy < str) {
                // Ray blocked by strong obstacle (stone/iron wall)
                break;
            } else {
                // Ray penetrates, losing energy proportional to strength
                rayEnergy = std::max(0.0f, rayEnergy - str);
                if (rayEnergy <= 0.0f) break;
            }
        }

        if (curX == x1 && curY == y1) break;

        core::i32 e2 = 2 * err;
        if (e2 >= dy) { err += dy; curX += sx; }
        if (e2 <= dx) { err += dx; curY += sy; }
    }
}

} // namespace sandbox::sim
