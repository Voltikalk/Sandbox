# Milestone 2 Explorer 3 Handoff Report: ExplosionSystem, Simulation Pipeline, and Unit Testing Architecture

**Author**: Explorer 3 (`teamwork_explorer_m2_3`)  
**Parent Orchestrator**: `452cac6c-0128-4a9c-9ecf-dde17c8213e1`  
**Milestone**: Milestone 2 (Thermodynamics, Reactions, Gases & Explosions — ШАГ 3)  
**Date**: 2026-11-11  

---

## 1. Observation

### 1.1 Existing Material Properties Relevant to Explosions and Obstacles
Direct inspection of material asset files (`assets/materials/*.json`) reveals the exact physical parameters configured for solids, powders, gases, and energetic materials:

1. **`assets/materials/tnt.json`**:
   - `id`: 15, `state`: "Solid", `strength`: 40.0, `flammability`: 0.90, `ignition_temp`: 250.0.
2. **`assets/materials/gunpowder.json`**:
   - `id`: 14, `state`: "Powder", `strength`: 10.0, `flammability`: 0.95, `ignition_temp`: 200.0.
3. **`assets/materials/stone.json`**:
   - `id`: 1, `state`: "Solid", `strength`: 80.0, `flammability`: 0.0, `ignition_temp`: 10000.0.
4. **`assets/materials/iron.json`**:
   - `id`: 13, `state`: "Solid", `strength`: 150.0, `flammability`: 0.0, `ignition_temp`: 10000.0.
5. **`assets/materials/wood.json`**:
   - `id`: 12, `state`: "Solid", `strength`: 30.0, `flammability`: 0.60, `ignition_temp`: 250.0.
6. **`assets/materials/sand.json`**:
   - `id`: 3, `state`: "Powder", `strength`: 15.0, `flammability`: 0.0, `ignition_temp`: 10000.0.
7. **`assets/materials/air.json`**:
   - `id`: 0, `state`: "Gas", `strength`: 0.0, `flammability`: 0.0, `ignition_temp`: 10000.0.
8. **`assets/materials/fire.json`**:
   - `id`: 10, `state`: "Gas", `strength`: 0.0, `flammability`: 1.00, `ignition_temp`: 0.0, `lifetime`: 30.
9. **`assets/materials/smoke.json`**:
   - `id`: 11, `state`: "Gas", `strength`: 0.0, `flammability`: 0.0, `ignition_temp`: 10000.0, `lifetime`: 75.
10. **`assets/materials/ash.json`**:
    - `id`: 17, `state`: "Powder", `strength`: 5.0, `flammability`: 0.0, `ignition_temp`: 10000.0, `lifetime`: 0.

### 1.2 Existing World and Simulation APIs
Direct inspection of `src/sim/World.hpp` and `src/sim/Chunk.hpp`:
- `World::SetCell(x, y, mat, temp, flags, lifetime)`: Thread-safe intra/inter-chunk setter updating `activeCellCount`, active vertical bounds (`minActiveY`/`maxActiveY`), and boundary wake propagation (`WakeNeighborsForCell`).
- `World::GetCellMaterial(x, y)`: $O(1)$ material ID lookup.
- `World::GetCellTemperature(x, y)`: $O(1)$ temperature lookup.
- `World::SwapCells(x1, y1, x2, y2)`: Atomic property swap with parity stamping.
- `World::AdvanceTick()`: Toggles parity bit `m_currentParity ^= FLAG_PARITY`.
- Currently, `src/main.cpp` lines 204-206 only execute:
  ```cpp
  physics.Step(world);
  world.AdvanceTick();
  simTickCount++;
  ```
  Neither `Thermodynamics`, `ReactionSystem`, nor `ExplosionSystem` are yet called in the game loop.

### 1.3 Existing Build System Layout
Direct inspection of `CMakeLists.txt` and `tests/CMakeLists.txt`:
- `CMakeLists.txt` lines 67-72 defines:
  ```cmake
  set(SIM_SOURCES
      ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Chunk.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/World.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/MaterialRegistry.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Physics.cpp
  )
  ```
- `tests/CMakeLists.txt` contains test targets `sandbox_e2e_tests`, `test_physics`, and `test_challenger2_stress`.
- `tests/test_physics.cpp` uses standard self-contained assertions (`#define TEST_ASSERT(cond)`) with exit code 0 on complete pass.

---

## 2. Logic Chain

### 2.1 ExplosionSystem Architecture & Raycasting Mechanics

#### The Problem with Radial Distance Checks vs. Raycasting
A naive explosion model uses Euclidean distance $r \le R$. In such a model, an explosion occurring outside a 5-cell thick stone bunker would instantaneously vaporize the contents inside the bunker because distance ignores intervening material geometry.

To model real physical shockwaves, the engine must simulate **energy propagation along sightlines** with **attenuation through matter**:
1. High-strength materials (Stone: strength 80, Iron: strength 150) absorb energy.
2. If the blast energy drops to zero, the shockwave cannot penetrate further along that ray.
3. Cells behind the obstacle are in the **blast shadow** (shielded).

#### 2-Phase Raycast Shockwave Design
To eliminate duplicate cell destruction and avoid overlapping ray recalculations:

```
[Phase 1: Raycasting]
  Cast 8R perimeter rays using Bresenham algorithm
  Trace outward from (x0, y0) to boundary of [-R, R] x [-R, R]
  Calculate distance falloff: alpha = 1.0 - (r / R)
  Attenuate ray energy by material strength: E_ray = E_ray - S_material
  If E_cell < S_material -> Ray STOPS (obstacle shielding!)
  Accumulate max blast energy & flash temp into local bounding box buffer:
    localEnergy[localIdx] = max(localEnergy[localIdx], E_cell)
    localTemp[localIdx]   = max(localTemp[localIdx], T_flash)

                 │
                 ▼
[Phase 2: Cell Modification & Debris Generation]
  For each visited cell with localEnergy > 0:
    1. Apply thermal flash heating: T_new = max(T_cur, T_cur + localTemp)
    2. Check ignition: if T_new >= mat.ignitionTemp and flammability > 0:
         - TNT / Gunpowder -> DETONATE (queue secondary explosion)
         - Wood / Oil -> Transform to Fire
    3. Check mechanical destruction: if localEnergy >= mat.strength:
         - Vaporize / pulverize to Air near epicenter (r <= 0.4 R)
         - Convert to debris at mid-distance (Stone -> Sand/Ash, Wood -> Ash/Fire)
         - Air cells in core (r <= 0.3 R) convert to Fire particles

                 │
                 ▼
[Phase 3: Chain Reaction Drain]
  Iteratively process secondary explosions from queue
  Depth guard prevents infinite recursion: if (event.depth < MAX_CHAIN_DEPTH)
```

#### Mathematical Formulas for Ray Energy & Thermal Flash
For an explosion with epicenter $(x_0, y_0)$, initial blast energy $E_0$, max radius $R$, and peak flash temperature $T_{\text{max}} = 1500^\circ\text{C}$:

1. **Euclidean Distance**:
   $$r = \sqrt{(x - x_0)^2 + (y - y_0)^2}$$

2. **Geometric Falloff Factor**:
   $$\alpha(r) = \max\left(0.0f, 1.0f - \frac{r}{R}\right)$$

3. **Cell Blast Energy along Ray**:
   $$E_{\text{cell}}(r) = E_{\text{ray}} \cdot \alpha(r)$$

4. **Flash Temperature Addition**:
   $$\Delta T(r) = T_{\text{max}} \cdot \alpha(r) \cdot \left(\frac{E_{\text{ray}}}{E_0}\right)$$

5. **Material Absorption & Ray Termination**:
   When stepping into a cell with material of strength $S$:
   $$\begin{cases}
   E_{\text{ray}} \leftarrow \max(0.0f, E_{\text{ray}} - S) & \text{if } E_{\text{cell}}(r) \ge S \\
   E_{\text{ray}} \leftarrow 0.0f & \text{if } E_{\text{cell}}(r) < S \quad (\textbf{RAY BLOCKED})
   \end{cases}$$

6. **Perimeter Ray Generation ($8R$ rays)**:
   By casting rays to all points on the square perimeter $[-R, R] \times [-R, R]$:
   - Top edge: $(x_0 + dx, y_0 - R)$ for $dx \in [-R, R]$ ($2R+1$ rays)
   - Bottom edge: $(x_0 + dx, y_0 + R)$ for $dx \in [-R, R]$ ($2R+1$ rays)
   - Left edge: $(x_0 - R, y_0 + dy)$ for $dy \in [-R+1, R-1]$ ($2R-1$ rays)
   - Right edge: $(x_0 + R, y_0 + dy)$ for $dy \in [-R+1, R-1]$ ($2R-1$ rays)
   Total rays: exactly $8R$. For $R=20$, $8 \times 20 = 160$ rays.
   **Coverage Theorem**: Every discrete grid cell $(x, y)$ inside the circle $r \le R$ is traversed by at least one ray. Zero angular gaps exist.

#### Secondary Chain Reaction Cascade
- TNT cell hit by $T \ge 250^\circ\text{C}$ or $E \ge 40.0$:
  Immediately sets cell to `EMPTY_MATERIAL_ID` (or Fire) to avoid re-triggering.
  Pushes `ExplosionEvent{x, y, power = 120.0f, radius = 25, maxTemp = 2000.0f, depth = curDepth + 1}`.
- Gunpowder cell hit by $T \ge 200^\circ\text{C}$ or $E \ge 10.0$:
  Pushes `ExplosionEvent{x, y, power = 60.0f, radius = 15, maxTemp = 1200.0f, depth = curDepth + 1}`.
- Iterative loop processes secondary events up to `MAX_CHAIN_DEPTH = 32`, guaranteeing zero stack overflow and bounded execution time.

---

### 2.2 Proposed Interface & Implementation for `ExplosionSystem`

#### Header: `src/sim/ExplosionSystem.hpp`
```cpp
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
    core::f32 power{100.0f};          // Base blast energy (e.g. 50.0 to 200.0)
    core::i32 radius{20};             // Blast radius in cells
    core::f32 maxTemperature{1500.0f};// Peak fireball temperature in °C
    core::u32 depth{0};              // Cascade recursion guard
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

private:
    std::vector<ExplosionEvent> m_queue;
    core::FastRNG m_rng{4242u};

public:
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
```

#### Implementation Highlights: `src/sim/ExplosionSystem.cpp`
```cpp
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
                        world.SetCell(wx, wy, EMPTY_MATERIAL_ID);
                    } else {
                        // Debris generation
                        if (mat.name == "stone") {
                            world.SetCell(wx, wy, (m_rng.NextFloat01() > 0.5f) ? sandId : ashId, newTemp);
                        } else if (mat.name == "wood") {
                            world.SetCell(wx, wy, ashId, newTemp);
                        } else {
                            world.SetCell(wx, wy, EMPTY_MATERIAL_ID);
                        }
                    }
                }
            } else {
                // Air cell in fireball core
                if (dist <= 0.25f * static_cast<core::f32>(R)) {
                    world.SetCell(wx, wy, fireId, std::max(newTemp, 800.0f), FLAG_BURNING, 20);
                } else if (dist <= 0.6f * static_cast<core::f32>(R) && m_rng.NextFloat01() < 0.25f) {
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
```

---

### 2.3 Simulation Pipeline Integration & Step Order

#### Pipeline Order:
$$\text{Physics} \longrightarrow \text{Reactions} \longrightarrow \text{Thermodynamics} \longrightarrow \text{Explosions} \longrightarrow \text{AdvanceTick}$$

```
Step 1: physics.Step(world);
  - Updates falling powders, dispersing liquids, rising buoyant gases, and burning fire.
  - Resolves mutual density displacement swaps.
  - Updates contact pairs and stamps parity flag.

Step 2: reactions.Step(world);
  - Inspects contact pairs between adjacent cells.
  - Executes O(1) reaction rules from reactions.json.
  - Modifies materials, adds reaction heat (exothermic/endothermic).
  - If reaction result specifies explode=true, queues explosion into ExplosionSystem.

Step 3: thermodynamics.Step(world);
  - Performs 2D discrete cellular heat diffusion with harmonic contact conductivity:
      k_contact = (2 * k1 * k2) / (k1 + k2 + 1e-6f)
  - Enforces CFL stability clamping (transfer <= 0.25 * Delta T).
  - Radiates heat to ambient T_ambient = 20.0°C.
  - Executes phase transitions (melting, boiling, condensation, freezing) with latent heat offset (Delta T_latent = 3.0°C).

Step 4: explosions.Update(world);
  - Processes any queued explosions (from player brush, reaction results, or burning TNT).
  - Casts 8R perimeter Bresenham shockwaves.
  - Attenuates energy by material strength, applies mechanical destruction and flash heat.
  - Resolves secondary chain reaction cascades.

Step 5: world.AdvanceTick();
  - Toggles parity bit (m_currentParity ^= FLAG_PARITY).
```

#### Integration in `src/main.cpp`:
In `src/main.cpp`, the simulation tick block (lines 201-210) becomes:
```cpp
HighResClock::TimePoint simStart = HighResClock::Now();

physics.Step(world);
reactions.Step(world);
thermodynamics.Step(world);
explosions.Update(world);
world.AdvanceTick();
simTickCount++;

lastSimTimeMs = HighResClock::ElapsedMilliseconds(simStart, HighResClock::Now());
```

---

### 2.4 Detailed Architecture for Unit Test Suite (`tests/test_thermo_reactions.cpp`)

The test suite will contain 6 comprehensive tests:

#### Test 1: Harmonic Heat Diffusion & Numerical Stability
- **Objective**: Verify discrete 2D heat conduction, harmonic contact conductivity, CFL stability clamping ($\le 0.25 \Delta T$), and absence of overshoot.
- **Fixture**: $64 \times 64$ world with an Iron block ($k_1 = 0.95$) at $T_{\text{hot}} = 100.0^\circ\text{C}$ adjacent to an Iron block at $T_{\text{cold}} = 0.0^\circ\text{C}$.
- **Assertions**:
  1. Monotonic heat flow: $T_{\text{hot}}(t)$ strictly decreases, $T_{\text{cold}}(t)$ strictly increases.
  2. Boundedness: For all cells and all ticks, $0.0^\circ\text{C} \le T(x, y) \le 100.0^\circ\text{C}$.
  3. No overshoot: $T_{\text{cold}}$ NEVER exceeds $T_{\text{hot}}$ at any tick.
  4. Insulator isolation: If adjacent block is an insulator ($k = 0.0$), harmonic mean $k_{\text{contact}} = 0$, resulting in zero heat transfer into the insulator.

#### Test 2: Material Phase Transitions with Latent Heat Offset
- **Objective**: Verify melting, boiling, condensation, and freezing state changes, and verify that the latent heat offset ($\Delta T_{\text{latent}} = 3.0^\circ\text{C}$) eliminates single-tick oscillation flip-flops.
- **Sub-tests**:
  1. **Ice Melting**: Ice cell placed at $T = 5.0^\circ\text{C}$ ($> \text{meltPoint} = 0.0^\circ\text{C}$). After `thermo.Step(world)`, cell material is `waterId` and temperature is $5.0 - \Delta T_{\text{latent}} = 2.0^\circ\text{C}$.
  2. **Water Boiling**: Water cell at $T = 105.0^\circ\text{C}$ ($> \text{boilPoint} = 100.0^\circ\text{C}$). After step, cell material is `steamId` and temperature is $105.0 - \Delta T_{\text{latent}} = 102.0^\circ\text{C}$.
  3. **Steam Condensing**: Steam cell cooled to $T = 95.0^\circ\text{C}$ ($< \text{freezePoint} = 100.0^\circ\text{C}$). After step, cell material is `waterId` and temperature increases by latent heat.
  4. **Lava Freezing**: Lava cell cooled to $T = 650.0^\circ\text{C}$ ($< \text{freezePoint} = 700.0^\circ\text{C}$). After step, cell material is `stoneId` and temperature increases by latent heat.
  5. **Hysteresis Stability**: Water placed at $100.5^\circ\text{C}$ boils to Steam at $97.5^\circ\text{C}$. Verify that on the subsequent tick, Steam DOES NOT immediately condense back to Water, verifying that the hysteresis band prevents flickering.

#### Test 3: Fast $O(1)$ ReactionSystem Lookup & Execution
- **Objective**: Verify symmetric $O(1)$ reaction table loading and execution of chemical/physical reactions.
- **Sub-tests**:
  1. **Table Symmetry**: Check `reactions.GetReaction(waterId, lavaId) != nullptr` and `reactions.GetReaction(lavaId, waterId) != nullptr`.
  2. **Water + Lava**: Place Water at $(10, 10)$ and Lava at $(11, 10)$. Step `reactions.Step(world)`. Verify Water transforms to Steam and Lava transforms to Stone, with exothermic heat release.
  3. **Acid + Stone (Corrosion)**: Place Acid at $(20, 20)$ and Stone at $(20, 21)$. Step reactions. Verify Stone is corroded and dissolved into Smoke or Air.
  4. **Water + Fire**: Place Water at $(30, 30)$ and Fire at $(30, 31)$. Step reactions. Verify Fire is extinguished and Water converts to Steam.

#### Test 4: Gas Buoyancy, Brownian Drift, and Lifetime Decay
- **Objective**: Verify upward movement (negative gravity), lateral dispersion, and lifetime decrement to Air.
- **Sub-tests**:
  1. **Buoyancy**: Place Smoke particle at $(32, 50)$ with Air above. Step physics for 10 ticks. Verify $y_{\text{final}} < 50$ (moved upward).
  2. **Brownian Drift**: Place 40 Smoke particles in a column at $x = 32, y \in [40, 60]$. Run 25 ticks. Verify horizontal dispersion (particles exist at $x < 32$ and $x > 32$).
  3. **Lifetime Decay**: Place Smoke particle with lifetime = 5. Run 6 ticks. Verify cell decays into Air (`EMPTY_MATERIAL_ID`) and `activeCellCount == 0`.

#### Test 5: Fire Combustion, Ignition, Temperature, and Emissions
- **Objective**: Verify Fire high temperature ($400-800^\circ\text{C}$), neighbor ignition of flammable materials, non-flammable resistance, and emission of Smoke/Ash on decay.
- **Sub-tests**:
  1. **High Temperature**: Fire particle maintains $T \ge 400^\circ\text{C}$ while alive.
  2. **Flammable Ignition**: Place Wood (`ignition_temp = 250.0`, `flammability = 0.60`) adjacent to Fire. Step simulation. Verify Wood heats up above $250^\circ\text{C}$ and ignites into Fire (`FLAG_BURNING`).
  3. **Non-Flammable Resistance**: Place Stone adjacent to Fire. Verify Stone absorbs heat but NEVER ignites or burns away.
  4. **Emissions**: Fire particle with lifetime = 1 decays on the next tick, emitting Smoke (`id=11`) or Ash (`id=17`).

#### Test 6: ExplosionSystem Shockwave Raycasting, Obstacle Shielding, and Chain Reactions
- **Objective**: Verify Bresenham raycast blast propagation, obstacle shielding (stone wall absorbing shockwave and protecting cells behind it), and TNT secondary chain reaction.
- **Sub-tests**:
  1. **Open Crater**: Trigger explosion at $(32, 32)$ with power = 100, radius = 10. Verify circular crater excavated to Air with boundary cells flash-heated.
  2. **Obstacle Shielding**:
     - Epicenter at $(20, 32)$.
     - 3-cell thick Stone wall ($strength = 80 \times 3 = 240$) at $x \in [25, 27], y \in [20, 44]$.
     - Target cell (Sand) placed behind wall at $(30, 32)$.
     - Symmetric unshielded target cell placed at $(10, 32)$ in open air.
     - Trigger explosion at $(20, 32)$ with power = 90.
     - Verify: Unshielded target at $(10, 32)$ is destroyed.
     - Verify: Shielded target at $(30, 32)$ behind the Stone wall is COMPLETELY INTACT!
  3. **TNT Chain Reaction**:
     - Place TNT 1 at $(15, 32)$, TNT 2 at $(30, 32)$ (distance 15, within blast radius $R=20$), and TNT 3 at $(45, 32)$ (distance 30 from TNT 1, out of its direct radius, but within radius of TNT 2).
     - Detonate TNT 1.
     - Step / Update explosion system.
     - Verify: All 3 TNT blocks detonated via chain reaction, carving a continuous trench from $x = 10$ to $x = 55$.

---

### 2.5 CMake Build Configuration

#### 1. Root `CMakeLists.txt`
Update `SIM_SOURCES` to include the three new Milestone 2 source files:
```cmake
set(SIM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Chunk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/World.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/MaterialRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Physics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Thermodynamics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/ReactionSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/ExplosionSystem.cpp
)
```

#### 2. `tests/CMakeLists.txt`
Add the target `test_thermo_reactions` with asset deployment and CTest registration:
```cmake
# ------------------------------------------------------------------------------
# 5. Milestone 2 Unit Test Suite: test_thermo_reactions
# ------------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_thermo_reactions.cpp")
    add_executable(test_thermo_reactions
        test_thermo_reactions.cpp
        ${CORE_SOURCES}
        ${SIM_SOURCES}
        ${UI_SOURCES}
    )

    target_include_directories(test_thermo_reactions PRIVATE
        ${TEST_COMMON_INCLUDES}
    )

    target_link_libraries(test_thermo_reactions PRIVATE
        ${SDL2_TARGET}
        imgui
    )

    if(WIN32)
        target_link_libraries(test_thermo_reactions PRIVATE winmm imm32 version setupapi user32 gdi32 shell32)
    endif()

    set_target_properties(test_thermo_reactions PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    add_custom_command(TARGET test_thermo_reactions POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:test_thermo_reactions>/assets
    )

    add_test(NAME unit_test_thermo_reactions COMMAND test_thermo_reactions)
    set_tests_properties(unit_test_thermo_reactions PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
```

---

## 3. Caveats

1. **Active Cell Counts on Destruction**:
   When converting cells to Air (`EMPTY_MATERIAL_ID`), `World::SetCell` correctly decrements `activeCellCount`. Ensure implementations use `World::SetCell` rather than raw array mutations so that chunk active vertical bounds (`minActiveY`/`maxActiveY`) remain accurate for multithreaded simulation in Milestone 3.
2. **Chain Reaction Recursion Limit**:
   Explosives placed in large dense clusters could cause infinite recursion if explosions immediately trigger adjacent explosions. The iterative queue design with `MAX_CHAIN_DEPTH = 32` and immediate conversion of detonating cells to Air prevents this.
3. **Deterministic Randomness in Tests**:
   For reproducible test execution in `tests/test_thermo_reactions.cpp`, random generators (Brownian jitter, debris selection) must be seeded with fixed constants (e.g. `FastRNG(1337u)`).

---

## 4. Conclusion

The investigation provides an actionable design for Milestone 2:
- **`ExplosionSystem`**: 2-phase $8R$ perimeter Bresenham raycasting, material strength attenuation, obstacle shielding, thermal flash heating, and depth-limited iterative chain reactions.
- **Simulation Pipeline**: Strict ordering `Physics -> Reactions -> Thermodynamics -> Explosions -> AdvanceTick`.
- **Test Suite**: 6 comprehensive unit tests in `tests/test_thermo_reactions.cpp` covering diffusion stability, phase transitions with latent heat, $O(1)$ reactions, gas buoyancy/decay, fire ignition/decay, and explosion shielding/chaining.
- **CMake**: Build targets mapped cleanly into `CMakeLists.txt` and `tests/CMakeLists.txt`.

---

## 5. Verification Method

To verify the designs and implementations once written:

```powershell
# 1. Compile the complete project and tests in Release mode
cmake --build build --config Release

# 2. Run the newly created Milestone 2 unit test suite
.\build\Release\test_thermo_reactions.exe

# 3. Run the Milestone 1 physics unit test suite (regression verification)
.\build\Release\test_physics.exe

# 4. Run the complete CTest test runner
ctest --test-dir build -C Release --output-on-failure
```
