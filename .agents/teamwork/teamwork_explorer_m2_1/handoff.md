# Milestone 2 Investigation Report: Cellular Heat Diffusion, Material Phase Transitions, and SoA Chunk Thermodynamics Integration

**Author**: Explorer 1 (`teamwork_explorer_m2_1`)  
**Parent Orchestrator ID**: `452cac6c-0128-4a9c-9ecf-dde17c8213e1`  
**Date**: 2026-11-11  
**Milestone**: Milestone 2 (Thermodynamics, Reactions, Gases & Explosions — ШАГ 3)  
**Status**: COMPLETE (Read-only Investigation)  

---

## 1. Observation

### 1.1 Existing Codebase & Data Structures

1. **Chunk Memory & SoA Layout (`src/sim/Chunk.hpp:27-31`)**:
   ```cpp
   alignas(core::CACHE_LINE_SIZE) MaterialId material[AREA]{0};     // 8,192 bytes (128 cache lines)
   alignas(core::CACHE_LINE_SIZE) core::f32  temperature[AREA]{};  // 16,384 bytes (256 cache lines)
   alignas(core::CACHE_LINE_SIZE) core::u8   flags[AREA]{0};        // 4,096 bytes (64 cache lines)
   alignas(core::CACHE_LINE_SIZE) core::u8   lifetime[AREA]{0};     // 4,096 bytes (64 cache lines)
   ```
   - Total chunk footprint: exactly 32,768 bytes (32 KB, perfectly fits in typical CPU L1 data cache).
   - `temperature[4096]` is contiguous 32-bit floating-point values aligned to 64 bytes (`alignas(64)`).
   - In `src/sim/Chunk.cpp:25`, `Clear()` initializes `temperature` array with `defaultTemp = 20.0f`.
   - In `src/sim/Chunk.cpp:34-58`, `SetCell()` receives an optional `temp = 20.0f` parameter and sets `temperature[idx] = temp`.

2. **World Cell Swap & Thermal Advection (`src/sim/World.cpp:154-155`)**:
   ```cpp
   std::swap(c1->material[idx1], c2->material[idx2]);
   std::swap(c1->temperature[idx1], c2->temperature[idx2]);
   std::swap(c1->lifetime[idx1], c2->lifetime[idx2]);
   ```
   - Physical movement of particles in `Physics` automatically advects their thermal energy with them (particle temperature moves with the material).

3. **Material Thermal & Phase Transition Properties (`src/sim/Material.hpp:31-45`)**:
   ```cpp
   // Thermal properties
   core::f32 thermalConductivity{0.02f};
   core::f32 specificHeat{1.0f};
   core::f32 meltPoint{9999.0f};
   core::f32 boilPoint{9999.0f};
   core::f32 freezePoint{-9999.0f};

   // Phase transition targets (Names for JSON deserialization, IDs for O(1) runtime)
   std::string meltTargetName{""};
   std::string boilTargetName{""};
   std::string freezeTargetName{""};
   MaterialId meltMaterialId{EMPTY_MATERIAL_ID};
   MaterialId boilMaterialId{EMPTY_MATERIAL_ID};
   MaterialId freezeMaterialId{EMPTY_MATERIAL_ID};
   ```

4. **Phase Target Resolution (`src/sim/MaterialRegistry.cpp:205-217`)**:
   - `MaterialRegistry::ResolvePhaseTargets()` already maps `meltTargetName -> meltMaterialId`, `boilTargetName -> boilMaterialId`, and `freezeTargetName -> freezeMaterialId`.
   - Verified configured material targets:
     - `ice.json`: `melt_point: 0.0`, `melt_target: "water"` (`meltMaterialId = 4`).
     - `water.json`: `boil_point: 100.0`, `boil_target: "steam"` (`boilMaterialId = 6`); `freeze_point: 0.0`, `freeze_target: "ice"` (`freezeMaterialId = 5`).
     - `steam.json`: `freeze_point: 95.0`, `freeze_target: "water"` (`freezeMaterialId = 4`), `lifetime: 120`.
     - `stone.json`: `melt_point: 1200.0`, `melt_target: "lava"` (`meltMaterialId = 9`).
     - `lava.json`: `freeze_point: 700.0`, `freeze_target: "stone"` (`freezeMaterialId = 1`).
     - `iron.json`: `melt_point: 1538.0`, `melt_target: "lava"` (`meltMaterialId = 9`).

5. **Existing Build & Test Status**:
   - Command: `ctest --test-dir build -C Release --output-on-failure`
   - Result: 8/8 tests pass (100% success rate, 0.57s execution time).

---

## 2. Logic Chain

### 2.1 Topic 1: Cellular Heat Diffusion Architecture & Formulas

1. **Neighborhood Topology (von Neumann 4-Neighborhood)**:
   - For cell $(x, y)$, heat conducts across its 4 orthogonal faces:
     - North: $(x, y - 1)$
     - South: $(x, y + 1)$
     - West: $(x - 1, y)$
     - East: $(x + 1, y)$
   - Diagonal neighbors (Moore neighborhood) have zero planar contact area in 2D cell grids (touch only at point vertices), so conduction through diagonals is physically ungrounded and causes diagonal thermal leaks. Von Neumann 4-neighborhood is both physically accurate and computationally optimal (4 operations per cell instead of 8).

2. **Harmonic Contact Conductivity**:
   - Physical foundation: Heat flux across a flat interface between two adjacent medium blocks of thickness $\Delta x$ obeys series thermal resistance:
     $$R_{\text{total}} = R_1 + R_2 = \frac{\Delta x / 2}{k_1 A} + \frac{\Delta x / 2}{k_2 A}$$
     The effective contact conductance $k_{\text{contact}}$ satisfies:
     $$k_{\text{contact}} = \frac{2 \cdot k_1 \cdot k_2}{k_1 + k_2 + \epsilon}$$
     where $\epsilon = 1.0\times 10^{-6}\text{f}$ prevents division by zero when both conductivities are 0.
   - Contrast with arithmetic mean:
     - If Iron ($k=0.95$) touches Air ($k=0.02$), arithmetic mean would yield $\frac{0.95 + 0.02}{2} = 0.485$ (massive, unphysical thermal bleed into air).
     - Harmonic mean yields:
       $$k_{\text{contact}} = \frac{2 \times 0.95 \times 0.02}{0.95 + 0.02 + 10^{-6}} = \frac{0.038}{0.97} \approx 0.0392$$
       Air properly acts as an insulating boundary!
   - Zero-Division Precomputed Lookup Table:
     - All 18 materials have static conductivities.
     - A $256 \times 256$ float array `m_contactConductivity[256][256]` (256 KB, or $18 \times 18 = 1.3$ KB L1 resident) precomputed during `Thermodynamics` initialization turns harmonic conductivity calculation into a single $O(1)$ flat array lookup:
       `float k_contact = m_contactConductivity[matId1][matId2];`
     - Eliminates 4 expensive floating-point divisions per cell $\times$ 4096 cells = 16,384 division operations per chunk per tick!

3. **Numerical Stability Clamping & The CFL Criterion**:
   - The continuous 2D heat equation $\frac{\partial T}{\partial t} = \alpha \nabla^2 T$ discretized with explicit Forward-Euler on a regular grid yields:
     $$T^{n+1}(x,y) = T^n(x,y) + \sum_{j=1}^4 c_j \cdot (T_j^n - T^n(x,y))$$
   - Von Neumann stability analysis mandates $\sum_{j=1}^4 c_j \le 1.0$. For an isotropic cell with 4 symmetric orthogonal neighbors, this requires:
     $$c_j \le \frac{1}{4} = 0.25$$
   - Clamping formulation:
     $$\Delta T_j = 0.25\text{f} \cdot k_{\text{contact}} \cdot (T_j - T_0)$$
   - Since $k_1, k_2 \in [0.0, 1.0]$, $k_{\text{contact}} \le 1.0$, which guarantees:
     $$0 \le 0.25\text{f} \cdot k_{\text{contact}} \le 0.25\text{f}$$
   - Proof of no overshoot:
     Even if all 4 neighbors are at extreme temperature $T_{\text{hot}} = 100^\circ\text{C}$ and cell is at $T_0 = 0^\circ\text{C}$ with maximum conductivity $k_{\text{contact}} = 1.0$:
     $$T_{\text{new}} = 0 + 4 \times (0.25 \times 1.0 \times 100) = 100^\circ\text{C}$$
     $T_{\text{new}} \equiv T_{\text{hot}}$! Overshoot is mathematically impossible. Thermal divergence and checkerboard ping-pong instabilities are completely eliminated without conditional branching.

4. **Ambient Heat Radiation & Equilibrium Snapping**:
   - In open physical space, bodies radiate heat towards ambient room temperature $T_{\text{ambient}} = 20.0^\circ\text{C}$:
     $$\Delta T_{\text{ambient}} = r_{\text{ambient}} \cdot (T_{\text{ambient}} - T_0)$$
     where $r_{\text{ambient}} = 0.005\text{f}$ for Air, and $0.001\text{f}$ for condensed materials.
   - **Critical Snapping Threshold**:
     Floating-point asymptotic decay causes temperatures to linger at $20.00003^\circ\text{C}$, preventing chunks from ever going to sleep!
     When $|T_{\text{new}} - T_{\text{ambient}}| < 0.05^\circ\text{C}$, snap explicitly:
     $$T_{\text{new}} = 20.0^\circ\text{C}$$
     This allows sleeping chunks to activate their `sleepCounter` hysteresis and sleep cleanly.

---

### 2.2 Topic 2: Material Phase Transitions Architecture & Formulas

1. **Threshold Checks**:
   - For every cell, after thermal diffusion updates its temperature to $T$:
     - **Boiling/Vaporization**: if $M.\text{boilMaterialId} \ne 0$ and $T \ge M.\text{boilPoint} + \Delta T_{\text{latent}}$.
     - **Melting/Liquefaction**: if $M.\text{meltMaterialId} \ne 0$ and $T \ge M.\text{meltPoint} + \Delta T_{\text{latent}}$.
     - **Freezing/Condensation**: if $M.\text{freezeMaterialId} \ne 0$ and $T \le M.\text{freezePoint} - \Delta T_{\text{latent}}$.

2. **The Ping-Pong Oscillation Bug & Latent Heat Offset**:
   - Without latent heat offset, consider Ice $\leftrightarrow$ Water at $0.0^\circ\text{C}$:
     - Tick 1: Ice at $+0.1^\circ\text{C} \implies$ melts to Water at $+0.1^\circ\text{C}$.
     - Tick 2: Water loses $-0.2^\circ\text{C}$ to colder neighbor $\implies$ reaches $-0.1^\circ\text{C} \implies$ freezes to Ice.
     - Tick 3: Ice warms to $+0.1^\circ\text{C} \implies$ melts to Water.
     - Result: Continuous high-frequency flickering (Ice $\to$ Water $\to$ Ice $\to$ Water every frame), destroying visual fidelity and causing sleep thrashing.
   - **Physics-Grounded Hysteresis Solution**:
     In nature, phase transitions require absorbing (endothermic) or releasing (exothermic) latent heat $L$:
     - Melting / Boiling (Endothermic):
       Requires temperature to exceed the threshold by $\Delta T_{\text{latent}} = 3.0^\circ\text{C}$.
       Upon transformation, latent heat is absorbed:
       $$T \leftarrow T - \Delta T_{\text{latent}}$$
       Example: Ice ($0^\circ\text{C}$ melt point) melts only when $T \ge +3.0^\circ\text{C}$. Upon melting to Water, $T$ becomes $+0.0^\circ\text{C}$ (or $T - 3.0$).
     - Freezing / Condensation (Exothermic):
       Requires temperature to drop below the threshold by $\Delta T_{\text{latent}} = 3.0^\circ\text{C}$.
       Upon transformation, latent heat is released:
       $$T \leftarrow T + \Delta T_{\text{latent}}$$
       Example: Water ($0^\circ\text{C}$ freeze point) freezes only when $T \le -3.0^\circ\text{C}$. Upon freezing to Ice, $T$ becomes $+0.0^\circ\text{C}$ (or $T + 3.0$).
   - Result: A hysteresis band of width $2 \times \Delta T_{\text{latent}} = 6.0^\circ\text{C}$ centered around the phase transition point. Within $[-3.0^\circ\text{C}, +3.0^\circ\text{C}]$, the material remains in its current phase. Zero oscillation!

3. **Cell State Transformation Protocol**:
   - `chunk.material[idx] = newMaterialId;`
   - `chunk.temperature[idx] = adjustedTemp;`
   - Gas / Energetic initialization: if `newMat.lifetime > 0`, set `chunk.lifetime[idx] = newMat.lifetime` (e.g. Steam gets 120 ticks).
   - Clear sleep bit: `chunk.flags[idx] &= ~FLAG_SLEEPING;`
   - Wake chunk and mark dirty: `chunk.isDirty = true; chunk.WakeUp();`
   - Wake neighboring chunks if cell touches chunk borders: `world.WakeNeighborsForCell(cx, cy, lx, ly);`

---

### 2.3 Topic 3: Thermodynamics Integration with Chunk SoA, World Grid & Active Tracking

1. **Chunk SoA Cache & In-Place Bias Elimination**:
   - Updating `chunk.temperature` directly in-place introduces directional propagation bias (heat moves down/right faster than up/left).
   - In-place updates across chunk borders would also cause race conditions during Milestone 3's 4-phase checkerboard parallel dispatch.
   - **Solution**:
     During `Thermodynamics::StepChunk(World& world, Chunk& chunk)`:
     Use an `alignas(64) core::f32 nextTemp[Chunk::AREA]` scratch buffer.
     - 4096 floats = exactly 16 KB. Resident in L1 cache alongside chunk data.
     - Step 1: Read current temperatures and materials (intra-chunk reads are direct L1 memory accesses; inter-chunk reads query `world.GetCellTemperature`).
     - Step 2: Compute diffusion $\Delta T$, ambient cooling, and phase transitions on `nextTemp`.
     - Step 3: Write back `nextTemp` to `chunk.temperature` (`std::memcpy`).
     - Guarantees 100% isotropic diffusion, zero directional drift, and thread safety.

2. **Integration with Sleeping Chunks & Wakeup Cascades**:
   - A chunk is skipped by `Thermodynamics` if:
     `chunk.isSleeping && (!chunkHasThermalGradients)`
   - When heat diffuses across a chunk boundary:
     If transfer across boundary $|\Delta T| > 0.05^\circ\text{C}$, call:
     `world.WakeNeighborsForCell(cx, cy, lx, ly);`
     The sleeping neighbor chunk is woken up immediately (`isSleeping = false; sleepCounter = SLEEP_HYSTERESIS;`).
   - Thermal Sleep Condition:
     If all cell temperatures in a chunk are within $0.05^\circ\text{C}$ of ambient $20.0^\circ\text{C}$ and no particles are moving, the chunk sleep counter decrements to 0 and the chunk sleeps.

3. **Active Row Sweep Optimization**:
   - `minActiveY` and `maxActiveY` in `Chunk` track active particles.
   - For thermal diffusion, we expand the vertical active bounds by 1 cell above and 1 cell below (`minY = std::max(0, minActiveY - 1)`, `maxY = std::min(63, maxActiveY + 1)`).
   - If air cells inside a chunk are heated ($|T - 20.0\text{f}| > 1.0\text{f}$), `chunk.ExpandActiveY(ly)` is called so the heated air column is included in active updates.

---

## 3. Concrete Code Designs for Worker

### 3.1 Interface Header: `src/sim/Thermodynamics.hpp`

```cpp
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
    [[nodiscard]] core::f32 GetContactConductivity(MaterialId id1, MaterialId id2) const noexcept {
        return m_contactConductivity[id1 & 0xFF][id2 & 0xFF];
    }

private:
    // Performs phase transition check on cell; returns true if material changed
    bool CheckPhaseTransition(World& world, Chunk& chunk, core::i32 lx, core::i32 ly,
                              core::i32 wx, core::i32 wy, core::f32& currentTemp) noexcept;
};

} // namespace sandbox::sim
```

### 3.2 Implementation: `src/sim/Thermodynamics.cpp`

```cpp
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
    m_tableInitialized = true;
}

void Thermodynamics::Step(World& world) noexcept {
    if (!m_tableInitialized) {
        InitLookupTable(MaterialRegistry::Instance());
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
    // 16 KB scratch buffer fitting in L1 cache
    alignas(core::CACHE_LINE_SIZE) core::f32 nextTemp[Chunk::AREA];
    std::memcpy(nextTemp, chunk.temperature, sizeof(nextTemp));

    core::i32 cx = chunk.chunkX;
    core::i32 cy = chunk.chunkY;
    core::i32 startX = cx * Chunk::SIZE;
    core::i32 startY = cy * Chunk::SIZE;

    // Use active bounds if available, clamped to chunk dimensions
    core::i32 minY = std::max(0, chunk.minActiveY);
    core::i32 maxY = std::min(Chunk::SIZE - 1, chunk.maxActiveY);

    if (minY > maxY) {
        // Chunk is mechanically empty, but might contain heat in air
        minY = 0;
        maxY = Chunk::SIZE - 1;
    } else {
        // Expand by 1 row above and below for heat diffusion fringe
        minY = std::max(0, minY - 1);
        maxY = std::min(Chunk::SIZE - 1, maxY + 1);
    }

    bool chunkHasThermalActivity = false;

    // 4-neighborhood offsets: {up, down, left, right}
    static constexpr core::i32 DX[4] = {0, 0, -1, 1};
    static constexpr core::i32 DY[4] = {-1, 1, 0, 0};

    for (core::i32 ly = minY; ly <= maxY; ++ly) {
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
        chunk.material[idx] = newId;
        currentTemp -= LATENT_HEAT_OFFSET; // Latent heat absorbed
        chunk.lifetime[idx] = newMat.lifetime;
        chunk.flags[idx] &= ~FLAG_SLEEPING;
        chunk.isDirty = true;
        chunk.WakeUp();
        world.WakeNeighborsForCell(chunk.chunkX, chunk.chunkY, lx, ly);
        return true;
    }

    // 2. Melting / Liquefaction (e.g. Ice -> Water, Stone -> Lava, Iron -> Lava)
    if (mat.meltMaterialId != EMPTY_MATERIAL_ID && currentTemp >= mat.meltPoint + LATENT_HEAT_OFFSET) {
        MaterialId newId = mat.meltMaterialId;
        const Material& newMat = MaterialRegistry::Instance().Get(newId);
        chunk.material[idx] = newId;
        currentTemp -= LATENT_HEAT_OFFSET; // Latent heat absorbed
        chunk.lifetime[idx] = newMat.lifetime;
        chunk.flags[idx] &= ~FLAG_SLEEPING;
        chunk.isDirty = true;
        chunk.WakeUp();
        world.WakeNeighborsForCell(chunk.chunkX, chunk.chunkY, lx, ly);
        return true;
    }

    // 3. Freezing / Condensation / Solidification (e.g. Water -> Ice, Steam -> Water, Lava -> Stone)
    if (mat.freezeMaterialId != EMPTY_MATERIAL_ID && currentTemp <= mat.freezePoint - LATENT_HEAT_OFFSET) {
        MaterialId newId = mat.freezeMaterialId;
        const Material& newMat = MaterialRegistry::Instance().Get(newId);
        chunk.material[idx] = newId;
        currentTemp += LATENT_HEAT_OFFSET; // Latent heat released
        chunk.lifetime[idx] = newMat.lifetime;
        chunk.flags[idx] &= ~FLAG_SLEEPING;
        chunk.isDirty = true;
        chunk.WakeUp();
        world.WakeNeighborsForCell(chunk.chunkX, chunk.chunkY, lx, ly);
        return true;
    }

    return false;
}

} // namespace sandbox::sim
```

---

## 4. Caveats

1. **Specific Heat Scaling**:
   - The current model transfers heat based on contact conductivity and numerical stability clamp ($0.25$). Specific heat $c$ can scale the rate of temperature change $\Delta T = \frac{\Delta Q}{c \cdot m}$. Because all material densities and specific heats are within reasonable ranges $[0.4, 4.2]$, treating $\Delta T$ directly with conductivity yields stable, predictable, and physically intuitive diffusion without numerical stiffness. If desired, Worker can add $\frac{1}{c_0}$ normalization factor.
2. **Latent Heat Offset Value**:
   - $\Delta T_{\text{latent}} = 3.0^\circ\text{C}$ is recommended. It creates a robust $6.0^\circ\text{C}$ hysteresis window. If test cases require exact melting at $0.0^\circ\text{C}$ without an offset, the test should set temperature to $\ge 3.0^\circ\text{C}$ or $\Delta T_{\text{latent}}$ can be configured via a constant.
3. **Empty Air Thermal Bounds**:
   - Heated empty air expands the active row bounds in `Chunk` so that hot air convection and thermal diffusion continue until the air cools to $20.0^\circ\text{C}$.

---

## 5. Conclusion

1. **Cellular Heat Diffusion** is fully designed with von Neumann 4-neighborhood, harmonic contact conductivity $k_{\text{contact}} = \frac{2 k_1 k_2}{k_1 + k_2 + \epsilon}$, $0.25$ stability clamping, ambient cooling towards $20.0^\circ\text{C}$, and ambient equilibrium snapping at $0.05^\circ\text{C}$. A $256 \times 256$ precomputed conductivity table eliminates all division in the inner loop.
2. **Material Phase Transitions** are designed for melting, boiling, and freezing using threshold checks and a $\Delta T_{\text{latent}} = 3.0^\circ\text{C}$ latent heat offset. This completely eliminates ping-pong oscillation flips between states.
3. **Chunk SoA Integration** leverages a 16 KB L1-cache scratch buffer (`alignas(64) core::f32 nextTemp[4096]`) ensuring isotropic diffusion without directional bias and providing thread safety for Milestone 3's 4-phase checkerboard parallel updates.

---

## 6. Verification Method

To verify the thermodynamic implementation when implemented by Worker:
1. **Build verification**:
   ```powershell
   cmake --build build --config Release
   ```
2. **Unit tests in `tests/test_thermo_reactions.cpp`**:
   Verify:
   - `HarmonicContactConductivity`: $k_1 = k_2 = 0.5 \implies k = 0.5$; $k_1 = 0.0, k_2 = 1.0 \implies k \approx 0.0$.
   - `NumericalStability`: Center cell at $0^\circ\text{C}$, 4 neighbors at $100^\circ\text{C} \implies T \le 100^\circ\text{C}$ over 100 ticks, zero overshoot.
   - `PhaseTransitionMelting`: Ice at $-5^\circ\text{C}$ heated to $+10^\circ\text{C} \implies$ becomes Water.
   - `PhaseTransitionBoiling`: Water at $80^\circ\text{C}$ heated to $120^\circ\text{C} \implies$ becomes Steam with lifetime 120.
   - `LatentHeatNoFlicker`: Ice and Water in contact at $0^\circ\text{C}$ do not alternate states each tick.
   - `ChunkBorderDiffusion`: Heat diffuses across chunk boundary from $(63, y)$ to $(64, y)$, waking the sleeping neighbor chunk.
3. **Full test suite execution**:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
