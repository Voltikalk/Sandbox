#define SDL_MAIN_HANDLED
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "ASSERTION FAILED: " #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

#include "core/Types.hpp"
#include "core/Log.hpp"
#include "sim/Chunk.hpp"
#include "sim/World.hpp"
#include "sim/MaterialRegistry.hpp"
#include "sim/Physics.hpp"
#include "sim/Thermodynamics.hpp"
#include "sim/ReactionSystem.hpp"
#include "sim/ExplosionSystem.hpp"

using namespace sandbox;
using namespace sandbox::sim;

void InitRegistries() {
    auto& registry = MaterialRegistry::Instance();
    bool loaded = registry.LoadFromDirectory("assets/materials");
    if (!loaded) {
        loaded = registry.LoadFromDirectory("../assets/materials");
    }
    TEST_ASSERT(loaded && "Failed to load materials from assets/materials");

    auto& reactions = ReactionSystem::Instance();
    bool rxLoaded = reactions.LoadFromFile("assets/reactions.json", registry);
    if (!rxLoaded) {
        rxLoaded = reactions.LoadFromFile("../assets/reactions.json", registry);
    }
    TEST_ASSERT(rxLoaded && "Failed to load reactions from assets/reactions.json");
}

// =============================================================================
// TEST 1: Heat diffusion and CFL stability clamping
// =============================================================================
void TestHeatDiffusionAndStability() {
    std::cout << "[TEST] 1. Heat Diffusion, Harmonic Conductivity & CFL Stability Clamping..." << std::endl;

    Thermodynamics thermo;
    const auto& registry = MaterialRegistry::Instance();
    MaterialId ironId = registry.FindByName("iron");
    MaterialId airId = registry.FindByName("air");

    // 1.1 Harmonic Contact Conductivity Verification
    core::f32 k_same = Thermodynamics::CalculateContactConductivity(0.5f, 0.5f);
    TEST_ASSERT(std::abs(k_same - 0.5f) < 0.001f);

    core::f32 k_insulator = Thermodynamics::CalculateContactConductivity(0.0f, 1.0f);
    TEST_ASSERT(k_insulator < 0.001f);

    core::f32 k_iron_air = thermo.GetContactConductivity(ironId, airId);
    TEST_ASSERT(k_iron_air < 0.05f && "Air must act as thermal insulator for iron");

    // 1.2 Forward-Euler Stability & Absence of Overshoot
    World world(64, 64);
    world.Clear(airId, 20.0f);

    // Place cold center Iron cell surrounded by 4 hot Iron cells at 100°C
    core::i32 cx = 32, cy = 32;
    world.SetCell(cx, cy, ironId, 0.0f);
    world.SetCell(cx, cy - 1, ironId, 100.0f);
    world.SetCell(cx, cy + 1, ironId, 100.0f);
    world.SetCell(cx - 1, cy, ironId, 100.0f);
    world.SetCell(cx + 1, cy, ironId, 100.0f);

    core::f32 maxCenterTemp = 0.0f;
    for (int t = 0; t < 50; ++t) {
        thermo.Step(world);
        core::f32 curCenterTemp = world.GetCellTemperature(cx, cy);

        if (t == 0) {
            TEST_ASSERT(curCenterTemp > 0.0f && "Center cell must gain heat on tick 1");
        }

        // Strict CFL stability clamp: center temp NEVER overshoots neighbor temp (100°C)
        TEST_ASSERT(curCenterTemp >= 0.0f && curCenterTemp <= 100.0f);

        if (curCenterTemp > maxCenterTemp) {
            maxCenterTemp = curCenterTemp;
        }
    }
    TEST_ASSERT(maxCenterTemp > 30.0f && "Center cell must have reached substantial peak temperature");

    // 1.3 Ambient Heat Dissipation & Snapping Equilibrium
    world.Clear(airId, 20.0f);
    world.SetCell(10, 10, airId, 20.04f); // Within 0.05°C snapping threshold
    thermo.Step(world);
    TEST_ASSERT(world.GetCellTemperature(10, 10) == 20.0f && "Must snap to ambient within threshold");

    // 1.4 Cross-Chunk Boundary Heat Transfer & Neighbor Wake-up
    World world2(128, 64); // 2 chunks horizontally: Chunk(0,0) and Chunk(1,0)
    world2.Clear(airId, 20.0f);

    Chunk* chunk1 = world2.GetChunk(1, 0);
    TEST_ASSERT(chunk1 != nullptr);

    // Border cells: (63, 32) in Chunk 0, (64, 32) in Chunk 1
    world2.SetCell(63, 32, ironId, 100.0f);
    world2.SetCell(64, 32, ironId, 20.0f);

    // Put chunk 1 into sleep state
    chunk1->isSleeping = true;
    chunk1->sleepCounter = 0;

    thermo.Step(world2);

    TEST_ASSERT(world2.GetCellTemperature(64, 32) > 20.0f && "Heat must cross chunk boundary");
    TEST_ASSERT(!chunk1->isSleeping && "Sleeping neighbor chunk must be woken by boundary heat");

    std::cout << "  -> Heat diffusion, harmonic table, stability clamp & boundary wake PASSED." << std::endl;
}

// =============================================================================
// TEST 2: Material phase transitions with latent heat offset
// =============================================================================
void TestPhaseTransitionsAndLatentHeat() {
    std::cout << "[TEST] 2. Material Phase Transitions & Latent Heat Hysteresis Band..." << std::endl;

    Thermodynamics thermo;
    const auto& registry = MaterialRegistry::Instance();
    MaterialId iceId = registry.FindByName("ice");
    MaterialId waterId = registry.FindByName("water");
    MaterialId steamId = registry.FindByName("steam");
    MaterialId lavaId = registry.FindByName("lava");
    MaterialId stoneId = registry.FindByName("stone");
    MaterialId airId = registry.FindByName("air");

    World world(64, 64);
    world.Clear(airId, 20.0f);

    // 2.1 Ice Melting (melt_point = 0.0, latent offset = 3.0)
    world.SetCell(10, 10, iceId, 5.0f);
    thermo.Step(world);
    TEST_ASSERT(world.GetCellMaterial(10, 10) == waterId);
    core::f32 t10 = world.GetCellTemperature(10, 10);
    TEST_ASSERT(t10 < 4.0f && t10 > 1.0f && "Latent heat must be absorbed");

    // 2.2 Water Boiling (boil_point = 100.0, latent offset = 3.0)
    world.SetCell(15, 15, waterId, 110.0f);
    thermo.Step(world);
    TEST_ASSERT(world.GetCellMaterial(15, 15) == steamId);
    TEST_ASSERT(world.GetCellTemperature(15, 15) < 108.0f && "Latent heat must be absorbed");
    TEST_ASSERT(world.GetCellLifetime(15, 15) > 0 && "Steam must initialize lifetime");

    // 2.3 Steam Condensing (freeze_point = 95.0, latent offset = 3.0)
    world.SetCell(20, 20, steamId, 85.0f, FLAG_NONE, 100);
    thermo.Step(world);
    TEST_ASSERT(world.GetCellMaterial(20, 20) == waterId);
    TEST_ASSERT(world.GetCellTemperature(20, 20) > 80.0f && "Latent heat must be released");

    // 2.4 Lava Freezing (freeze_point = 700.0, latent offset = 3.0)
    world.SetCell(25, 25, lavaId, 650.0f);
    thermo.Step(world);
    TEST_ASSERT(world.GetCellMaterial(25, 25) == stoneId);
    TEST_ASSERT(world.GetCellTemperature(25, 25) > 600.0f && "Latent heat must be released");

    // 2.5 Hysteresis Band Prevents Oscillation Ping-Pong
    // Water placed at 110.0°C boils to Steam
    world.SetCell(30, 30, waterId, 110.0f);
    thermo.Step(world);
    TEST_ASSERT(world.GetCellMaterial(30, 30) == steamId);

    // On the subsequent tick, Steam must NOT condense back to Water
    thermo.Step(world);
    TEST_ASSERT(world.GetCellMaterial(30, 30) == steamId && "Hysteresis band must prevent ping-pong flip back");

    std::cout << "  -> Phase transitions (melting, boiling, condensation, freezing) & hysteresis PASSED." << std::endl;
}

// =============================================================================
// TEST 3: O(1) ReactionSystem lookup and reaction execution
// =============================================================================
void TestReactionSystem() {
    std::cout << "[TEST] 3. O(1) ReactionSystem Lookup & Reaction Execution..." << std::endl;

    auto& reactions = ReactionSystem::Instance();
    const auto& registry = MaterialRegistry::Instance();
    MaterialId waterId = registry.FindByName("water");
    MaterialId lavaId = registry.FindByName("lava");
    MaterialId steamId = registry.FindByName("steam");
    MaterialId stoneId = registry.FindByName("stone");
    MaterialId fireId = registry.FindByName("fire");
    MaterialId acidId = registry.FindByName("acid");
    MaterialId airId = registry.FindByName("air");

    // 3.1 Table Symmetry Verification
    const ReactionResult* rxDirect = reactions.GetReaction(waterId, lavaId);
    const ReactionResult* rxInverse = reactions.GetReaction(lavaId, waterId);

    TEST_ASSERT(rxDirect != nullptr);
    TEST_ASSERT(rxInverse != nullptr);
    TEST_ASSERT(rxDirect->productA == steamId && rxDirect->productB == stoneId);
    TEST_ASSERT(rxInverse->productA == stoneId && rxInverse->productB == steamId);
    TEST_ASSERT(reactions.GetReaction(airId, stoneId) == nullptr);

    // 3.2 Water + Lava Reaction Execution
    World world(64, 64);
    world.Clear(airId, 20.0f);

    world.SetCell(10, 10, waterId, 20.0f);
    world.SetCell(11, 10, lavaId, 800.0f);

    reactions.Step(world);

    TEST_ASSERT(world.GetCellMaterial(10, 10) == steamId);
    TEST_ASSERT(world.GetCellMaterial(11, 10) == stoneId);
    TEST_ASSERT(world.GetCellTemperature(10, 10) > 20.0f && "Reaction heatDelta applied");

    // 3.3 Water + Fire Extinguish Reaction
    world.SetCell(20, 20, waterId, 20.0f);
    world.SetCell(20, 21, fireId, 500.0f, FLAG_BURNING, 30);

    reactions.Step(world);

    TEST_ASSERT(world.GetCellMaterial(20, 20) == steamId);
    TEST_ASSERT(world.GetCellMaterial(20, 21) == airId);

    // 3.4 Acid Corrosion of Stone
    world.SetCell(30, 30, acidId, 20.0f);
    world.SetCell(30, 31, stoneId, 20.0f);

    bool stoneCorroded = false;
    for (int t = 0; t < 25; ++t) {
        reactions.Step(world);
        if (world.GetCellMaterial(30, 31) != stoneId) {
            stoneCorroded = true;
            break;
        }
    }
    TEST_ASSERT(stoneCorroded && "Acid must corrode stone block within 25 ticks");

    std::cout << "  -> ReactionSystem dense table, symmetric lookup, water+lava, water+fire & acid PASSED." << std::endl;
}

// =============================================================================
// TEST 4: Gas cellular automata (buoyancy, drift, lifetime decay)
// =============================================================================
void TestGasCellularAutomata() {
    std::cout << "[TEST] 4. Gas Cellular Automata (Buoyancy, Drift, Lifetime Decay & Parity)..." << std::endl;

    Physics physics(1337u);
    const auto& registry = MaterialRegistry::Instance();
    MaterialId smokeId = registry.FindByName("smoke");
    MaterialId steamId = registry.FindByName("steam");
    MaterialId airId = registry.FindByName("air");

    World world(64, 64);
    world.Clear(airId, 20.0f);

    // 4.1 Buoyancy (Negative Gravity)
    world.SetCell(32, 50, smokeId, 50.0f, FLAG_NONE, 100);
    for (int t = 0; t < 5; ++t) {
        physics.Step(world);
        world.AdvanceTick();
    }
    // Particle must have moved upward (accounting for Brownian diagonal drift)
    bool foundAbove = false;
    for (core::i32 y = 0; y < 50; ++y) {
        for (core::i32 x = 0; x < 64; ++x) {
            if (world.GetCellMaterial(x, y) == smokeId) {
                foundAbove = true;
                break;
            }
        }
        if (foundAbove) break;
    }
    TEST_ASSERT(foundAbove && "Smoke must rise upward due to buoyancy");

    // 4.2 Brownian Horizontal Dispersion
    world.Clear(airId, 20.0f);
    for (core::i32 y = 45; y <= 55; ++y) {
        world.SetCell(32, y, smokeId, 50.0f, FLAG_NONE, 100);
    }
    for (int t = 0; t < 20; ++t) {
        physics.Step(world);
        world.AdvanceTick();
    }
    bool dispersedLeft = false;
    bool dispersedRight = false;
    for (core::i32 y = 0; y < 64; ++y) {
        for (core::i32 x = 0; x < 64; ++x) {
            if (world.GetCellMaterial(x, y) == smokeId) {
                if (x < 32) dispersedLeft = true;
                if (x > 32) dispersedRight = true;
            }
        }
    }
    TEST_ASSERT(dispersedLeft && dispersedRight && "Smoke must disperse laterally");

    // 4.3 Lifetime Decay to Air
    world.Clear(airId, 20.0f);
    world.SetCell(20, 20, smokeId, 25.0f, FLAG_NONE, 3);
    for (int t = 0; t < 5; ++t) {
        physics.Step(world);
        world.AdvanceTick();
    }
    TEST_ASSERT(world.GetTotalActiveCells() == 0 && "Smoke with lifetime 3 must decay completely to Air");

    // 4.4 Parity Bit Prevents Multi-Stepping Rocket Cascade
    world.Clear(airId, 20.0f);
    for (core::i32 x = 20; x < 30; ++x) {
        world.SetCell(x, 50, steamId, 100.0f, FLAG_NONE, 100);
    }
    physics.Step(world);
    world.AdvanceTick();

    // After exactly 1 tick, steam must have moved at most 1 cell upward (to y >= 49)
    for (core::i32 y = 0; y < 49; ++y) {
        for (core::i32 x = 0; x < 64; ++x) {
            TEST_ASSERT(world.GetCellMaterial(x, y) != steamId && "Gas must NOT rocket upwards more than 1 cell in a single tick");
        }
    }

    std::cout << "  -> Gas buoyancy, dispersion, lifetime decay, and single-tick parity PASSED." << std::endl;
}

// =============================================================================
// TEST 5: Fire cellular automata (combustion, ignition, smoke/ash emission)
// =============================================================================
void TestFireCellularAutomata() {
    std::cout << "[TEST] 5. Fire Cellular Automata (Combustion, High Temp, Ignition & Emissions)..." << std::endl;

    Physics physics(777u);
    const auto& registry = MaterialRegistry::Instance();
    MaterialId fireId = registry.FindByName("fire");
    MaterialId woodId = registry.FindByName("wood");
    MaterialId stoneId = registry.FindByName("stone");
    MaterialId smokeId = registry.FindByName("smoke");
    MaterialId ashId = registry.FindByName("ash");
    MaterialId airId = registry.FindByName("air");

    World world(64, 64);
    world.Clear(airId, 20.0f);

    // 5.1 Fire Maintains High Core Temperature (400-800°C)
    world.SetCell(20, 20, fireId, 20.0f, FLAG_BURNING, 30);
    physics.Step(world);
    world.AdvanceTick();
    // Fire flickers upward/sideways; find the active fire particle
    core::f32 fireTemp = 0.0f;
    for (core::i32 y = 19; y <= 20; ++y) {
        for (core::i32 x = 19; x <= 21; ++x) {
            if (world.GetCellMaterial(x, y) == fireId) {
                fireTemp = world.GetCellTemperature(x, y);
                break;
            }
        }
        if (fireTemp > 0.0f) break;
    }
    TEST_ASSERT(fireTemp >= 400.0f && fireTemp <= 800.0f);

    // 5.2 Flammable Ignition (Wood adjacent to Fire in hearth)
    world.Clear(airId, 20.0f);
    // Preheated wood near ignition temp (240°C, ignition temp is 250°C)
    world.SetCell(30, 30, woodId, 240.0f);
    world.SetCell(31, 30, fireId, 600.0f, FLAG_BURNING, 30);
    // Hearth bricks prevent fire from escaping horizontally/vertically away from wood
    world.SetCell(31, 29, stoneId, 20.0f);
    world.SetCell(32, 29, stoneId, 20.0f);
    world.SetCell(32, 30, stoneId, 20.0f);

    bool woodIgnited = false;
    for (int t = 0; t < 20; ++t) {
        physics.Step(world);
        world.AdvanceTick();
        MaterialId m = world.GetCellMaterial(30, 30);
        if (m == fireId || m == ashId) {
            woodIgnited = true;
            break;
        }
    }
    TEST_ASSERT(woodIgnited && "Wood adjacent to Fire must ignite or burn into Ash");

    // 5.3 Non-Flammable Resistance (Stone adjacent to Fire)
    world.Clear(airId, 20.0f);
    world.SetCell(40, 40, stoneId, 20.0f);
    world.SetCell(40, 41, fireId, 600.0f, FLAG_BURNING, 30);
    for (int t = 0; t < 10; ++t) {
        physics.Step(world);
        world.AdvanceTick();
    }
    TEST_ASSERT(world.GetCellMaterial(40, 40) == stoneId && "Stone must resist fire and never ignite");

    // 5.4 Combustion Product Emission (Fire with lifetime 1)
    world.Clear(airId, 20.0f);
    world.SetCell(15, 15, fireId, 600.0f, FLAG_BURNING, 1);
    physics.Step(world);
    world.AdvanceTick();
    MaterialId product = world.GetCellMaterial(15, 15);
    TEST_ASSERT(product == smokeId || product == ashId || product == airId);

    std::cout << "  -> Fire core temperature, wood ignition, stone resistance & product emissions PASSED." << std::endl;
}

// =============================================================================
// TEST 6: ExplosionSystem (raycast shockwave, obstacle shielding, TNT chain)
// =============================================================================
void TestExplosionSystem() {
    std::cout << "[TEST] 6. ExplosionSystem (Raycast Shockwave, Obstacle Shielding & TNT Chain)..." << std::endl;

    auto& explosions = ExplosionSystem::Instance();
    const auto& registry = MaterialRegistry::Instance();
    MaterialId stoneId = registry.FindByName("stone");
    MaterialId sandId = registry.FindByName("sand");
    MaterialId tntId = registry.FindByName("tnt");
    MaterialId airId = registry.FindByName("air");

    World world(64, 64);
    world.Clear(airId, 20.0f);

    // 6.1 Open Crater Excavation & Flash Heating
    for (core::i32 dy = -8; dy <= 8; ++dy) {
        for (core::i32 dx = -8; dx <= 8; ++dx) {
            world.SetCell(32 + dx, 32 + dy, sandId, 20.0f);
        }
    }
    explosions.TriggerExplosion(world, ExplosionEvent{32, 32, 100.0f, 6, 1500.0f, 0});
    // Epicenter must be excavated
    TEST_ASSERT(world.GetCellMaterial(32, 32) != sandId && "Epicenter sand must be vaporized/destroyed");
    // Near-fringe cells within blast radius (r = 4 or 5) must experience thermal flash
    core::f32 fringeTemp = world.GetCellTemperature(32, 36);
    TEST_ASSERT(fringeTemp > 25.0f && "Fringe cell must experience thermal flash");

    // 6.2 Obstacle Shielding (Stone Wall Blast Shadow)
    world.Clear(airId, 20.0f);
    core::i32 epicX = 20, epicY = 32;

    // Build 3-cell thick Stone wall at x in [24, 26], y in [15, 49]
    for (core::i32 wx = 24; wx <= 26; ++wx) {
        for (core::i32 wy = 15; wy <= 49; ++wy) {
            world.SetCell(wx, wy, stoneId, 20.0f);
        }
    }

    // Shielded target behind wall at (30, 32) (distance 10 from epicenter)
    world.SetCell(30, 32, sandId, 20.0f);

    // Symmetric unshielded target in open air at (10, 32) (distance 10 from epicenter)
    world.SetCell(10, 32, sandId, 20.0f);

    // Trigger explosion with power 90 (distance 10, Sand strength 15, Stone strength 80)
    // Ray energy reaches wall with ~90 * (1 - 4/15) = 66 energy.
    // 66 < 80 (Stone strength), so ray is STOPPED by Stone wall!
    explosions.TriggerExplosion(world, ExplosionEvent{epicX, epicY, 90.0f, 15, 1500.0f, 0});

    // Unshielded target must be destroyed
    TEST_ASSERT(world.GetCellMaterial(10, 32) != sandId && "Unshielded target in open air must be destroyed");

    // Shielded target behind the stone wall must be completely INTACT!
    TEST_ASSERT(world.GetCellMaterial(30, 32) == sandId && "Shielded target behind stone wall MUST remain intact");

    // 6.3 TNT Secondary Chain Reactions
    world.Clear(airId, 20.0f);
    world.SetCell(15, 32, tntId, 20.0f);
    world.SetCell(28, 32, tntId, 20.0f); // within radius 20 of TNT 1
    world.SetCell(41, 32, tntId, 20.0f); // within radius 20 of TNT 2, but out of radius of TNT 1

    explosions.TriggerExplosion(world, ExplosionEvent{15, 32, 120.0f, 20, 2000.0f, 0});

    // All 3 TNT blocks must have detonated in chain reaction!
    TEST_ASSERT(world.GetCellMaterial(15, 32) != tntId);
    TEST_ASSERT(world.GetCellMaterial(28, 32) != tntId);
    TEST_ASSERT(world.GetCellMaterial(41, 32) != tntId && "TNT 3 must have detonated via secondary chain reaction from TNT 2");

    std::cout << "  -> Explosion raycast shockwave, obstacle shielding & TNT chain reactions PASSED." << std::endl;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "==========================================================" << std::endl;
    std::cout << "  MILESTONE 2: THERMODYNAMICS, REACTIONS & EXPLOSIONS     " << std::endl;
    std::cout << "==========================================================" << std::endl;

    InitRegistries();

    TestHeatDiffusionAndStability();
    TestPhaseTransitionsAndLatentHeat();
    TestReactionSystem();
    TestGasCellularAutomata();
    TestFireCellularAutomata();
    TestExplosionSystem();

    std::cout << "==========================================================" << std::endl;
    std::cout << "  ALL 6 MILESTONE 2 UNIT TESTS PASSED WITH 100% SUCCESS!  " << std::endl;
    std::cout << "==========================================================" << std::endl;

    return 0;
}
