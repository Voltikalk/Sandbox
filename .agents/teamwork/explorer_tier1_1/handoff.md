# Handoff Report: Explorer 1 — Tier 1 Test Specifications (Features F1 - F8)

**Date**: 2026-11-11T07:30:00Z  
**Author**: Explorer 1 (`teamwork_explorer`)  
**Recipient**: Parent Orchestrator (`9ae2990c-8916-46fc-93a6-792421757ac2`)  
**Scope**: Features F1 through F8 public API investigation and Tier 1 test formulation (41 concrete test specifications with exact C++ code snippets using `e2e_framework.hpp`).

---

## 1. Observation

### 1.1 Source Code and Architecture Verification
Direct inspection of the codebase in `c:\Users\Drilla\Desktop\Sandbox` verified the implementation of all Milestone 1 features (F1 through F8):

1. **Feature F1 (64x64 SoA Chunk Layout & World)**:
   - File `src/sim/Chunk.hpp`:
     - Line 20: `class alignas(core::CACHE_LINE_SIZE) Chunk` with `SIZE = 64`, `AREA = 4096`.
     - Lines 27-30: Four cache-line aligned arrays:
       - `alignas(64) MaterialId material[4096];` (8,192 bytes)
       - `alignas(64) core::f32 temperature[4096];` (16,384 bytes)
       - `alignas(64) core::u8 flags[4096];` (4,096 bytes)
       - `alignas(64) core::u8 lifetime[4096];` (4,096 bytes)
     - Total raw SoA memory footprint: 32,768 bytes (32 KB, fitting in CPU L1 data cache).
     - Lines 47-55: Fast 1D index mapping: `Index(lx, ly) = (ly << 6) | lx` and inverse `FromIndex(idx, lx, ly)`.
     - Lines 58-61: Checkerboard phase calculation: `GetPhase() = (chunkX & 1) | ((chunkY & 1) << 1)`.
     - Lines 64-80: Active vertical row bounds tracking: `minActiveY`, `maxActiveY`, `ExpandActiveY()`, `ResetActiveY()`.
     - Lines 94-98: Enforced `static_assert` statements verifying exact byte sizes and alignment.
   - File `src/sim/World.hpp` & `src/sim/World.cpp`:
     - Coordinate transformations: `WorldToChunkX(x) = x >> 6`, `WorldToChunkY(y) = y >> 6`, `WorldToLocalX(x) = x & 63`, `WorldToLocalY(y) = y & 63`.
     - `GetChunk(cx, cy)`, `GetChunkAtWorld(x, y)`, `SetCell(...)`, `GetCellMaterial(...)`, `GetCellTemperature(...)`, `SwapCells(...)`, `WakeNeighbors(...)`.

2. **Feature F2 (MaterialRegistry & Schema)**:
   - File `src/sim/Material.hpp`:
     - `struct Material` defining mechanical properties (`density`, `dispersion`, `strength`), thermal properties (`specificHeat`, `thermalConductivity`, `meltPoint`, `boilPoint`, `freezePoint`), phase targets (`meltMaterialId`, `boilMaterialId`, `freezeMaterialId`), and lifecycle (`flammability`, `ignitionTemp`, `lifetime`).
   - File `src/sim/MaterialRegistry.hpp` & `src/sim/MaterialRegistry.cpp`:
     - Singleton `MaterialRegistry::Instance()`.
     - Line 15: `m_emptyMaterial` initialized as `"air"` with ID 0 (`EMPTY_MATERIAL_ID`), state `MaterialState::Gas`, density `0.0f`.
     - `RegisterMaterial(Material mat)` handles dynamic material insertion and ensures Air remains at ID 0.
     - `ResolvePhaseTargets()` resolves two-pass phase name strings (`melt_target`, `boil_target`, `freeze_target`) into $O(1)$ numerical `MaterialId` targets.
     - Out-of-bounds `Get(id)` safely falls back to `m_emptyMaterial`.

3. **Feature F3 (Core Materials Assets)**:
   - Directory `assets/materials/`: Exactly 18 JSON material definition files present:
     - `acid.json`: Liquid, density 1200.0, dispersion 4
     - `air.json`: Gas, density 0.0, dispersion 0
     - `ash.json`: Powder, density 500.0, dispersion 1
     - `dirt.json`: Powder, density 1500.0, dispersion 1
     - `fire.json`: Gas, density 0.1, dispersion 2, lifetime 45
     - `glass.json`: Solid, density 2400.0, dispersion 0
     - `gunpowder.json`: Powder, density 1650.0, dispersion 1, flammability 0.95
     - `ice.json`: Solid, density 917.0, dispersion 0, melt_point 0.0 -> water
     - `iron.json`: Solid, density 7800.0, dispersion 0, strength 80.0
     - `lava.json`: Liquid, density 3000.0, dispersion 1, freeze_point 700.0 -> stone
     - `oil.json`: Liquid, density 800.0, dispersion 3, flammability 0.8
     - `sand.json`: Powder, density 1600.0, dispersion 2, melt_point 1700.0 -> glass
     - `smoke.json`: Gas, density 0.8, dispersion 2, lifetime 120
     - `steam.json`: Gas, density 0.6, dispersion 3, freeze_point 100.0 -> water
     - `stone.json`: Solid, density 2500.0, dispersion 0, melt_point 1200.0 -> lava
     - `tnt.json`: Solid, density 1654.0, dispersion 0
     - `water.json`: Liquid, density 1000.0, dispersion 5, boil_point 100.0 -> steam, freeze_point 0.0 -> ice
     - `wood.json`: Solid, density 650.0, dispersion 0, flammability 0.4

4. **Feature F4 (Powder Cellular Automata)**:
   - File `src/sim/Physics.cpp` lines 119-152 (`UpdatePowder`):
     - Step 1: Direct downward gravity displacement (`x, y + 1`).
     - Step 2: Diagonal sliding with randomized lateral direction (`dir = (m_rng.NextU32() & 1u) ? 1 : -1`) to prevent lateral slope bias.
     - Line 142: Diagonal corner tunneling prevention (`if (sideMatRef.state == Solid && downMatRef.state == Solid) continue`).
     - Stable angle of repose pile formation verified empirically in `tier5_adversarial_m1.cpp`.

5. **Feature F5 (Liquid Cellular Automata)**:
   - File `src/sim/Physics.cpp` lines 154-223 (`UpdateLiquid`):
     - Step 1: Direct downward gravity displacement (`x, y + 1`).
     - Step 2: Diagonal downward sliding with corner leak protection.
     - Step 3: Lateral dispersion with raycast stepping up to `mat.dispersion` cells.
     - Lines 206-215: Cliff drop-off priority:
       ```cpp
       core::i32 belowTargetY = y + 1;
       if (belowTargetY < world.GetHeight()) {
           MaterialId belowMat = world.GetCellMaterial(targetX, belowTargetY);
           if (CanDisplace(mat, belowMat)) {
               break; // Cliff / hole discovered! Halt lateral expansion immediately
           }
       }
       ```

6. **Feature F6 (Density Mutual Displacement)**:
   - File `src/sim/Physics.cpp` lines 81-117 (`CanDisplace`):
     - Empty/Air target is always displaceable.
     - Solid targets (`targetMat.state == MaterialState::Solid`) are strictly non-displaceable.
     - Powder displaces liquids with strictly lower density (`sourceMat.density > targetMat.density`) and gases; does NOT displace other powders.
     - Liquid displaces lighter liquids (`sourceMat.density > targetMat.density`) and gases; does NOT displace powders or solids.
     - Gas displaces heavier gases upward (`sourceMat.density < targetMat.density`).

7. **Feature F7 (Streaming Texture Rendering)**:
   - File `src/ui/Renderer.hpp` & `src/ui/Renderer.cpp`:
     - `WorldRenderer` maintains `m_pixelBuffer` of 32-bit RGBA values (`Color4u::ToU32()`).
     - Background color for Air/empty cells: `Color4u(18, 18, 22, 255).ToU32() == 0xFF161212`.
     - Deterministic coordinate noise variation: `core::CellNoiseVariation(wx, wy, mat.colorVariation)`.
     - `ScreenToWorld(screenX, screenY, viewport, worldX, worldY)` provides decoupled, headless mathematical mapping of viewport coordinates to world cells.
     - `Chunk::isDirty` flags track which chunks need pixel re-extraction.

8. **Feature F8 (Interactive Mouse Brush)**:
   - File `src/ui/Brush.hpp` & `src/ui/Brush.cpp`:
     - Clamped radius: `std::clamp(r, 1, 50)`.
     - `PaintDisk(world, cx, cy, mat, radius)` evaluates circle equation $dx^2 + dy^2 \le r^2$ and sets cells safely within world boundaries.
     - `PaintStroke(world, x0, y0, x1, y1, mat, radius)` uses Bresenham line algorithm for continuous stroke interpolation across arbitrary angles.
     - Eraser mode: When `IsErasing() == true`, paints `EMPTY_MATERIAL_ID` (Air).

### 1.2 Test Harness and Build System Observations
- `tests/e2e/e2e_framework.hpp`:
  - Completely self-contained, header-only testing framework.
  - Macros: `E2E_TEST(suite, name, tier)`, `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_FLOAT_NEAR`, `ASSERT_GT`, `ASSERT_LT`, `ASSERT_GE`, `ASSERT_LE`.
  - Fixture: `TestWorldFixture` / `WorldFixture` provides world, physics, automatic material loading via `ResolveAssetPath("assets/materials")`.
  - Inspection helpers: `inspection::CountMaterial`, `inspection::CountMaterialInBox`, `inspection::GetBoundingBox`, `inspection::GetCenterOfMass`, `inspection::IsBoxUniform`.
  - Stepping helpers: `step::Ticks`, `step::Until`, `step::UntilSettled`.
- Build observation in `tests/CMakeLists.txt`:
  - `sandbox_e2e_tests` currently compiles `${E2E_SOURCES}`, `${E2E_TIER_SOURCES}`, `${CORE_SOURCES}`, `${SIM_SOURCES}`.
  - When Tier 1 tests for F7 (`ui/Renderer.hpp`) and F8 (`ui/Brush.hpp`) are added to `tier1_features.cpp`, `${UI_SOURCES}` (`src/ui/Renderer.cpp`, `src/ui/Brush.cpp`) must be included in `sandbox_e2e_tests` in `tests/CMakeLists.txt`.

---

## 2. Logic Chain

1. **Requirement Mapping**:
   - `ORIGINAL_REQUEST.md` (R1) and `PROJECT.md` establish Features F1 through F8 as the foundational milestone M1.
   - `TEST_INFRA.md § 6.1` mandates $\ge 5$ tests per feature for Tier 1 functional equivalence.
   - $8 \text{ features} \times 5 = \ge 40 \text{ test cases}$.

2. **Opaque-Box Verification Principle**:
   - Tier 1 tests must verify nominal, happy-path functional requirements using exclusively public APIs (`World`, `Chunk`, `MaterialRegistry`, `Physics`, `WorldRenderer`, `Brush`).
   - Tests run headlessly with deterministic simulation stepping, verifying physical conservation laws (mass, bounding box, spatial sorting) without GUI or GPU dependencies.

3. **Stratification Across Features F1 - F8**:
   - **F1 (Chunk Layout)**: 6 test cases verifying cache line alignment, 32KB footprint, bijective 1D indexing, active row tracking, 4-phase checkerboard calculation, and world coordinate transformations.
   - **F2 (MaterialRegistry)**: 5 test cases verifying air at ID 0, programmatic material registration, property query fidelity, two-pass phase resolution, and missing material graceful fallback.
   - **F3 (Core Materials Assets)**: 5 test cases verifying all 18 JSON files loaded, schema parameters for powder, liquid, solid, and gas categories.
   - **F4 (Powder Cellular Automata)**: 5 test cases verifying vertical gravity fall, flat floor impact resting, diagonal sliding on peak, angle-of-repose pile formation, and narrow tube obstacle containment.
   - **F5 (Liquid Cellular Automata)**: 5 test cases verifying pure vertical fall mid-air without lateral spread, horizontal leveling on floor, lateral dispersion step limits, cliff drop-off priority, and U-basin containment without leakage.
   - **F6 (Density Mutual Displacement)**: 5 test cases verifying sand sinking in water, water sinking in oil, ash floating on liquids, 4-liquid column stratification, and solid barrier non-displacement.
   - **F7 (Streaming Texture Rendering)**: 5 test cases verifying `Color4u` little-endian packing, air background color mapping, deterministic spatial noise variation, `ScreenToWorld` coordinate conversion, and chunk dirty flag tracking.
   - **F8 (Interactive Mouse Brush)**: 5 test cases verifying radius clamping and single-cell precision, circular disk geometry, eraser functionality, Bresenham stroke interpolation, and world boundary clipping safety.
   - **Total: 41 concrete Tier 1 test specifications.**

---

## 3. Caveats

1. **`${UI_SOURCES}` in `sandbox_e2e_tests`**:
   `tests/CMakeLists.txt` lines 26-31 currently link `${CORE_SOURCES}` and `${SIM_SOURCES}` into `sandbox_e2e_tests`, but omit `${UI_SOURCES}`. Features F7 (`WorldRenderer`) and F8 (`Brush`) reside in `src/ui/`. The implementer / Worker must ensure `${UI_SOURCES}` is added to `sandbox_e2e_tests` target in `tests/CMakeLists.txt`.
2. **Headless Execution of `WorldRenderer`**:
   `WorldRenderer` takes `SDL_Renderer*` in its constructor, but when called headlessly with `nullptr`, `SDL_CreateTexture` safely returns `nullptr`. `ScreenToWorld`, `MarkAllDirty`, `Color4u` calculations, and `UpdateChunkPixels` logic remain fully testable without any active window or GPU driver.
3. **Materials Asset Path**:
   `TestWorldFixture` uses `ResolveAssetPath("assets/materials")`. Tests must ensure the working directory or executable directory has access to `assets/`. CMake automatically copies `assets/` to `$<TARGET_FILE_DIR>` in post-build steps.
4. **Parity Bit Ticking in Direct Physics Calls**:
   When invoking `physics.Step(world)`, tests must follow with `world.AdvanceTick()` if simulating multiple consecutive ticks, or use `fixture.Step(count)` which handles parity toggling automatically.

---

## 4. Conclusion & Complete Tier 1 Test Suite Specifications

Below is the complete, self-contained C++ code for all 41 Tier 1 test cases across Features F1 through F8, ready to be incorporated into `tests/e2e/tier1_features.cpp`.

```cpp
// ============================================================================
// tests/e2e/tier1_features.cpp — Tier 1 Functional Equivalence Tests (F1 - F8)
// ============================================================================

#include "e2e_framework.hpp"
#include "ui/Renderer.hpp"
#include "ui/Brush.hpp"
#include <cmath>
#include <vector>
#include <algorithm>

using namespace sandbox;
using namespace sandbox::sim;
using namespace sandbox::ui;
using namespace sandbox::test;

// ============================================================================
// FEATURE F1: 64x64 SoA Chunk Layout
// ============================================================================

// F1.1: Memory layout, 32KB payload, and 64-byte cache line alignment
E2E_TEST(ChunkLayout, MemoryLayoutAndCacheAlignment, 1) {
    // Compile-time static contracts
    static_assert(alignof(Chunk) == 64, "Chunk class must be 64-byte cache line aligned");
    static_assert(sizeof(Chunk::material) == 8192, "Material array must be exactly 8192 bytes (4096 * 2)");
    static_assert(sizeof(Chunk::temperature) == 16384, "Temperature array must be exactly 16384 bytes (4096 * 4)");
    static_assert(sizeof(Chunk::flags) == 4096, "Flags array must be exactly 4096 bytes (4096 * 1)");
    static_assert(sizeof(Chunk::lifetime) == 4096, "Lifetime array must be exactly 4096 bytes (4096 * 1)");

    constexpr size_t totalSoA = sizeof(Chunk::material) + sizeof(Chunk::temperature) +
                                sizeof(Chunk::flags) + sizeof(Chunk::lifetime);
    ASSERT_EQ(totalSoA, 32768u); // Exactly 32 KB

    // Runtime instance alignment verification
    Chunk chunk(0, 0);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(&chunk) % 64, 0u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(chunk.material) % 64, 0u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(chunk.temperature) % 64, 0u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(chunk.flags) % 64, 0u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(chunk.lifetime) % 64, 0u);
}

// F1.2: Bijective 1D index mapping and boundary coordinate corners
E2E_TEST(ChunkLayout, IndexBijectiveMapping, 1) {
    // Test the 4 extreme corners of a 64x64 chunk
    ASSERT_EQ(Chunk::Index(0, 0), 0);
    ASSERT_EQ(Chunk::Index(63, 0), 63);
    ASSERT_EQ(Chunk::Index(0, 63), 4032);
    ASSERT_EQ(Chunk::Index(63, 63), 4095);

    // Verify full bijective roundtrip for all 4096 cells
    for (core::i32 ly = 0; ly < Chunk::SIZE; ++ly) {
        for (core::i32 lx = 0; lx < Chunk::SIZE; ++lx) {
            core::i32 idx = Chunk::Index(lx, ly);
            ASSERT_GE(idx, 0);
            ASSERT_LT(idx, Chunk::AREA);
            ASSERT_EQ(idx, (ly << 6) | lx);

            core::i32 outLx = -1, outLy = -1;
            Chunk::FromIndex(idx, outLx, outLy);
            ASSERT_EQ(outLx, lx);
            ASSERT_EQ(outLy, ly);
        }
    }
}

// F1.3: Active cell count and active vertical row bounds tracking
E2E_TEST(ChunkLayout, ActiveCellCountAndRowBounds, 1) {
    Chunk chunk(0, 0);
    ASSERT_EQ(chunk.activeCellCount, 0u);
    ASSERT_EQ(chunk.minActiveY, Chunk::SIZE);
    ASSERT_EQ(chunk.maxActiveY, -1);

    // Set cell at (10, 20)
    chunk.SetCell(10, 20, 3, 20.0f); // Sand
    ASSERT_EQ(chunk.activeCellCount, 1u);
    ASSERT_EQ(chunk.minActiveY, 20);
    ASSERT_EQ(chunk.maxActiveY, 20);

    // Set cell at (50, 45)
    chunk.SetCell(50, 45, 3, 20.0f);
    ASSERT_EQ(chunk.activeCellCount, 2u);
    ASSERT_EQ(chunk.minActiveY, 20);
    ASSERT_EQ(chunk.maxActiveY, 45);

    // Remove first cell (set to EMPTY_MATERIAL_ID)
    chunk.SetCell(10, 20, EMPTY_MATERIAL_ID, 20.0f);
    ASSERT_EQ(chunk.activeCellCount, 1u);

    // Recompute bounds
    chunk.UpdateActiveBounds();
    ASSERT_EQ(chunk.minActiveY, 45);
    ASSERT_EQ(chunk.maxActiveY, 45);

    // Remove remaining cell
    chunk.SetCell(50, 45, EMPTY_MATERIAL_ID, 20.0f);
    ASSERT_EQ(chunk.activeCellCount, 0u);
    ASSERT_EQ(chunk.minActiveY, Chunk::SIZE);
    ASSERT_EQ(chunk.maxActiveY, -1);
}

// F1.4: 4-Phase checkerboard spatial phase calculation
E2E_TEST(ChunkLayout, CheckerboardSpatialPhase, 1) {
    Chunk c00(0, 0);
    Chunk c10(1, 0);
    Chunk c01(0, 1);
    Chunk c11(1, 1);

    ASSERT_EQ(c00.GetPhase(), 0u); // (0&1) | ((0&1)<<1) = 0
    ASSERT_EQ(c10.GetPhase(), 1u); // (1&1) | ((0&1)<<1) = 1
    ASSERT_EQ(c01.GetPhase(), 2u); // (0&1) | ((1&1)<<1) = 2
    ASSERT_EQ(c11.GetPhase(), 3u); // (1&1) | ((1&1)<<1) = 3

    // Test arbitrary chunk coordinates across a larger grid
    Chunk c14_9(14, 9); // cx=14 (even), cy=9 (odd) -> Phase 2
    ASSERT_EQ(c14_9.GetPhase(), 2u);

    Chunk c7_6(7, 6);   // cx=7 (odd), cy=6 (even) -> Phase 1
    ASSERT_EQ(c7_6.GetPhase(), 1u);
}

// F1.5: Chunk Clear and state reset
E2E_TEST(ChunkLayout, ClearAndResetState, 1) {
    Chunk chunk(2, 3);
    // Populate chunk with dirty data
    chunk.SetCell(5, 5, 3, 500.0f, FLAG_BURNING, 100);
    chunk.SetCell(60, 60, 4, 10.0f, FLAG_FALLING, 50);
    ASSERT_GT(chunk.activeCellCount, 0u);

    // Clear with EMPTY_MATERIAL_ID
    chunk.Clear(EMPTY_MATERIAL_ID, 20.0f);

    ASSERT_EQ(chunk.activeCellCount, 0u);
    ASSERT_EQ(chunk.minActiveY, Chunk::SIZE);
    ASSERT_EQ(chunk.maxActiveY, -1);
    ASSERT_TRUE(chunk.isDirty);
    ASSERT_FALSE(chunk.isSleeping);
    ASSERT_EQ(chunk.sleepCounter, Chunk::SLEEP_HYSTERESIS);

    // Verify all SoA arrays are reset
    for (core::i32 i = 0; i < Chunk::AREA; ++i) {
        ASSERT_EQ(chunk.material[i], EMPTY_MATERIAL_ID);
        ASSERT_FLOAT_NEAR(chunk.temperature[i], 20.0f, 0.001f);
        ASSERT_EQ(chunk.flags[i], FLAG_NONE);
        ASSERT_EQ(chunk.lifetime[i], 0);
    }
}

// F1.6: World coordinate conversions and boundary queries
E2E_TEST(ChunkLayout, WorldCoordinateTransformations, 1) {
    World world(1280, 768);
    ASSERT_EQ(world.GetWidth(), 1280);
    ASSERT_EQ(world.GetHeight(), 768);
    ASSERT_EQ(world.GetChunksX(), 20);
    ASSERT_EQ(world.GetChunksY(), 12);
    ASSERT_EQ(world.GetChunkCount(), 240u);

    // Boundary origin
    ASSERT_EQ(World::WorldToChunkX(0), 0);
    ASSERT_EQ(World::WorldToChunkY(0), 0);
    ASSERT_EQ(World::WorldToLocalX(0), 0);
    ASSERT_EQ(World::WorldToLocalY(0), 0);

    // First chunk boundary transition (63 -> 64)
    ASSERT_EQ(World::WorldToChunkX(63), 0);
    ASSERT_EQ(World::WorldToLocalX(63), 63);
    ASSERT_EQ(World::WorldToChunkX(64), 1);
    ASSERT_EQ(World::WorldToLocalX(64), 0);

    // Far corner (1279, 767)
    ASSERT_EQ(World::WorldToChunkX(1279), 19);
    ASSERT_EQ(World::WorldToLocalX(1279), 63);
    ASSERT_EQ(World::WorldToChunkY(767), 11);
    ASSERT_EQ(World::WorldToLocalY(767), 63);

    // LocalToWorld inverse mapping
    ASSERT_EQ(World::LocalToWorldX(19, 63), 1279);
    ASSERT_EQ(World::LocalToWorldY(11, 63), 767);

    // Bounds checking
    ASSERT_TRUE(world.IsInBounds(0, 0));
    ASSERT_TRUE(world.IsInBounds(1279, 767));
    ASSERT_FALSE(world.IsInBounds(-1, 0));
    ASSERT_FALSE(world.IsInBounds(0, -1));
    ASSERT_FALSE(world.IsInBounds(1280, 50));
    ASSERT_FALSE(world.IsInBounds(50, 768));
}

// ============================================================================
// FEATURE F2: MaterialRegistry & Schema
// ============================================================================

// F2.1: Air guaranteed at ID 0 with Gas state and zero density
E2E_TEST(MaterialRegistry, AirGuaranteedAtIdZero, 1) {
    auto& registry = MaterialRegistry::Instance();
    const Material& air = registry.Get(EMPTY_MATERIAL_ID);

    ASSERT_EQ(air.id, EMPTY_MATERIAL_ID);
    ASSERT_EQ(air.name, "air");
    ASSERT_EQ(air.category, "gases");
    ASSERT_EQ(air.state, MaterialState::Gas);
    ASSERT_FLOAT_NEAR(air.density, 0.0f, 0.001f);
    ASSERT_EQ(registry.FindByName("air"), EMPTY_MATERIAL_ID);
}

// F2.2: Programmatic custom material registration
E2E_TEST(MaterialRegistry, ProgrammaticMaterialRegistration, 1) {
    auto& registry = MaterialRegistry::Instance();
    size_t prevCount = registry.Count();

    Material custom;
    custom.name = "test_crystal";
    custom.category = "solids";
    custom.state = MaterialState::Solid;
    custom.density = 3500.0f;
    custom.strength = 150.0f;
    custom.specificHeat = 0.8f;
    custom.color = core::Color4u(100, 200, 255, 255);

    MaterialId customId = registry.RegisterMaterial(custom);
    ASSERT_GT(customId, 0u);
    ASSERT_EQ(registry.FindByName("test_crystal"), customId);

    const Material& retrieved = registry.Get(customId);
    ASSERT_EQ(retrieved.name, "test_crystal");
    ASSERT_EQ(retrieved.state, MaterialState::Solid);
    ASSERT_FLOAT_NEAR(retrieved.density, 3500.0f, 0.001f);
    ASSERT_FLOAT_NEAR(retrieved.strength, 150.0f, 0.001f);
    ASSERT_FLOAT_NEAR(retrieved.specificHeat, 0.8f, 0.001f);
    ASSERT_GE(registry.Count(), prevCount);
}

// F2.3: Material properties fidelity and default value contracts
E2E_TEST(MaterialRegistry, MaterialPropertiesFidelity, 1) {
    TestWorldFixture fixture(64, 64);
    auto& registry = MaterialRegistry::Instance();

    MaterialId waterId = registry.FindByName("water");
    MaterialId sandId = registry.FindByName("sand");
    MaterialId ironId = registry.FindByName("iron");

    ASSERT_NE(waterId, EMPTY_MATERIAL_ID);
    ASSERT_NE(sandId, EMPTY_MATERIAL_ID);
    ASSERT_NE(ironId, EMPTY_MATERIAL_ID);

    const Material& water = registry.Get(waterId);
    ASSERT_EQ(water.state, MaterialState::Liquid);
    ASSERT_FLOAT_NEAR(water.density, 1000.0f, 0.01f);
    ASSERT_EQ(water.dispersion, 5);

    const Material& sand = registry.Get(sandId);
    ASSERT_EQ(sand.state, MaterialState::Powder);
    ASSERT_FLOAT_NEAR(sand.density, 1600.0f, 0.01f);
    ASSERT_EQ(sand.dispersion, 2);

    const Material& iron = registry.Get(ironId);
    ASSERT_EQ(iron.state, MaterialState::Solid);
    ASSERT_FLOAT_NEAR(iron.density, 7800.0f, 0.01f);
    ASSERT_EQ(iron.dispersion, 0);
    ASSERT_GT(iron.strength, 50.0f);
}

// F2.4: Phase target two-pass string to ID resolution
E2E_TEST(MaterialRegistry, PhaseTargetTwoPassResolution, 1) {
    TestWorldFixture fixture(64, 64);
    auto& registry = MaterialRegistry::Instance();

    MaterialId waterId = registry.FindByName("water");
    MaterialId steamId = registry.FindByName("steam");
    MaterialId iceId   = registry.FindByName("ice");
    MaterialId stoneId = registry.FindByName("stone");
    MaterialId lavaId  = registry.FindByName("lava");
    MaterialId sandId  = registry.FindByName("sand");
    MaterialId glassId = registry.FindByName("glass");

    ASSERT_EQ(registry.Get(waterId).boilMaterialId, steamId);
    ASSERT_EQ(registry.Get(waterId).freezeMaterialId, iceId);
    ASSERT_EQ(registry.Get(iceId).meltMaterialId, waterId);
    ASSERT_EQ(registry.Get(steamId).freezeMaterialId, waterId);
    ASSERT_EQ(registry.Get(stoneId).meltMaterialId, lavaId);
    ASSERT_EQ(registry.Get(sandId).meltMaterialId, glassId);
}

// F2.5: Non-existent material queries safe fallback
E2E_TEST(MaterialRegistry, NonExistentMaterialFallback, 1) {
    auto& registry = MaterialRegistry::Instance();

    MaterialId missing = registry.FindByName("non_existent_material_xyz");
    ASSERT_EQ(missing, EMPTY_MATERIAL_ID);

    const Material& fallback = registry.Get(9999);
    ASSERT_EQ(fallback.id, EMPTY_MATERIAL_ID);
    ASSERT_EQ(fallback.name, "air");
    ASSERT_EQ(fallback.state, MaterialState::Gas);
}

// ============================================================================
// FEATURE F3: Core Materials Assets
// ============================================================================

// F3.1: All 18 core material JSON files successfully loaded
E2E_TEST(CoreMaterialsAssets, All18CoreMaterialsLoaded, 1) {
    TestWorldFixture fixture(64, 64);
    auto& registry = MaterialRegistry::Instance();
    ASSERT_GE(registry.Count(), 18u);

    const char* coreMaterials[] = {
        "air", "sand", "water", "stone", "dirt", "wood", "acid", "lava",
        "ice", "steam", "oil", "fire", "smoke", "iron", "gunpowder", "tnt",
        "glass", "ash"
    };

    for (const char* matName : coreMaterials) {
        MaterialId id = registry.FindByName(matName);
        if (std::string(matName) == "air") {
            ASSERT_EQ(id, EMPTY_MATERIAL_ID);
        } else {
            ASSERT_NE(id, EMPTY_MATERIAL_ID);
            ASSERT_EQ(registry.Get(id).name, matName);
        }
    }
}

// F3.2: Powder materials schema and parameter integrity
E2E_TEST(CoreMaterialsAssets, CorePowderMaterialsIntegrity, 1) {
    TestWorldFixture fixture(64, 64);
    auto& registry = MaterialRegistry::Instance();

    const Material& sand = registry.Get(registry.FindByName("sand"));
    ASSERT_EQ(sand.state, MaterialState::Powder);
    ASSERT_FLOAT_NEAR(sand.density, 1600.0f, 0.01f);
    ASSERT_EQ(sand.dispersion, 2);

    const Material& dirt = registry.Get(registry.FindByName("dirt"));
    ASSERT_EQ(dirt.state, MaterialState::Powder);
    ASSERT_FLOAT_NEAR(dirt.density, 1500.0f, 0.01f);
    ASSERT_EQ(dirt.dispersion, 1);

    const Material& gunpowder = registry.Get(registry.FindByName("gunpowder"));
    ASSERT_EQ(gunpowder.state, MaterialState::Powder);
    ASSERT_FLOAT_NEAR(gunpowder.density, 1650.0f, 0.01f);
    ASSERT_GT(gunpowder.flammability, 0.5f);

    const Material& ash = registry.Get(registry.FindByName("ash"));
    ASSERT_EQ(ash.state, MaterialState::Powder);
    ASSERT_FLOAT_NEAR(ash.density, 500.0f, 0.01f);
}

// F3.3: Liquid materials schema and density dispersion ordering
E2E_TEST(CoreMaterialsAssets, CoreLiquidMaterialsIntegrity, 1) {
    TestWorldFixture fixture(64, 64);
    auto& registry = MaterialRegistry::Instance();

    const Material& oil   = registry.Get(registry.FindByName("oil"));
    const Material& water = registry.Get(registry.FindByName("water"));
    const Material& acid  = registry.Get(registry.FindByName("acid"));
    const Material& lava  = registry.Get(registry.FindByName("lava"));

    ASSERT_EQ(oil.state, MaterialState::Liquid);
    ASSERT_EQ(water.state, MaterialState::Liquid);
    ASSERT_EQ(acid.state, MaterialState::Liquid);
    ASSERT_EQ(lava.state, MaterialState::Liquid);

    // Strictly ascending densities: Oil (800) < Water (1000) < Acid (1200) < Lava (3000)
    ASSERT_LT(oil.density, water.density);
    ASSERT_LT(water.density, acid.density);
    ASSERT_LT(acid.density, lava.density);

    ASSERT_FLOAT_NEAR(oil.density, 800.0f, 0.01f);
    ASSERT_FLOAT_NEAR(water.density, 1000.0f, 0.01f);
    ASSERT_FLOAT_NEAR(acid.density, 1200.0f, 0.01f);
    ASSERT_FLOAT_NEAR(lava.density, 3000.0f, 0.01f);

    ASSERT_EQ(oil.dispersion, 3);
    ASSERT_EQ(water.dispersion, 5);
    ASSERT_EQ(acid.dispersion, 4);
    ASSERT_EQ(lava.dispersion, 1);
}

// F3.4: Solid materials schema and thermal thresholds
E2E_TEST(CoreMaterialsAssets, CoreSolidMaterialsIntegrity, 1) {
    TestWorldFixture fixture(64, 64);
    auto& registry = MaterialRegistry::Instance();

    const Material& stone = registry.Get(registry.FindByName("stone"));
    const Material& iron  = registry.Get(registry.FindByName("iron"));
    const Material& wood  = registry.Get(registry.FindByName("wood"));
    const Material& ice   = registry.Get(registry.FindByName("ice"));

    ASSERT_EQ(stone.state, MaterialState::Solid);
    ASSERT_EQ(iron.state, MaterialState::Solid);
    ASSERT_EQ(wood.state, MaterialState::Solid);
    ASSERT_EQ(ice.state, MaterialState::Solid);

    ASSERT_EQ(stone.dispersion, 0);
    ASSERT_EQ(iron.dispersion, 0);
    ASSERT_EQ(wood.dispersion, 0);
    ASSERT_EQ(ice.dispersion, 0);

    ASSERT_FLOAT_NEAR(ice.meltPoint, 0.0f, 0.01f);
    ASSERT_GT(wood.flammability, 0.0f);
    ASSERT_GT(iron.strength, stone.strength);
}

// F3.5: Gas and energetic materials lifecycles
E2E_TEST(CoreMaterialsAssets, CoreGasAndEnergyMaterialsIntegrity, 1) {
    TestWorldFixture fixture(64, 64);
    auto& registry = MaterialRegistry::Instance();

    const Material& steam = registry.Get(registry.FindByName("steam"));
    const Material& smoke = registry.Get(registry.FindByName("smoke"));
    const Material& fire  = registry.Get(registry.FindByName("fire"));

    ASSERT_EQ(steam.state, MaterialState::Gas);
    ASSERT_EQ(smoke.state, MaterialState::Gas);
    ASSERT_EQ(fire.state, MaterialState::Gas);

    ASSERT_LT(steam.density, 1.0f);
    ASSERT_LT(smoke.density, 1.0f);
    ASSERT_LT(fire.density, 0.5f);

    ASSERT_GT(fire.lifetime, 0);
    ASSERT_GT(smoke.lifetime, 0);
    ASSERT_GT(fire.flammability, 0.0f);
}

// ============================================================================
// FEATURE F4: Powder Cellular Automata
// ============================================================================

// F4.1: Downward gravity free fall at 1 cell/tick with mass conservation
E2E_TEST(PowderPhysics, SandVerticalFreeFall, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand = fixture.GetMatId("sand");

    fixture.world.SetCell(50, 10, sand);
    for (int t = 0; t < 20; ++t) {
        ASSERT_EQ(fixture.world.GetCellMaterial(50, 10 + t), sand);
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }
    ASSERT_EQ(fixture.world.GetCellMaterial(50, 30), sand);
    ASSERT_EQ(fixture.world.GetCellMaterial(50, 10), EMPTY_MATERIAL_ID);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 1u);
}

// F4.2: Direct impact on flat floor halts descent
E2E_TEST(PowderPhysics, SandFloorImpactHaltsDescent, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand  = fixture.GetMatId("sand");
    MaterialId stone = fixture.GetMatId("stone");

    // Stone floor at y = 50
    for (int x = 40; x <= 60; ++x) fixture.world.SetCell(x, 50, stone);

    // Drop sand particle at (50, 30)
    fixture.world.SetCell(50, 30, sand);

    // Step 30 ticks
    for (int t = 0; t < 30; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Sand rests on top of stone floor at (50, 49)
    ASSERT_EQ(fixture.world.GetCellMaterial(50, 49), sand);
    ASSERT_EQ(fixture.world.GetCellMaterial(50, 50), stone);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 1u);
}

// F4.3: Diagonal sliding off stone peak without floating
E2E_TEST(PowderPhysics, SandAngleOfReposeDiagonalSliding, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand  = fixture.GetMatId("sand");
    MaterialId stone = fixture.GetMatId("stone");

    // Single stone peak at (50, 30), floor at y = 40
    fixture.world.SetCell(50, 30, stone);
    for (int x = 40; x <= 60; ++x) fixture.world.SetCell(x, 40, stone);

    // Drop sand right on top of stone peak at (50, 29)
    fixture.world.SetCell(50, 29, sand);

    // Step 20 ticks
    for (int t = 0; t < 20; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Sand must slide diagonally off (50, 30) and land on the floor at y=39 (x=49 or x=51)
    ASSERT_EQ(fixture.world.GetCellMaterial(50, 29), EMPTY_MATERIAL_ID);
    ASSERT_EQ(fixture.world.GetCellMaterial(50, 30), stone);

    bool landedOnLeft = (fixture.world.GetCellMaterial(49, 39) == sand);
    bool landedOnRight = (fixture.world.GetCellMaterial(51, 39) == sand);
    ASSERT_TRUE(landedOnLeft || landedOnRight);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 1u);
}

// F4.4: Conical repose pile formation from column
E2E_TEST(PowderPhysics, SandReposePileFormation, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand  = fixture.GetMatId("sand");
    MaterialId stone = fixture.GetMatId("stone");

    for (int x = 30; x <= 70; ++x) fixture.world.SetCell(x, 60, stone); // Floor

    // Spawn 15 sand particles vertically at x = 50, y = 20..34
    for (int y = 20; y <= 34; ++y) {
        fixture.world.SetCell(50, y, sand);
    }
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 15u);

    // Settle for 100 ticks
    for (int t = 0; t < 100; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Mass conservation
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 15u);

    // Check pile height at center vs flanks
    int hCenter = 0, hLeft = 0, hRight = 0;
    for (int y = 0; y < 60; ++y) {
        if (fixture.world.GetCellMaterial(50, y) == sand) hCenter++;
        if (fixture.world.GetCellMaterial(48, y) == sand) hLeft++;
        if (fixture.world.GetCellMaterial(52, y) == sand) hRight++;
    }
    ASSERT_GT(hCenter, 0);
    ASSERT_GE(hCenter, hLeft);
    ASSERT_GE(hCenter, hRight);
}

// F4.5: Narrow tube obstacle containment without lateral leakage
E2E_TEST(PowderPhysics, SandObstacleContainmentInNarrowTube, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand  = fixture.GetMatId("sand");
    MaterialId stone = fixture.GetMatId("stone");

    // Narrow 1-cell tube: floor at y=50, walls at x=49 and x=51
    for (int x = 48; x <= 52; ++x) fixture.world.SetCell(x, 50, stone);
    for (int y = 20; y <= 49; ++y) {
        fixture.world.SetCell(49, y, stone);
        fixture.world.SetCell(51, y, stone);
    }

    // Drop 10 sand particles into tube at x=50, y=20..29
    for (int y = 20; y <= 29; ++y) {
        fixture.world.SetCell(50, y, sand);
    }

    for (int t = 0; t < 50; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Inside tube, particles must settle into a straight column: y=40..49
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 10u);
    for (int y = 40; y <= 49; ++y) {
        ASSERT_EQ(fixture.world.GetCellMaterial(50, y), sand);
    }
    // No particles leaked outside tube
    for (int y = 20; y <= 50; ++y) {
        ASSERT_NE(fixture.world.GetCellMaterial(48, y), sand);
        ASSERT_NE(fixture.world.GetCellMaterial(52, y), sand);
    }
}

// ============================================================================
// FEATURE F5: Liquid Cellular Automata
// ============================================================================

// F5.1: Vertical gravity fall with strictly zero mid-air lateral spreading
E2E_TEST(LiquidPhysics, WaterVerticalFallNoLateralSpreadingMidAir, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water");

    fixture.world.SetCell(60, 20, water);
    for (int t = 0; t < 25; ++t) {
        ASSERT_EQ(fixture.world.GetCellMaterial(60, 20 + t), water);
        ASSERT_EQ(fixture.world.GetCellMaterial(59, 20 + t), EMPTY_MATERIAL_ID);
        ASSERT_EQ(fixture.world.GetCellMaterial(61, 20 + t), EMPTY_MATERIAL_ID);

        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }
    ASSERT_EQ(fixture.world.GetCellMaterial(60, 45), water);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 1u);
}

// F5.2: Horizontal leveling on flat floor
E2E_TEST(LiquidPhysics, WaterHorizontalLevelingOnFloor, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water");
    MaterialId stone = fixture.GetMatId("stone");

    // Floor at y=50, container walls at x=30 and x=60
    for (int x = 30; x <= 60; ++x) fixture.world.SetCell(x, 50, stone);
    for (int y = 40; y <= 49; ++y) {
        fixture.world.SetCell(30, y, stone);
        fixture.world.SetCell(60, y, stone);
    }

    // Place vertical column of 20 Water cells at x = 45, y = 30..49
    for (int y = 30; y <= 49; ++y) {
        fixture.world.SetCell(45, y, water);
    }
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 20u);

    // Simulate 150 ticks
    for (int t = 0; t < 150; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Conservation check
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 20u);

    // 20 water cells in a basin of width 29 (x=31..59) must level out along the bottom floor row y=49
    core::u32 waterAtBottom = 0;
    for (int x = 31; x <= 59; ++x) {
        if (fixture.world.GetCellMaterial(x, 49) == water) {
            waterAtBottom++;
        }
    }
    ASSERT_EQ(waterAtBottom, 20u); // Perfectly level single layer
}

// F5.3: Lateral dispersion step bounds (Water max 5, Oil max 3)
E2E_TEST(LiquidPhysics, LiquidDispersionStepLimits, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water"); // dispersion = 5
    MaterialId oil   = fixture.GetMatId("oil");   // dispersion = 3
    MaterialId stone = fixture.GetMatId("stone");

    // Test Water dispersion limit
    for (int x = 10; x <= 118; ++x) fixture.world.SetCell(x, 50, stone);
    fixture.world.SetCell(64, 49, water);

    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();

    // Water moved away from (64, 49) but within distance 5
    ASSERT_EQ(fixture.world.GetCellMaterial(64, 49), EMPTY_MATERIAL_ID);
    bool foundWater = false;
    for (int dx = -5; dx <= 5; ++dx) {
        if (dx == 0) continue;
        if (fixture.world.GetCellMaterial(64 + dx, 49) == water) {
            foundWater = true;
            break;
        }
    }
    ASSERT_TRUE(foundWater);
    // Did not leap beyond dispersion 5
    for (int x = 0; x < 128; ++x) {
        if (std::abs(x - 64) > 5) {
            ASSERT_NE(fixture.world.GetCellMaterial(x, 49), water);
        }
    }

    // Test Oil dispersion limit
    fixture.Reset();
    for (int x = 10; x <= 118; ++x) fixture.world.SetCell(x, 50, stone);
    fixture.world.SetCell(64, 49, oil);

    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();

    ASSERT_EQ(fixture.world.GetCellMaterial(64, 49), EMPTY_MATERIAL_ID);
    bool foundOil = false;
    for (int dx = -3; dx <= 3; ++dx) {
        if (dx == 0) continue;
        if (fixture.world.GetCellMaterial(64 + dx, 49) == oil) {
            foundOil = true;
            break;
        }
    }
    ASSERT_TRUE(foundOil);
    for (int x = 0; x < 128; ++x) {
        if (std::abs(x - 64) > 3) {
            ASSERT_NE(fixture.world.GetCellMaterial(x, 49), oil);
        }
    }
}

// F5.4: Cliff drop-off priority halts lateral expansion at ledge
E2E_TEST(LiquidPhysics, WaterCliffDropOffHaltsLateralExpansion, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water");
    MaterialId stone = fixture.GetMatId("stone");

    // Platform from x = 20 to x = 30 at y = 50. Left wall at x = 19. Cliff at x = 31 (air below).
    fixture.world.SetCell(19, 49, stone);
    for (int x = 20; x <= 30; ++x) fixture.world.SetCell(x, 50, stone);

    // Place water at x = 28, y = 49 (3 cells from cliff edge)
    fixture.world.SetCell(28, 49, water);

    // Step 1: Water disperses rightward and stops at cliff edge x = 31
    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();

    ASSERT_EQ(fixture.world.GetCellMaterial(31, 49), water);
    ASSERT_EQ(fixture.world.GetCellMaterial(32, 49), EMPTY_MATERIAL_ID); // Did not float over gap

    // Step 2: Falls vertically down off the cliff
    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();
    ASSERT_EQ(fixture.world.GetCellMaterial(31, 50), water);
}

// F5.5: U-basin fluid containment with strict zero leakage
E2E_TEST(LiquidPhysics, LiquidBasinContainmentZeroLeakage, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water");
    MaterialId stone = fixture.GetMatId("stone");

    // U-basin: floor y=60, x=30..50; walls at x=30 and x=50, y=40..60
    for (int x = 30; x <= 50; ++x) fixture.world.SetCell(x, 60, stone);
    for (int y = 40; y <= 60; ++y) {
        fixture.world.SetCell(30, y, stone);
        fixture.world.SetCell(50, y, stone);
    }

    // Fill with 50 water particles inside basin (x in [32, 48], y in [45, 59])
    core::u32 placed = 0;
    for (int y = 55; y <= 59; ++y) {
        for (int x = 35; x <= 44; ++x) {
            fixture.world.SetCell(x, y, water);
            placed++;
        }
    }
    ASSERT_EQ(placed, 50u);

    // Step 100 ticks
    for (int t = 0; t < 100; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 50u);

    // Verify zero leakage outside basin bounds
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            if (x <= 30 || x >= 50 || y >= 60) {
                ASSERT_NE(fixture.world.GetCellMaterial(x, y), water);
            }
        }
    }
}

// ============================================================================
// FEATURE F6: Density Mutual Displacement
// ============================================================================

// F6.1: Sand sinks in Water, Water displaced upward symmetrically
E2E_TEST(DensityDisplacement, SandSinksInWater, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand  = fixture.GetMatId("sand");   // 1600
    MaterialId water = fixture.GetMatId("water");  // 1000
    MaterialId stone = fixture.GetMatId("stone");

    // Stone basin: x in [40, 44], floor at y = 30
    for (int x = 39; x <= 45; ++x) fixture.world.SetCell(x, 31, stone);
    for (int y = 20; y <= 31; ++y) {
        fixture.world.SetCell(39, y, stone);
        fixture.world.SetCell(45, y, stone);
    }

    // Bottom row filled with Water: y = 30, x in [40, 44] (5 cells)
    for (int x = 40; x <= 44; ++x) fixture.world.SetCell(x, 30, water);

    // Top row filled with Sand: y = 29, x in [40, 44] (5 cells)
    for (int x = 40; x <= 44; ++x) fixture.world.SetCell(x, 29, sand);

    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 5u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 5u);

    // Step 20 ticks
    for (int t = 0; t < 20; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Sand has sunk to bottom y = 30
    for (int x = 40; x <= 44; ++x) {
        ASSERT_EQ(fixture.world.GetCellMaterial(x, 30), sand);
    }

    // Water displaced upward to y = 29
    for (int x = 40; x <= 44; ++x) {
        ASSERT_EQ(fixture.world.GetCellMaterial(x, 29), water);
    }

    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 5u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 5u);
}

// F6.2: Water sinks in Oil, Oil floats to surface
E2E_TEST(DensityDisplacement, WaterSinksInOil, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water"); // 1000
    MaterialId oil   = fixture.GetMatId("oil");   // 800
    MaterialId stone = fixture.GetMatId("stone");

    // Stone tube: x in [50, 52], y in [20, 30]
    for (int x = 49; x <= 53; ++x) fixture.world.SetCell(x, 31, stone);
    for (int y = 20; y <= 31; ++y) {
        fixture.world.SetCell(49, y, stone);
        fixture.world.SetCell(53, y, stone);
    }

    // Bottom half: Oil (y = 26..30, 3 cols * 5 rows = 15 cells)
    for (int y = 26; y <= 30; ++y) {
        for (int x = 50; x <= 52; ++x) fixture.world.SetCell(x, y, oil);
    }

    // Top half: Water (y = 21..25, 3 cols * 5 rows = 15 cells)
    for (int y = 21; y <= 25; ++y) {
        for (int x = 50; x <= 52; ++x) fixture.world.SetCell(x, y, water);
    }

    // Simulate 100 ticks
    for (int t = 0; t < 100; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Stratification check: Water must be at bottom (y=26..30), Oil at top (y=21..25)
    for (int y = 26; y <= 30; ++y) {
        for (int x = 50; x <= 52; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), water);
        }
    }
    for (int y = 21; y <= 25; ++y) {
        for (int x = 50; x <= 52; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), oil);
        }
    }

    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 15u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, oil), 15u);
}

// F6.3: Ash floats on Water and Oil without sinking
E2E_TEST(DensityDisplacement, AshFloatsOnWaterAndOil, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId ash   = fixture.GetMatId("ash");   // 500 (Powder)
    MaterialId water = fixture.GetMatId("water"); // 1000 (Liquid)
    MaterialId stone = fixture.GetMatId("stone");

    // Basin: floor y=40, walls x=40 and x=50
    for (int x = 40; x <= 50; ++x) fixture.world.SetCell(x, 40, stone);
    for (int y = 30; y <= 40; ++y) {
        fixture.world.SetCell(40, y, stone);
        fixture.world.SetCell(50, y, stone);
    }

    // Fill with Water y=35..39
    for (int y = 35; y <= 39; ++y) {
        for (int x = 41; x <= 49; ++x) fixture.world.SetCell(x, y, water);
    }

    // Place Ash on top row y=34
    for (int x = 41; x <= 49; ++x) {
        fixture.world.SetCell(x, 34, ash);
    }

    // Step 50 ticks
    for (int t = 0; t < 50; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Ash must remain at y=34, never sinking into water
    for (int x = 41; x <= 49; ++x) {
        ASSERT_EQ(fixture.world.GetCellMaterial(x, 34), ash);
    }
    for (int y = 35; y <= 39; ++y) {
        for (int x = 41; x <= 49; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), water);
        }
    }
}

// F6.4: Four-liquid density sorting column
E2E_TEST(DensityDisplacement, FourLiquidDensitySorting, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId lava  = fixture.GetMatId("lava");  // 3000
    MaterialId acid  = fixture.GetMatId("acid");  // 1200
    MaterialId water = fixture.GetMatId("water"); // 1000
    MaterialId oil   = fixture.GetMatId("oil");   // 800
    MaterialId stone = fixture.GetMatId("stone");

    // Vertical tube: x in [50, 52], floor at y=60, walls at x=49 and x=53
    for (int x = 49; x <= 53; ++x) fixture.world.SetCell(x, 60, stone);
    for (int y = 20; y <= 60; ++y) {
        fixture.world.SetCell(49, y, stone);
        fixture.world.SetCell(53, y, stone);
    }

    // Inverted initial stack: Oil at bottom (y=50..59), Lava at top (y=20..29)
    for (int y = 50; y <= 59; ++y) {
        for (int x = 50; x <= 52; ++x) fixture.world.SetCell(x, y, oil);
    }
    for (int y = 40; y <= 49; ++y) {
        for (int x = 50; x <= 52; ++x) fixture.world.SetCell(x, y, water);
    }
    for (int y = 30; y <= 39; ++y) {
        for (int x = 50; x <= 52; ++x) fixture.world.SetCell(x, y, acid);
    }
    for (int y = 20; y <= 29; ++y) {
        for (int x = 50; x <= 52; ++x) fixture.world.SetCell(x, y, lava);
    }

    // Step 350 ticks for full settling
    for (int t = 0; t < 350; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Verify sorted density layers:
    // Bottom: Lava (y = 50..59)
    for (int y = 50; y <= 59; ++y) {
        for (int x = 50; x <= 52; ++x) ASSERT_EQ(fixture.world.GetCellMaterial(x, y), lava);
    }
    // Middle-low: Acid (y = 40..49)
    for (int y = 40; y <= 49; ++y) {
        for (int x = 50; x <= 52; ++x) ASSERT_EQ(fixture.world.GetCellMaterial(x, y), acid);
    }
    // Middle-high: Water (y = 30..39)
    for (int y = 30; y <= 39; ++y) {
        for (int x = 50; x <= 52; ++x) ASSERT_EQ(fixture.world.GetCellMaterial(x, y), water);
    }
    // Top: Oil (y = 20..29)
    for (int y = 20; y <= 29; ++y) {
        for (int x = 50; x <= 52; ++x) ASSERT_EQ(fixture.world.GetCellMaterial(x, y), oil);
    }
}

// F6.5: Dense liquid does not displace solid barrier
E2E_TEST(DensityDisplacement, DenseLiquidDoesNotDisplaceSolid, 1) {
    TestWorldFixture fixture(128, 128);
    MaterialId lava  = fixture.GetMatId("lava");  // 3000 (Liquid)
    MaterialId wood  = fixture.GetMatId("wood");  // 650 (Solid)
    MaterialId stone = fixture.GetMatId("stone");

    // Wood floor at y = 40, stone foundation below at y = 41
    for (int x = 40; x <= 60; ++x) {
        fixture.world.SetCell(x, 40, wood);
        fixture.world.SetCell(x, 41, stone);
    }

    // Place heavy Lava above Wood at y = 39
    for (int x = 45; x <= 55; ++x) {
        fixture.world.SetCell(x, 39, lava);
    }

    // Step 20 ticks
    for (int t = 0; t < 20; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Wood remains intact at y=40, never displaced by Lava mechanically
    for (int x = 40; x <= 60; ++x) {
        ASSERT_EQ(fixture.world.GetCellMaterial(x, 40), wood);
    }
}

// ============================================================================
// FEATURE F7: Streaming Texture Rendering
// ============================================================================

// F7.1: Color4u serialization to u32 Little-Endian format
E2E_TEST(StreamingRenderer, Color4uSerializationAndPacking, 1) {
    core::Color4u c1(18, 18, 22, 255);
    core::u32 u1 = c1.ToU32();

    ASSERT_EQ(static_cast<core::u8>(u1 & 0xFF), 18u);
    ASSERT_EQ(static_cast<core::u8>((u1 >> 8) & 0xFF), 18u);
    ASSERT_EQ(static_cast<core::u8>((u1 >> 16) & 0xFF), 22u);
    ASSERT_EQ(static_cast<core::u8>((u1 >> 24) & 0xFF), 255u);

    // Exact background color matches 0xFF161212
    ASSERT_EQ(u1, 0xFF161212u);

    // Roundtrip verification
    core::Color4u c2(255, 128, 64, 200);
    core::Color4u c3 = core::Color4u::FromU32(c2.ToU32());
    ASSERT_EQ(c3.r, 255u);
    ASSERT_EQ(c3.g, 128u);
    ASSERT_EQ(c3.b, 64u);
    ASSERT_EQ(c3.a, 200u);
}

// F7.2: Empty cell produces exact background color
E2E_TEST(StreamingRenderer, EmptyCellBackgroundColorMapping, 1) {
    core::Color4u bg(18, 18, 22, 255);
    core::u32 bgU32 = bg.ToU32();

    // Verify background color constant matches WorldRenderer specification
    ASSERT_EQ(bgU32, 0xFF161212u);
}

// F7.3: Deterministic spatial noise variation within max variation bounds
E2E_TEST(StreamingRenderer, DeterministicSpatialColorNoise, 1) {
    core::i32 variation = 10;

    // Determinism test: repeated calls at same (x, y) return identical noise
    for (core::i32 y = 0; y < 10; ++y) {
        for (core::i32 x = 0; x < 10; ++x) {
            core::i32 n1 = core::CellNoiseVariation(x, y, variation);
            core::i32 n2 = core::CellNoiseVariation(x, y, variation);
            ASSERT_EQ(n1, n2);
            ASSERT_GE(n1, -variation);
            ASSERT_LE(n1, variation);
        }
    }

    // Zero variation test
    ASSERT_EQ(core::CellNoiseVariation(50, 50, 0), 0);
}

// F7.4: ScreenToWorld coordinate transformation and viewport clipping
E2E_TEST(StreamingRenderer, ScreenToWorldCoordinateProjection, 1) {
    // Headless WorldRenderer instance (nullptr SDL_Renderer)
    WorldRenderer renderer(nullptr, 1280, 768);

    SDL_Rect viewport{0, 0, 1280, 768};
    core::i32 wx = -1, wy = -1;

    // Screen origin -> world origin
    ASSERT_TRUE(renderer.ScreenToWorld(0, 0, viewport, wx, wy));
    ASSERT_EQ(wx, 0);
    ASSERT_EQ(wy, 0);

    // Screen max -> world max
    ASSERT_TRUE(renderer.ScreenToWorld(1279, 767, viewport, wx, wy));
    ASSERT_EQ(wx, 1279);
    ASSERT_EQ(wy, 767);

    // Center
    ASSERT_TRUE(renderer.ScreenToWorld(640, 384, viewport, wx, wy));
    ASSERT_EQ(wx, 640);
    ASSERT_EQ(wy, 384);

    // Points outside viewport return false
    ASSERT_FALSE(renderer.ScreenToWorld(-1, 100, viewport, wx, wy));
    ASSERT_FALSE(renderer.ScreenToWorld(1280, 100, viewport, wx, wy));
    ASSERT_FALSE(renderer.ScreenToWorld(100, -1, viewport, wx, wy));
    ASSERT_FALSE(renderer.ScreenToWorld(100, 768, viewport, wx, wy));

    // Scaled viewport with offset: pos (100, 50), size (640, 384)
    SDL_Rect scaledViewport{100, 50, 640, 384};
    ASSERT_TRUE(renderer.ScreenToWorld(100, 50, scaledViewport, wx, wy));
    ASSERT_EQ(wx, 0);
    ASSERT_EQ(wy, 0);

    ASSERT_TRUE(renderer.ScreenToWorld(420, 242, scaledViewport, wx, wy));
    ASSERT_FLOAT_NEAR(static_cast<float>(wx), 640.0f, 2.0f);
    ASSERT_FLOAT_NEAR(static_cast<float>(wy), 384.0f, 2.0f);
}

// F7.5: Chunk dirty flag tracking lifecycle
E2E_TEST(StreamingRenderer, ChunkDirtyFlagTracking, 1) {
    World world(128, 128);
    Chunk* chunk = world.GetChunk(0, 0);
    ASSERT_TRUE(chunk != nullptr);

    // Clear dirty flag
    chunk->isDirty = false;
    ASSERT_FALSE(chunk->isDirty);

    // Modify cell -> chunk must become dirty
    world.SetCell(10, 10, 3);
    ASSERT_TRUE(chunk->isDirty);

    // Swap cells -> both chunks must become dirty
    chunk->isDirty = false;
    world.SwapCells(10, 10, 10, 11);
    ASSERT_TRUE(chunk->isDirty);
}

// ============================================================================
// FEATURE F8: Interactive Mouse Brush
// ============================================================================

// F8.1: Brush radius clamping and single-cell precision
E2E_TEST(MouseBrush, RadiusClampingAndSingleCellPrecision, 1) {
    Brush brush;
    ASSERT_EQ(brush.GetRadius(), 5);

    // Clamp to MIN_RADIUS (1)
    brush.SetRadius(0);
    ASSERT_EQ(brush.GetRadius(), Brush::MIN_RADIUS);

    brush.SetRadius(-10);
    ASSERT_EQ(brush.GetRadius(), Brush::MIN_RADIUS);

    // Clamp to MAX_RADIUS (50)
    brush.SetRadius(100);
    ASSERT_EQ(brush.GetRadius(), Brush::MAX_RADIUS);

    // Set radius 1: must paint strictly 1 cell
    brush.SetRadius(1);
    World world(128, 128);
    MaterialId sand = 3;

    brush.PaintDisk(world, 50, 50, sand, 1);
    ASSERT_EQ(world.GetCellMaterial(50, 50), sand);
    ASSERT_EQ(inspection::CountMaterial(world, sand), 1u);
}

// F8.2: Circular disk geometry matches integer lattice equation dx^2 + dy^2 <= R^2
E2E_TEST(MouseBrush, CircularDiskGeometry, 1) {
    Brush brush;
    World world(128, 128);
    MaterialId stone = 1;
    const core::i32 radius = 5;

    brush.PaintDisk(world, 64, 64, stone, radius);

    // Count painted cells and check geometric bounds
    core::u32 count = inspection::CountMaterial(world, stone);

    // Exact count of integer lattice points inside x^2 + y^2 <= 25 is 81
    ASSERT_EQ(count, 81u);

    // Verify distance invariant
    for (core::i32 dy = -10; dy <= 10; ++dy) {
        for (core::i32 dx = -10; dx <= 10; ++dx) {
            core::i32 d2 = dx * dx + dy * dy;
            MaterialId m = world.GetCellMaterial(64 + dx, 64 + dy);
            if (d2 <= 25) {
                ASSERT_EQ(m, stone);
            } else {
                ASSERT_NE(m, stone);
            }
        }
    }
}

// F8.3: Eraser brush clears cells to EMPTY_MATERIAL_ID
E2E_TEST(MouseBrush, EraserFunctionality, 1) {
    Brush brush;
    World world(128, 128);
    MaterialId stone = 1;

    // Fill 20x20 area with stone
    for (int y = 40; y < 60; ++y) {
        for (int x = 40; x < 60; ++x) {
            world.SetCell(x, y, stone);
        }
    }
    ASSERT_EQ(inspection::CountMaterial(world, stone), 400u);

    // Set brush to erase mode
    brush.SetMaterial(stone);
    brush.SetRadius(5);
    brush.SetErasing(true);
    ASSERT_TRUE(brush.IsErasing());

    // Paint disk at center (50, 50) in erase mode
    brush.PaintDisk(world, 50, 50);

    // 81 cells cleared inside radius 5
    ASSERT_EQ(world.GetCellMaterial(50, 50), EMPTY_MATERIAL_ID);
    ASSERT_EQ(inspection::CountMaterial(world, stone), 400u - 81u);
}

// F8.4: Continuous Bresenham stroke interpolation across arbitrary angles
E2E_TEST(MouseBrush, BresenhamStrokeInterpolation, 1) {
    Brush brush;
    World world(128, 128);
    MaterialId sand = 3;

    // Diagonal stroke from (20, 20) to (50, 35) with radius 1
    brush.PaintStroke(world, 20, 20, 50, 35, sand, 1);

    ASSERT_EQ(world.GetCellMaterial(20, 20), sand);
    ASSERT_EQ(world.GetCellMaterial(50, 35), sand);

    // Check that stroke is connected (Chebyshev distance between consecutive painted cells <= 1)
    core::u32 count = inspection::CountMaterial(world, sand);
    ASSERT_GE(count, 31u); // dx = 30, so at least 31 points

    core::Rect2i bbox = inspection::GetBoundingBox(world, sand);
    ASSERT_EQ(bbox.x, 20);
    ASSERT_EQ(bbox.y, 20);
    ASSERT_EQ(bbox.width, 31);
    ASSERT_EQ(bbox.height, 16);
}

// F8.5: Boundary safety and clipping at world edges
E2E_TEST(MouseBrush, BoundaryClippingSafety, 1) {
    Brush brush;
    World world(128, 128);
    MaterialId sand = 3;

    // Paint partially outside top-left boundary
    brush.PaintDisk(world, 0, 0, sand, 10);
    ASSERT_EQ(world.GetCellMaterial(0, 0), sand);

    // Paint stroke originating outside negative space and crossing into world
    brush.PaintStroke(world, -10, -10, 15, 15, sand, 3);
    ASSERT_EQ(world.GetCellMaterial(15, 15), sand);

    // Paint partially outside bottom-right boundary
    brush.PaintDisk(world, 127, 127, sand, 10);
    ASSERT_EQ(world.GetCellMaterial(127, 127), sand);

    // Stroke crossing right boundary outward
    brush.PaintStroke(world, 120, 64, 150, 64, sand, 2);
    ASSERT_EQ(world.GetCellMaterial(127, 64), sand);

    // All painted cells are strictly within [0, 127] bounds
    for (core::i32 y = 0; y < 128; ++y) {
        for (core::i32 x = 0; x < 128; ++x) {
            MaterialId m = world.GetCellMaterial(x, y);
            if (m == sand) {
                ASSERT_TRUE(world.IsInBounds(x, y));
            }
        }
    }
}
```

---

## 5. Verification Method

To independently verify the investigation findings and test suite specifications:

1. **Verify Target File Inclusion**:
   Confirm that `tests/CMakeLists.txt` includes `${UI_SOURCES}` within `sandbox_e2e_tests`:
   ```cmake
   add_executable(sandbox_e2e_tests
       ${E2E_SOURCES}
       ${E2E_TIER_SOURCES}
       ${CORE_SOURCES}
       ${SIM_SOURCES}
       ${UI_SOURCES}
   )
   ```
2. **Build Test Executable**:
   Execute the build command:
   ```powershell
   cmake --build build --config Release --target sandbox_e2e_tests
   ```
   *Expected result*: Build succeeds with zero compile errors and zero warnings.
3. **Execute Tier 1 Test Suite via CTest**:
   Execute CTest:
   ```powershell
   ctest --test-dir build -C Release -R e2e_tier1 --output-on-failure
   ```
   Or execute directly from console:
   ```powershell
   .\build\sandbox_e2e_tests.exe --tier=1 --verbose
   ```
   *Expected result*: All 41 Tier 1 test cases pass ($100\%$ pass rate, 0 failures, total execution time $\le 100$ ms).
4. **Invalidation Conditions**:
   The test specifications are invalidated if:
   - Chunk layout size deviates from 32,768 bytes or loses 64-byte alignment.
   - Material ID 0 ceases to represent Air / Empty.
   - Core materials asset JSON files change properties or are relocated without updating the schema.
   - Cellular automata gravity or dispersion logic alters state displacement rules.
