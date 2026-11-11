#include "e2e_framework.hpp"
#include <cmath>
#include <iostream>
#include <vector>

using namespace sandbox;
using namespace sandbox::sim;
using namespace sandbox::test;

// ============================================================================
// SUITE 1: Powder Cellular Automata Empirical Stress Tests
// ============================================================================

// 1.1 Downward gravity & mass conservation
E2E_TEST(AdversarialPowder, DownwardGravityAndMassConservation, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand = fixture.GetMatId("sand");
    MaterialId stone = fixture.GetMatId("stone");

    // Single particle vertical fall
    fixture.world.SetCell(50, 10, sand);
    for (int t = 0; t < 30; ++t) {
        ASSERT_EQ(fixture.world.GetCellMaterial(50, 10 + t), sand);
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }
    ASSERT_EQ(fixture.world.GetCellMaterial(50, 40), sand);
    ASSERT_EQ(fixture.world.GetCellMaterial(50, 10), EMPTY_MATERIAL_ID);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 1u);

    // Multi-particle column falling onto flat floor
    fixture.Reset();
    // Floor at y = 60
    for (int x = 40; x <= 60; ++x) {
        fixture.world.SetCell(x, 60, stone);
    }
    // Column of 20 sand particles at x = 50, y = 10..29
    for (int y = 10; y <= 29; ++y) {
        fixture.world.SetCell(50, y, sand);
    }
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 20u);

    // Step until all particles settle (maximum 100 ticks)
    for (int t = 0; t < 100; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Mass conservation: strictly 20 sand particles
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 20u);

    // In open air: forms a repose pile on the floor (y=56..59, x=46..54)
    for (int y = 56; y <= 59; ++y) {
        for (int x = 46; x <= 54; ++x) {
            if (fixture.world.GetCellMaterial(x, y) == sand) {
                // Every sand particle is in the pile
            }
        }
    }
    // No particles floating in air above y=55
    for (int y = 0; y <= 55; ++y) {
        for (int x = 0; x < 128; ++x) {
            ASSERT_NE(fixture.world.GetCellMaterial(x, y), sand);
        }
    }

    // Now test column falling inside a 1-cell wide tube (walls at x=49 and x=51)
    fixture.Reset();
    for (int x = 48; x <= 52; ++x) fixture.world.SetCell(x, 60, stone); // floor
    for (int y = 10; y <= 59; ++y) {
        fixture.world.SetCell(49, y, stone); // left tube wall
        fixture.world.SetCell(51, y, stone); // right tube wall
    }
    // Place column of 20 sand particles inside the tube: x=50, y=10..29
    for (int y = 10; y <= 29; ++y) {
        fixture.world.SetCell(50, y, sand);
    }
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 20u);

    for (int t = 0; t < 100; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Inside the tube, the 20 particles must settle into a straight vertical column: y=40..59
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 20u);
    for (int y = 40; y <= 59; ++y) {
        ASSERT_EQ(fixture.world.GetCellMaterial(50, y), sand);
    }
    for (int y = 10; y <= 39; ++y) {
        ASSERT_EQ(fixture.world.GetCellMaterial(50, y), EMPTY_MATERIAL_ID);
    }

    // Nothing above y=39
    for (int y = 10; y <= 39; ++y) {
        ASSERT_EQ(fixture.world.GetCellMaterial(50, y), EMPTY_MATERIAL_ID);
    }
}

// 1.2 Angle of repose pile symmetry and absence of horizontal bias over 1000+ ticks
E2E_TEST(AdversarialPowder, AngleOfReposeSymmetry1000Ticks, 5) {
    TestWorldFixture fixture(128, 128, 424242u);
    MaterialId sand = fixture.GetMatId("sand");
    MaterialId stone = fixture.GetMatId("stone");

    // Floor at y = 80 from x = 10 to x = 118
    for (int x = 10; x <= 118; ++x) {
        fixture.world.SetCell(x, 80, stone);
    }

    // Drop 1000 sand particles at x = 64, y = 10
    // Every 2 ticks, emit 1 sand particle, total 2000 ticks
    const int totalParticles = 1000;
    int emitted = 0;

    for (int tick = 0; tick < 2500; ++tick) {
        if (emitted < totalParticles && (tick % 2 == 0)) {
            // Only emit if spawn cell is clear; otherwise particle sits at emitter
            if (fixture.world.GetCellMaterial(64, 10) == EMPTY_MATERIAL_ID) {
                fixture.world.SetCell(64, 10, sand);
                emitted++;
            }
        }
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Settle for another 300 ticks to ensure full repose equilibrium
    for (int t = 0; t < 300; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Verify 100% of emitted particles are in the world
    core::u32 countInWorld = inspection::CountMaterial(fixture.world, sand);
    ASSERT_EQ(countInWorld, static_cast<core::u32>(emitted));
    ASSERT_GE(countInWorld, 950u); // At least 950 particles emitted

    // Count particles on left (x < 64) vs right (x > 64)
    core::u32 leftCount = 0;
    core::u32 rightCount = 0;
    core::u32 centerCount = 0;

    for (int y = 0; y < 80; ++y) {
        for (int x = 10; x <= 118; ++x) {
            if (fixture.world.GetCellMaterial(x, y) == sand) {
                if (x < 64) leftCount++;
                else if (x > 64) rightCount++;
                else centerCount++;
            }
        }
    }

    // Statistical symmetry test:
    // Left vs Right counts should be well balanced (no systematic horizontal bias)
    double diff = std::abs(static_cast<double>(leftCount) - static_cast<double>(rightCount));
    double totalSides = static_cast<double>(leftCount + rightCount);
    double sigma = std::sqrt(totalSides * 0.5 * 0.5); // Binomial std dev

    // Within 3.5 standard deviations (p > 0.001)
    ASSERT_LE(diff, 3.5 * sigma);

    // Center of mass X must be extremely close to 64.0 (within 0.5 cells)
    auto [comX, comY] = inspection::GetCenterOfMass(fixture.world, sand);
    ASSERT_FLOAT_NEAR(comX, 64.0f, 0.5f);

    // Repose slope check: height must decrease smoothly away from center
    // Check height at center vs x = 50 and x = 78
    int heightCenter = 0, heightLeft = 0, heightRight = 0;
    for (int y = 0; y < 80; ++y) {
        if (fixture.world.GetCellMaterial(64, y) == sand) heightCenter++;
        if (fixture.world.GetCellMaterial(50, y) == sand) heightLeft++;
        if (fixture.world.GetCellMaterial(78, y) == sand) heightRight++;
    }
    ASSERT_GT(heightCenter, heightLeft);
    ASSERT_GT(heightCenter, heightRight);
    // Symmetry of flank heights
    ASSERT_LE(std::abs(heightLeft - heightRight), 3);
}

// ============================================================================
// SUITE 2: Liquid Cellular Automata Empirical Stress Tests
// ============================================================================

// 2.1 Downward fall without lateral spreading mid-air
E2E_TEST(AdversarialLiquid, MidAirFallNoLateralSpread, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water");

    fixture.world.SetCell(60, 20, water);
    for (int t = 0; t < 25; ++t) {
        ASSERT_EQ(fixture.world.GetCellMaterial(60, 20 + t), water);
        // Ensure strictly no lateral spreading into x=59 or x=61 during vertical fall
        ASSERT_EQ(fixture.world.GetCellMaterial(59, 20 + t), EMPTY_MATERIAL_ID);
        ASSERT_EQ(fixture.world.GetCellMaterial(61, 20 + t), EMPTY_MATERIAL_ID);

        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }
    ASSERT_EQ(fixture.world.GetCellMaterial(60, 45), water);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 1u);
}

// 2.2 Lateral dispersion limits for different liquids & obstacle containment
E2E_TEST(AdversarialLiquid, LateralDispersionDistancesAndObstacles, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water"); // dispersion = 5
    MaterialId oil   = fixture.GetMatId("oil");   // dispersion = 3
    MaterialId lava  = fixture.GetMatId("lava");  // dispersion = 1
    MaterialId stone = fixture.GetMatId("stone");

    // Test Water dispersion (max 5 cells per tick)
    for (int x = 10; x <= 118; ++x) fixture.world.SetCell(x, 50, stone);
    fixture.world.SetCell(64, 49, water);
    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();

    // Water moved away from center (64, 49)
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
    // Water did NOT jump further than dispersion=5
    for (int x = 0; x < 128; ++x) {
        if (std::abs(x - 64) > 5) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, 49), EMPTY_MATERIAL_ID);
        }
    }

    // Test Oil dispersion (max 3 cells per tick)
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
            ASSERT_EQ(fixture.world.GetCellMaterial(x, 49), EMPTY_MATERIAL_ID);
        }
    }

    // Test Lava dispersion (max 1 cell per tick)
    fixture.Reset();
    for (int x = 10; x <= 118; ++x) fixture.world.SetCell(x, 50, stone);
    fixture.world.SetCell(64, 49, lava);
    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();

    ASSERT_EQ(fixture.world.GetCellMaterial(64, 49), EMPTY_MATERIAL_ID);
    bool foundLava = (fixture.world.GetCellMaterial(63, 49) == lava || fixture.world.GetCellMaterial(65, 49) == lava);
    ASSERT_TRUE(foundLava);
    for (int x = 0; x < 128; ++x) {
        if (std::abs(x - 64) > 1) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, 49), EMPTY_MATERIAL_ID);
        }
    }

    // Test obstacle containment: wall at distance 2 must NOT be tunneled through
    fixture.Reset();
    for (int x = 50; x <= 70; ++x) fixture.world.SetCell(x, 50, stone); // floor
    fixture.world.SetCell(58, 49, stone); // left wall at dist 6
    fixture.world.SetCell(66, 49, stone); // right wall at dist 2!
    fixture.world.SetCell(64, 49, water); // water dispersion=5

    for (int t = 0; t < 20; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }
    // Water must remain strictly within [59, 65]
    for (int x = 0; x < 128; ++x) {
        if (x < 59 || x > 65) {
            ASSERT_NE(fixture.world.GetCellMaterial(x, 49), water);
        }
    }
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 1u);
}

// 2.3 Cliff drop-off priority and floor hole detection
E2E_TEST(AdversarialLiquid, CliffDropOffAndHoleDetection, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water");
    MaterialId stone = fixture.GetMatId("stone");

    // Platform from x = 20 to x = 30 at y = 50. Left wall at x = 19, y = 49.
    // Ledge cliff at x = 31 (air below at y = 50).
    fixture.world.SetCell(19, 49, stone);
    for (int x = 20; x <= 30; ++x) {
        fixture.world.SetCell(x, 50, stone);
    }

    // Place water at x = 28, y = 49 (3 steps from cliff)
    fixture.world.SetCell(28, 49, water);

    // Step 1: Water disperses towards right (blocked left by x=19, x=20..27 already air)
    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();

    // Cliff check: Water MUST halt at x = 31 (cliff edge) because belowTargetY has air!
    // Water MUST NOT float to x = 32 or x = 33!
    ASSERT_EQ(fixture.world.GetCellMaterial(31, 49), water);
    ASSERT_EQ(fixture.world.GetCellMaterial(32, 49), EMPTY_MATERIAL_ID);
    ASSERT_EQ(fixture.world.GetCellMaterial(33, 49), EMPTY_MATERIAL_ID);

    // Step 2: Water falls down off cliff to (31, 50)
    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();
    ASSERT_EQ(fixture.world.GetCellMaterial(31, 49), EMPTY_MATERIAL_ID);
    ASSERT_EQ(fixture.world.GetCellMaterial(31, 50), water);

    // Hole detection in floor:
    // Platform at y = 60 from x = 10 to x = 40, with a 1-cell hole at x = 25!
    fixture.Reset();
    fixture.world.SetCell(19, 59, stone); // Left wall
    for (int x = 20; x <= 40; ++x) {
        if (x != 25) {
            fixture.world.SetCell(x, 60, stone);
        }
    }
    // Place water at x = 22, y = 59 (dispersion 5 could reach x=27)
    fixture.world.SetCell(22, 59, water);

    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();

    // Water must halt at hole (x = 25, y = 59)
    ASSERT_EQ(fixture.world.GetCellMaterial(25, 59), water);
    ASSERT_EQ(fixture.world.GetCellMaterial(26, 59), EMPTY_MATERIAL_ID);
    ASSERT_EQ(fixture.world.GetCellMaterial(27, 59), EMPTY_MATERIAL_ID);

    // Step 2: Water falls into hole to (25, 60)
    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();
    ASSERT_EQ(fixture.world.GetCellMaterial(25, 60), water);
}

// ============================================================================
// SUITE 3: Symmetric Mutual Density Displacement Stress Tests
// ============================================================================

// 3.1 Sand sinks in Water, Water displaced upward
E2E_TEST(AdversarialDensity, SandSinksInWater, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand = fixture.GetMatId("sand");   // 1600
    MaterialId water = fixture.GetMatId("water"); // 1000
    MaterialId stone = fixture.GetMatId("stone");

    // Stone basin: x in [40, 50], y in [30, 42]
    for (int x = 39; x <= 51; ++x) fixture.world.SetCell(x, 43, stone); // floor
    for (int y = 30; y <= 43; ++y) {
        fixture.world.SetCell(39, y, stone); // left wall
        fixture.world.SetCell(51, y, stone); // right wall
    }

    // Fill bottom with Water: y = 35..42 (8 rows * 11 cols = 88 cells)
    core::u32 initialWater = 0;
    for (int y = 35; y <= 42; ++y) {
        for (int x = 40; x <= 50; ++x) {
            fixture.world.SetCell(x, y, water);
            initialWater++;
        }
    }

    // Drop 11 Sand particles on top of the water: y = 34
    const core::u32 sandCount = 11;
    for (int x = 40; x <= 50; ++x) {
        fixture.world.SetCell(x, 34, sand);
    }

    // Simulate 50 ticks
    for (int t = 0; t < 50; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Sand must have sunk to bottom row y = 42
    for (int x = 40; x <= 50; ++x) {
        ASSERT_EQ(fixture.world.GetCellMaterial(x, 42), sand);
    }

    // Water displaced upward to y = 34
    core::u32 waterAtTop = 0;
    for (int x = 40; x <= 50; ++x) {
        if (fixture.world.GetCellMaterial(x, 34) == water) {
            waterAtTop++;
        }
    }
    ASSERT_EQ(waterAtTop, 11u);

    // Strict conservation
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), sandCount);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), initialWater);
}

// 3.2 Water sinks in Oil, Oil floats on Water
E2E_TEST(AdversarialDensity, WaterSinksInOilAndOilFloats, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water"); // 1000
    MaterialId oil   = fixture.GetMatId("oil");   // 800
    MaterialId stone = fixture.GetMatId("stone");

    // Stone tube: x in [50, 53], y in [40, 55]
    for (int x = 49; x <= 54; ++x) fixture.world.SetCell(x, 56, stone);
    for (int y = 40; y <= 56; ++y) {
        fixture.world.SetCell(49, y, stone);
        fixture.world.SetCell(54, y, stone);
    }

    // Bottom half filled with Oil (y = 48..55, 8 rows * 4 cols = 32 cells)
    for (int y = 48; y <= 55; ++y) {
        for (int x = 50; x <= 53; ++x) {
            fixture.world.SetCell(x, y, oil);
        }
    }
    // Top half filled with Water (y = 40..47, 8 rows * 4 cols = 32 cells)
    for (int y = 40; y <= 47; ++y) {
        for (int x = 50; x <= 53; ++x) {
            fixture.world.SetCell(x, y, water);
        }
    }

    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 32u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, oil), 32u);

    // Step 250 ticks to allow full mutual displacement
    for (int t = 0; t < 250; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Bottom half MUST now be Water (y = 48..55)
    for (int y = 48; y <= 55; ++y) {
        for (int x = 50; x <= 53; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), water);
        }
    }

    // Top half MUST now be Oil (y = 40..47)
    for (int y = 40; y <= 47; ++y) {
        for (int x = 50; x <= 53; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), oil);
        }
    }

    // Conservation check
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 32u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, oil), 32u);
}

// 3.3 Four-Liquid Multi-Layer Stratification Column
E2E_TEST(AdversarialDensity, FourLiquidStratificationColumn, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId lava  = fixture.GetMatId("lava");  // 3000
    MaterialId acid  = fixture.GetMatId("acid");  // 1200
    MaterialId water = fixture.GetMatId("water"); // 1000
    MaterialId oil   = fixture.GetMatId("oil");   // 800
    MaterialId stone = fixture.GetMatId("stone");

    // Vertical tube: x in [60, 63], floor at y = 70, walls at x=59 and x=64
    for (int x = 59; x <= 64; ++x) fixture.world.SetCell(x, 70, stone);
    for (int y = 25; y <= 70; ++y) {
        fixture.world.SetCell(59, y, stone);
        fixture.world.SetCell(64, y, stone);
    }

    // Place inverted stack (lightest on bottom, heaviest on top):
    // y = 60..69 (10 rows): Oil (800)
    // y = 50..59 (10 rows): Water (1000)
    // y = 40..49 (10 rows): Acid (1200)
    // y = 30..39 (10 rows): Lava (3000)
    for (int y = 60; y <= 69; ++y) {
        for (int x = 60; x <= 63; ++x) fixture.world.SetCell(x, y, oil);
    }
    for (int y = 50; y <= 59; ++y) {
        for (int x = 60; x <= 63; ++x) fixture.world.SetCell(x, y, water);
    }
    for (int y = 40; y <= 49; ++y) {
        for (int x = 60; x <= 63; ++x) fixture.world.SetCell(x, y, acid);
    }
    for (int y = 30; y <= 39; ++y) {
        for (int x = 60; x <= 63; ++x) fixture.world.SetCell(x, y, lava);
    }

    ASSERT_EQ(inspection::CountMaterial(fixture.world, oil), 40u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 40u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, acid), 40u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, lava), 40u);

    // Simulate 500 ticks for complete sorting
    for (int t = 0; t < 500; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Verify stratified equilibrium:
    // Bottom: Lava (y = 60..69)
    for (int y = 60; y <= 69; ++y) {
        for (int x = 60; x <= 63; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), lava);
        }
    }
    // Lower-mid: Acid (y = 50..59)
    for (int y = 50; y <= 59; ++y) {
        for (int x = 60; x <= 63; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), acid);
        }
    }
    // Upper-mid: Water (y = 40..49)
    for (int y = 40; y <= 49; ++y) {
        for (int x = 60; x <= 63; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), water);
        }
    }
    // Top: Oil (y = 30..39)
    for (int y = 30; y <= 39; ++y) {
        for (int x = 60; x <= 63; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), oil);
        }
    }

    // Total conservation
    ASSERT_EQ(inspection::CountMaterial(fixture.world, oil), 40u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), 40u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, acid), 40u);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, lava), 40u);
}

// 3.4 Ash floats on Water and Oil without sinking
E2E_TEST(AdversarialDensity, AshFloatsOnWaterAndOil, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId ash   = fixture.GetMatId("ash");   // 500 powder
    MaterialId water = fixture.GetMatId("water"); // 1000 liquid
    MaterialId oil   = fixture.GetMatId("oil");   // 800 liquid
    MaterialId stone = fixture.GetMatId("stone");

    // Container 1: Ash on Water
    for (int x = 20; x <= 30; ++x) fixture.world.SetCell(x, 60, stone);
    for (int y = 50; y <= 60; ++y) {
        fixture.world.SetCell(20, y, stone);
        fixture.world.SetCell(30, y, stone);
    }
    for (int y = 55; y <= 59; ++y) {
        for (int x = 21; x <= 29; ++x) fixture.world.SetCell(x, y, water);
    }
    for (int x = 21; x <= 29; ++x) {
        fixture.world.SetCell(x, 54, ash);
    }

    // Container 2: Ash on Oil
    for (int x = 40; x <= 50; ++x) fixture.world.SetCell(x, 60, stone);
    for (int y = 50; y <= 60; ++y) {
        fixture.world.SetCell(40, y, stone);
        fixture.world.SetCell(50, y, stone);
    }
    for (int y = 55; y <= 59; ++y) {
        for (int x = 41; x <= 49; ++x) fixture.world.SetCell(x, y, oil);
    }
    for (int x = 41; x <= 49; ++x) {
        fixture.world.SetCell(x, 54, ash);
    }

    // Step 100 ticks
    for (int t = 0; t < 100; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Ash must remain at y = 54, NEVER sinking into water (y >= 55)
    for (int x = 21; x <= 29; ++x) {
        ASSERT_EQ(fixture.world.GetCellMaterial(x, 54), ash);
    }
    for (int y = 55; y <= 59; ++y) {
        for (int x = 21; x <= 29; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), water);
        }
    }

    // Ash must remain at y = 54, NEVER sinking into oil (y >= 55)
    for (int x = 41; x <= 49; ++x) {
        ASSERT_EQ(fixture.world.GetCellMaterial(x, 54), ash);
    }
    for (int y = 55; y <= 59; ++y) {
        for (int x = 41; x <= 49; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), oil);
        }
    }
}

// 3.5 Absence of artificial churning or perpetual jittering in resting fluid
E2E_TEST(AdversarialDensity, NoArtificialChurningInRestingFluid, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water");
    MaterialId stone = fixture.GetMatId("stone");

    // Container 10x10 of water
    for (int x = 40; x <= 52; ++x) fixture.world.SetCell(x, 60, stone);
    for (int y = 45; y <= 60; ++y) {
        fixture.world.SetCell(40, y, stone);
        fixture.world.SetCell(52, y, stone);
    }
    for (int y = 48; y <= 59; ++y) {
        for (int x = 41; x <= 51; ++x) {
            fixture.world.SetCell(x, y, water);
        }
    }

    // Settle for 100 ticks
    for (int t = 0; t < 100; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Snapshot world state
    std::vector<MaterialId> snapshot(128 * 128);
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            snapshot[y * 128 + x] = fixture.world.GetCellMaterial(x, y);
        }
    }

    // Advance 500 more ticks: resting identical density cells must NOT churn
    for (int t = 0; t < 500; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Compare with snapshot: must be 100% identical!
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            ASSERT_EQ(fixture.world.GetCellMaterial(x, y), snapshot[y * 128 + x]);
        }
    }
}

// ============================================================================
// SUITE 4: Diagonal Wall Tunneling Prevention Stress Tests
// ============================================================================

// 4.1 Powder corner tunneling prevention (both orientations)
E2E_TEST(AdversarialWalls, DiagonalCornerTunnelingPowder, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand  = fixture.GetMatId("sand");
    MaterialId stone = fixture.GetMatId("stone");

    // Configuration 1: Anti-diagonal corner
    // Sand at (20, 20), Stone at (20, 21) and (21, 20), Empty at (21, 21)
    // Left/Top blocked to force movement attempt
    fixture.world.SetCell(19, 20, stone);
    fixture.world.SetCell(19, 21, stone);
    fixture.world.SetCell(20, 20, sand);
    fixture.world.SetCell(20, 21, stone);
    fixture.world.SetCell(21, 20, stone);

    // Configuration 2: Main diagonal corner
    // Sand at (40, 20), Stone at (40, 21) and (39, 20), Empty at (39, 21)
    // Right/Top blocked
    fixture.world.SetCell(41, 20, stone);
    fixture.world.SetCell(41, 21, stone);
    fixture.world.SetCell(40, 20, sand);
    fixture.world.SetCell(40, 21, stone);
    fixture.world.SetCell(39, 20, stone);

    for (int t = 0; t < 100; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Config 1 check: Sand MUST stay at (20, 20), MUST NOT leak to (21, 21)
    ASSERT_EQ(fixture.world.GetCellMaterial(20, 20), sand);
    ASSERT_EQ(fixture.world.GetCellMaterial(21, 21), EMPTY_MATERIAL_ID);

    // Config 2 check: Sand MUST stay at (40, 20), MUST NOT leak to (39, 21)
    ASSERT_EQ(fixture.world.GetCellMaterial(40, 20), sand);
    ASSERT_EQ(fixture.world.GetCellMaterial(39, 21), EMPTY_MATERIAL_ID);
}

// 4.2 Liquid corner tunneling prevention (both orientations)
E2E_TEST(AdversarialWalls, DiagonalCornerTunnelingLiquid, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId water = fixture.GetMatId("water");
    MaterialId stone = fixture.GetMatId("stone");

    // Configuration 1: Anti-diagonal
    fixture.world.SetCell(19, 20, stone);
    fixture.world.SetCell(19, 21, stone);
    fixture.world.SetCell(20, 20, water);
    fixture.world.SetCell(20, 21, stone);
    fixture.world.SetCell(21, 20, stone);

    // Configuration 2: Main diagonal
    fixture.world.SetCell(41, 20, stone);
    fixture.world.SetCell(41, 21, stone);
    fixture.world.SetCell(40, 20, water);
    fixture.world.SetCell(40, 21, stone);
    fixture.world.SetCell(39, 20, stone);

    for (int t = 0; t < 100; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Water must NOT leak through diagonal corner
    ASSERT_EQ(fixture.world.GetCellMaterial(20, 20), water);
    ASSERT_EQ(fixture.world.GetCellMaterial(21, 21), EMPTY_MATERIAL_ID);

    ASSERT_EQ(fixture.world.GetCellMaterial(40, 20), water);
    ASSERT_EQ(fixture.world.GetCellMaterial(39, 21), EMPTY_MATERIAL_ID);
}

// 4.3 High-pressure reservoir against 45-degree diagonal solid wall
E2E_TEST(AdversarialWalls, DiagonalWall45DegreeBarrierZeroLeakage, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand  = fixture.GetMatId("sand");
    MaterialId water = fixture.GetMatId("water");
    MaterialId stone = fixture.GetMatId("stone");

    // Continuous diagonal barrier from (30, 30) down-right to (60, 60)
    // To form a solid barrier against diagonal tunneling, each segment has
    // (x, y) and (x, y+1) as solid stone.
    for (int i = 0; i <= 30; ++i) {
        fixture.world.SetCell(30 + i, 30 + i, stone);
        fixture.world.SetCell(30 + i, 31 + i, stone);
    }
    // Vertical left wall at x = 29, y = 10..60
    for (int y = 10; y <= 60; ++y) {
        fixture.world.SetCell(29, y, stone);
    }

    // Fill reservoir above/left of diagonal wall with 100 Sand and 100 Water cells
    core::u32 placedSand = 0;
    core::u32 placedWater = 0;
    for (int y = 15; y <= 24; ++y) {
        for (int x = 30; x <= 39; ++x) {
            fixture.world.SetCell(x, y, sand);
            placedSand++;
        }
    }
    for (int y = 25; y <= 34; ++y) {
        for (int x = 30; x <= 39; ++x) {
            fixture.world.SetCell(x, y, water);
            placedWater++;
        }
    }

    // Step 300 ticks under high pressure
    for (int t = 0; t < 300; ++t) {
        fixture.physics.Step(fixture.world);
        fixture.world.AdvanceTick();
    }

    // Verify ZERO cells leaked across the diagonal barrier to the other side
    // For any x >= 30, y >= 32: if y > x + 1, it is on the other side of the wall
    core::u32 leakedCount = 0;
    for (int x = 30; x <= 65; ++x) {
        for (int y = x + 2; y <= 70; ++y) {
            MaterialId m = fixture.world.GetCellMaterial(x, y);
            if (m == sand || m == water) {
                leakedCount++;
            }
        }
    }
    ASSERT_EQ(leakedCount, 0u);

    // Conservation check: all placed cells still present
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), placedSand);
    ASSERT_EQ(inspection::CountMaterial(fixture.world, water), placedWater);
}

// ============================================================================
// SUITE 5: Multi-Chunk Boundary Dynamics Stress Tests
// ============================================================================

// 5.1 Sand crossing vertical chunk border (63 -> 64) and diagonal chunk corner
E2E_TEST(AdversarialChunkBoundary, VerticalAndDiagonalChunkBorderCrossings, 5) {
    TestWorldFixture fixture(128, 128);
    MaterialId sand  = fixture.GetMatId("sand");
    MaterialId stone = fixture.GetMatId("stone");

    // Vertical crossing: Sand at (30, 63) in chunk (0, 0), drops into (30, 64) in chunk (0, 1)
    fixture.world.SetCell(30, 63, sand);
    Chunk* c00 = fixture.world.GetChunk(0, 0);
    Chunk* c01 = fixture.world.GetChunk(0, 1);
    ASSERT_EQ(c00->activeCellCount, 1u);
    ASSERT_EQ(c01->activeCellCount, 0u);

    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();

    // Now in chunk (0, 1) at (30, 64) -> lx = 30, ly = 0
    ASSERT_EQ(fixture.world.GetCellMaterial(30, 64), sand);
    ASSERT_EQ(c00->activeCellCount, 0u);
    ASSERT_EQ(c01->activeCellCount, 1u);
    ASSERT_EQ(c01->minActiveY, 0);
    ASSERT_EQ(c01->maxActiveY, 0);

    // Next step: must continue falling down to (30, 65) without freezing
    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();
    ASSERT_EQ(fixture.world.GetCellMaterial(30, 65), sand);
    ASSERT_GE(c01->maxActiveY, 1);

    // Diagonal crossing across chunk corner: from Chunk(0, 0) to Chunk(1, 1)
    // Sand at (63, 63) (bottom-right of Chunk 0,0)
    // Obstacle at (63, 64) (top-right of Chunk 0,1)
    // Destination: (64, 64) (top-left of Chunk 1,1)
    fixture.Reset();
    Chunk* c11 = fixture.world.GetChunk(1, 1);
    ASSERT_TRUE(c11 != nullptr);
    fixture.world.SetCell(63, 63, sand);
    fixture.world.SetCell(63, 64, stone); // Block straight down

    fixture.physics.Step(fixture.world);
    fixture.world.AdvanceTick();

    // Sand should slide diagonally to (64, 64) in Chunk(1, 1) or (62, 64) in Chunk(0, 1)
    MaterialId dRight = fixture.world.GetCellMaterial(64, 64);
    MaterialId dLeft  = fixture.world.GetCellMaterial(62, 64);
    ASSERT_TRUE((dRight == sand && dLeft == EMPTY_MATERIAL_ID) ||
                (dLeft == sand && dRight == EMPTY_MATERIAL_ID));

    if (dRight == sand) {
        ASSERT_EQ(c11->activeCellCount, 1u);
        ASSERT_EQ(c11->minActiveY, 0);
    }

    // Total sand count preserved
    ASSERT_EQ(inspection::CountMaterial(fixture.world, sand), 1u);
}
