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
#include "ui/Brush.hpp"
#include "ui/Renderer.hpp"

using namespace sandbox;
using namespace sandbox::sim;
using namespace sandbox::ui;

void TestChunkLayout() {
    std::cout << "[TEST] 1. Chunk Memory Layout & Cache Alignment..." << std::endl;

    static_assert(alignof(Chunk) == 64, "Chunk must be 64-byte aligned");
    static_assert(sizeof(Chunk::material) == 8192, "Material array must be 8192 bytes");
    static_assert(sizeof(Chunk::temperature) == 16384, "Temperature array must be 16384 bytes");
    static_assert(sizeof(Chunk::flags) == 4096, "Flags array must be 4096 bytes");
    static_assert(sizeof(Chunk::lifetime) == 4096, "Lifetime array must be 4096 bytes");

    Chunk chunk(0, 0);
    TEST_ASSERT(reinterpret_cast<uintptr_t>(&chunk) % 64 == 0);
    TEST_ASSERT(reinterpret_cast<uintptr_t>(chunk.material) % 64 == 0);
    TEST_ASSERT(reinterpret_cast<uintptr_t>(chunk.temperature) % 64 == 0);
    TEST_ASSERT(reinterpret_cast<uintptr_t>(chunk.flags) % 64 == 0);
    TEST_ASSERT(reinterpret_cast<uintptr_t>(chunk.lifetime) % 64 == 0);

    static_assert(sizeof(Chunk) >= 32768 && sizeof(Chunk) <= 33000, "Chunk size must be ~32KB");
    std::cout << "  -> Chunk size: " << sizeof(Chunk) << " bytes (aligned 64 bytes). PASSED." << std::endl;
}

void TestMaterialRegistry() {
    std::cout << "[TEST] 2. MaterialRegistry Loading & Phase Resolution..." << std::endl;

    auto& registry = MaterialRegistry::Instance();
    bool loaded = registry.LoadFromDirectory("assets/materials");
    if (!loaded) {
        loaded = registry.LoadFromDirectory("../assets/materials");
    }
    TEST_ASSERT(loaded && "Failed to load materials from assets/materials");
    TEST_ASSERT(registry.Count() >= 18 && "Registry must contain at least 18 core materials");

    // Check specific IDs and names
    TEST_ASSERT(registry.Get(0).name == "air");
    TEST_ASSERT(registry.FindByName("stone") == 1);
    TEST_ASSERT(registry.FindByName("dirt") == 2);
    TEST_ASSERT(registry.FindByName("sand") == 3);
    TEST_ASSERT(registry.FindByName("water") == 4);
    TEST_ASSERT(registry.FindByName("ice") == 5);
    TEST_ASSERT(registry.FindByName("steam") == 6);
    TEST_ASSERT(registry.FindByName("oil") == 7);
    TEST_ASSERT(registry.FindByName("acid") == 8);
    TEST_ASSERT(registry.FindByName("lava") == 9);
    TEST_ASSERT(registry.FindByName("fire") == 10);
    TEST_ASSERT(registry.FindByName("smoke") == 11);
    TEST_ASSERT(registry.FindByName("wood") == 12);
    TEST_ASSERT(registry.FindByName("iron") == 13);
    TEST_ASSERT(registry.FindByName("gunpowder") == 14);
    TEST_ASSERT(registry.FindByName("tnt") == 15);
    TEST_ASSERT(registry.FindByName("glass") == 16);
    TEST_ASSERT(registry.FindByName("ash") == 17);

    // Verify two-pass phase targets
    MaterialId waterId = registry.FindByName("water");
    MaterialId steamId = registry.FindByName("steam");
    MaterialId iceId = registry.FindByName("ice");
    MaterialId stoneId = registry.FindByName("stone");
    MaterialId lavaId = registry.FindByName("lava");
    MaterialId sandId = registry.FindByName("sand");
    MaterialId glassId = registry.FindByName("glass");

    TEST_ASSERT(registry.Get(waterId).boilMaterialId == steamId);
    TEST_ASSERT(registry.Get(waterId).freezeMaterialId == iceId);
    TEST_ASSERT(registry.Get(stoneId).meltMaterialId == lavaId);
    TEST_ASSERT(registry.Get(sandId).meltMaterialId == glassId);
    TEST_ASSERT(registry.Get(steamId).freezeMaterialId == waterId);
    TEST_ASSERT(registry.Get(iceId).meltMaterialId == waterId);

    std::cout << "  -> Loaded " << registry.Count() << " materials. Phase targets verified. PASSED." << std::endl;
}

void TestWorldCoordinates() {
    std::cout << "[TEST] 3. World Coordinate Transformations..." << std::endl;

    World world(1280, 768);
    TEST_ASSERT(world.GetChunksX() == 20);
    TEST_ASSERT(world.GetChunksY() == 12);
    TEST_ASSERT(world.GetChunkCount() == 240);

    for (int y = 0; y < 768; y += 31) {
        for (int x = 0; x < 1280; x += 47) {
            int cx = World::WorldToChunkX(x);
            int cy = World::WorldToChunkY(y);
            int lx = World::WorldToLocalX(x);
            int ly = World::WorldToLocalY(y);

            TEST_ASSERT(cx >= 0 && cx < 20);
            TEST_ASSERT(cy >= 0 && cy < 12);
            TEST_ASSERT(lx >= 0 && lx < 64);
            TEST_ASSERT(ly >= 0 && ly < 64);

            int rx = World::LocalToWorldX(cx, lx);
            int ry = World::LocalToWorldY(cy, ly);
            TEST_ASSERT(rx == x);
            TEST_ASSERT(ry == y);

            int idx = Chunk::Index(lx, ly);
            int decLx = 0, decLy = 0;
            Chunk::FromIndex(idx, decLx, decLy);
            TEST_ASSERT(decLx == lx && decLy == ly);
        }
    }

    std::cout << "  -> Coordinate round-trips verified. PASSED." << std::endl;
}

void TestPowderPhysics() {
    std::cout << "[TEST] 4. Powder Cellular Automata (Gravity & Repose)..." << std::endl;

    World world(1280, 768);
    Physics physics;
    auto& reg = MaterialRegistry::Instance();
    MaterialId sand = reg.FindByName("sand");
    MaterialId stone = reg.FindByName("stone");

    // 1. Vertical fall
    world.SetCell(10, 10, sand);
    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(10, 10) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(10, 11) == sand);

    // 2. Diagonal slide off obstacle
    world.Clear();
    world.SetCell(20, 20, sand);
    world.SetCell(20, 21, stone); // Block straight down

    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(20, 20) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(20, 21) == stone);
    MaterialId leftDiag = world.GetCellMaterial(19, 21);
    MaterialId rightDiag = world.GetCellMaterial(21, 21);
    TEST_ASSERT((leftDiag == sand && rightDiag == EMPTY_MATERIAL_ID) ||
                (rightDiag == sand && leftDiag == EMPTY_MATERIAL_ID));

    std::cout << "  -> Powder gravity and diagonal slide verified. PASSED." << std::endl;
}

void TestLiquidPhysics() {
    std::cout << "[TEST] 5. Liquid Lateral Dispersion & Cliff Drop-off..." << std::endl;

    World world(1280, 768);
    Physics physics;
    auto& reg = MaterialRegistry::Instance();
    MaterialId water = reg.FindByName("water");
    MaterialId stone = reg.FindByName("stone");

    // 1. Liquid downward fall
    world.SetCell(50, 50, water);
    physics.Step(world);
    world.AdvanceTick();
    TEST_ASSERT(world.GetCellMaterial(50, 50) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(50, 51) == water);

    // 2. Lateral dispersion on flat ground
    world.Clear();
    // Build flat floor: y = 100 from x = 40 to 60
    for (int x = 40; x <= 60; ++x) {
        world.SetCell(x, 100, stone);
    }
    // Place single water cell on the floor
    world.SetCell(50, 99, water);
    physics.Step(world);
    world.AdvanceTick();

    // Water must have moved horizontally (either left or right) along y = 99
    TEST_ASSERT(world.GetCellMaterial(50, 99) == EMPTY_MATERIAL_ID);
    bool foundWater = false;
    for (int x = 45; x <= 55; ++x) {
        if (x != 50 && world.GetCellMaterial(x, 99) == water) {
            foundWater = true;
            break;
        }
    }
    TEST_ASSERT(foundWater && "Water must disperse laterally on flat ground");

    // 3. Cliff drop-off priority
    world.Clear();
    // Build a ledge: stone at x=10..12, y=50. Left wall at x=9, y=49. Air at x=13, y=50 (cliff).
    world.SetCell(9, 49, stone);
    world.SetCell(10, 50, stone);
    world.SetCell(11, 50, stone);
    world.SetCell(12, 50, stone);

    world.SetCell(10, 49, water); // Water placed at x=10, y=49
    physics.Step(world);
    world.AdvanceTick();

    // Water must stop exactly at cliff edge (x=13, y=49)
    TEST_ASSERT(world.GetCellMaterial(10, 49) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(13, 49) == water);

    // Next step: water should fall down off the cliff to y=50
    physics.Step(world);
    world.AdvanceTick();
    TEST_ASSERT(world.GetCellMaterial(13, 49) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(13, 50) == water);

    std::cout << "  -> Liquid lateral dispersion & cliff drop-off verified. PASSED." << std::endl;
}

void TestMutualDensityDisplacement() {
    std::cout << "[TEST] 6. Symmetric Mutual Density Displacement..." << std::endl;

    World world(1280, 768);
    Physics physics;
    auto& reg = MaterialRegistry::Instance();
    MaterialId stone = reg.FindByName("stone");
    MaterialId sand = reg.FindByName("sand");     // 1600 kg/m3
    MaterialId water = reg.FindByName("water");   // 1000 kg/m3
    MaterialId oil = reg.FindByName("oil");       // 800 kg/m3
    MaterialId ash = reg.FindByName("ash");       // 500 kg/m3
    MaterialId lava = reg.FindByName("lava");     // 3000 kg/m3

    // 1. Sand sinks into Water (Sand 1600 > Water 1000) inside container
    world.Clear();
    world.SetCell(9, 20, stone);
    world.SetCell(11, 20, stone);
    world.SetCell(10, 21, stone);
    world.SetCell(10, 20, water);
    world.SetCell(10, 19, sand);
    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(10, 20) == sand && "Sand must sink into Water");
    TEST_ASSERT(world.GetCellMaterial(10, 19) == water && "Water must be displaced upward by Sand");

    // 2. Water sinks into Oil (Water 1000 > Oil 800) inside container
    world.Clear();
    world.SetCell(19, 20, stone);
    world.SetCell(21, 20, stone);
    world.SetCell(20, 21, stone);
    world.SetCell(20, 20, oil);
    world.SetCell(20, 19, water);
    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(20, 20) == water && "Water must sink into Oil");
    TEST_ASSERT(world.GetCellMaterial(20, 19) == oil && "Oil must be displaced upward by Water");

    // 3. Ash floats on Water (Ash 500 < Water 1000) inside container
    world.Clear();
    world.SetCell(29, 20, stone);
    world.SetCell(31, 20, stone);
    world.SetCell(30, 21, stone);
    world.SetCell(30, 20, water);
    world.SetCell(30, 19, ash);
    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(30, 19) == ash && "Ash must float on Water");
    TEST_ASSERT(world.GetCellMaterial(30, 20) == water && "Water must not be displaced downward by lighter Ash");

    // 4. Sand floats on Lava (Sand 1600 < Lava 3000) inside container
    world.Clear();
    world.SetCell(39, 20, stone);
    world.SetCell(41, 20, stone);
    world.SetCell(40, 21, stone);
    world.SetCell(40, 20, lava);
    world.SetCell(40, 19, sand);
    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(40, 19) == sand && "Sand must float on Lava");
    TEST_ASSERT(world.GetCellMaterial(40, 20) == lava && "Lava must not be displaced by lighter Sand");

    // 5. Lava sinks into Water (Lava 3000 > Water 1000) inside container
    world.Clear();
    world.SetCell(49, 20, stone);
    world.SetCell(51, 20, stone);
    world.SetCell(50, 21, stone);
    world.SetCell(50, 20, water);
    world.SetCell(50, 19, lava);
    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(50, 20) == lava && "Lava must sink into Water");
    TEST_ASSERT(world.GetCellMaterial(50, 19) == water && "Water must be displaced upward by Lava");

    std::cout << "  -> Mutual density displacement verified. PASSED." << std::endl;
}

void TestDiagonalWallProtection() {
    std::cout << "[TEST] 7. Diagonal Wall Tunneling Prevention..." << std::endl;

    World world(1280, 768);
    Physics physics;
    auto& reg = MaterialRegistry::Instance();
    MaterialId sand = reg.FindByName("sand");
    MaterialId stone = reg.FindByName("stone");

    // Diagonal corner configuration:
    // S  .
    // #  #
    // Where sand is at (10, 10), stone at (10, 11) and (11, 10), empty at (11, 11).
    // Sand must NOT leak diagonally into (11, 11) through the corner of two solid stones!
    world.SetCell(10, 10, sand);
    world.SetCell(10, 11, stone);
    world.SetCell(11, 10, stone);
    world.SetCell(9, 10, stone); // block left as well
    world.SetCell(9, 11, stone);

    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(10, 10) == sand && "Sand must not tunnel through solid diagonal corner");
    TEST_ASSERT(world.GetCellMaterial(11, 11) == EMPTY_MATERIAL_ID);

    std::cout << "  -> Diagonal wall tunneling prevention verified. PASSED." << std::endl;
}

void TestBrushBresenham() {
    std::cout << "[TEST] 8. Brush Bresenham Stroke Continuity & Programmatic API..." << std::endl;

    World world(1280, 768);
    Brush brush;
    auto& reg = MaterialRegistry::Instance();
    MaterialId sand = reg.FindByName("sand");
    TEST_ASSERT(sand != EMPTY_MATERIAL_ID && "Sand material must exist in registry");

    // 8.1 Property accessors
    brush.SetRadius(1);
    TEST_ASSERT(brush.GetRadius() == 1);
    brush.SetMaterial(sand);
    TEST_ASSERT(brush.GetMaterial() == sand);
    TEST_ASSERT(!brush.IsErasing());

    // =========================================================================
    // 8.2 Horizontal Stroke: (10, 50) -> (30, 50) (21 points)
    // =========================================================================
    world.Clear();
    brush.PaintStroke(world, 10, 50, 30, 50);

    for (int x = 10; x <= 30; ++x) {
        TEST_ASSERT(world.GetCellMaterial(x, 50) == sand && "Horizontal stroke must have no gaps");
    }
    TEST_ASSERT(world.GetCellMaterial(9, 50) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(31, 50) == EMPTY_MATERIAL_ID);
    for (int x = 10; x <= 30; ++x) {
        TEST_ASSERT(world.GetCellMaterial(x, 49) == EMPTY_MATERIAL_ID);
        TEST_ASSERT(world.GetCellMaterial(x, 51) == EMPTY_MATERIAL_ID);
    }
    int countH = 0;
    for (int y = 45; y <= 55; ++y) {
        for (int x = 5; x <= 35; ++x) {
            if (world.GetCellMaterial(x, y) == sand) countH++;
        }
    }
    TEST_ASSERT(countH == 21 && "Horizontal stroke length 20 must paint exactly 21 cells");
    TEST_ASSERT(world.GetTotalActiveCells() == 21);

    // =========================================================================
    // 8.3 Vertical Stroke: (50, 10) -> (50, 30) (21 points)
    // =========================================================================
    world.Clear();
    brush.PaintStroke(world, 50, 10, 50, 30);

    for (int y = 10; y <= 30; ++y) {
        TEST_ASSERT(world.GetCellMaterial(50, y) == sand && "Vertical stroke must have no gaps");
    }
    TEST_ASSERT(world.GetCellMaterial(50, 9) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(50, 31) == EMPTY_MATERIAL_ID);
    for (int y = 10; y <= 30; ++y) {
        TEST_ASSERT(world.GetCellMaterial(49, y) == EMPTY_MATERIAL_ID);
        TEST_ASSERT(world.GetCellMaterial(51, y) == EMPTY_MATERIAL_ID);
    }
    int countV = 0;
    for (int y = 5; y <= 35; ++y) {
        for (int x = 45; x <= 55; ++x) {
            if (world.GetCellMaterial(x, y) == sand) countV++;
        }
    }
    TEST_ASSERT(countV == 21 && "Vertical stroke length 20 must paint exactly 21 cells");
    TEST_ASSERT(world.GetTotalActiveCells() == 21);

    // =========================================================================
    // 8.4 Diagonal Stroke: (10, 10) -> (30, 30) (21 points)
    // =========================================================================
    world.Clear();
    brush.PaintStroke(world, 10, 10, 30, 30);

    for (int i = 0; i <= 20; ++i) {
        TEST_ASSERT(world.GetCellMaterial(10 + i, 10 + i) == sand && "Diagonal stroke must be continuous");
    }
    TEST_ASSERT(world.GetCellMaterial(9, 9) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(31, 31) == EMPTY_MATERIAL_ID);
    int countD = 0;
    for (int y = 5; y <= 35; ++y) {
        for (int x = 5; x <= 35; ++x) {
            if (world.GetCellMaterial(x, y) == sand) countD++;
        }
    }
    TEST_ASSERT(countD == 21 && "Diagonal stroke slope=1 must paint exactly 21 cells");
    TEST_ASSERT(world.GetTotalActiveCells() == 21);

    // =========================================================================
    // 8.5 Steep Diagonal Stroke: (10, 10) -> (20, 40) (31 points, zero 8-way gaps)
    // =========================================================================
    world.Clear();
    brush.PaintStroke(world, 10, 10, 20, 40);

    int countSteep = 0;
    std::vector<core::Point2i> steepPoints;
    for (int y = 5; y <= 45; ++y) {
        for (int x = 5; x <= 25; ++x) {
            if (world.GetCellMaterial(x, y) == sand) {
                countSteep++;
                steepPoints.push_back({x, y});
            }
        }
    }
    TEST_ASSERT(countSteep == 31 && "Steep stroke (dy=30) must paint exactly 31 cells");
    TEST_ASSERT(world.GetTotalActiveCells() == 31);

    // (a) Every row y from 10 to 40 must have at least one painted pixel
    for (int y = 10; y <= 40; ++y) {
        bool rowHasCell = false;
        for (int x = 10; x <= 20; ++x) {
            if (world.GetCellMaterial(x, y) == sand) {
                rowHasCell = true;
                break;
            }
        }
        TEST_ASSERT(rowHasCell && "Every row along steep stroke must have a painted cell");
    }
    // (b) 8-way connectivity verification: consecutive sorted points have max(|dx|, |dy|) <= 1
    std::sort(steepPoints.begin(), steepPoints.end(), [](const core::Point2i& a, const core::Point2i& b) {
        return (a.y == b.y) ? (a.x < b.x) : (a.y < b.y);
    });
    TEST_ASSERT(steepPoints.front().x == 10 && steepPoints.front().y == 10);
    TEST_ASSERT(steepPoints.back().x == 20 && steepPoints.back().y == 40);
    for (size_t i = 1; i < steepPoints.size(); ++i) {
        int dX = std::abs(steepPoints[i].x - steepPoints[i - 1].x);
        int dY = std::abs(steepPoints[i].y - steepPoints[i - 1].y);
        TEST_ASSERT(std::max(dX, dY) <= 1 && "Steep line points must be 8-connected with zero gaps");
    }

    // =========================================================================
    // 8.6 Negative Direction Stroke: (40, 40) -> (20, 20) (21 points)
    // =========================================================================
    world.Clear();
    brush.PaintStroke(world, 40, 40, 20, 20);

    for (int i = 0; i <= 20; ++i) {
        TEST_ASSERT(world.GetCellMaterial(40 - i, 40 - i) == sand && "Negative direction stroke must be continuous");
    }
    TEST_ASSERT(world.GetCellMaterial(41, 41) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(19, 19) == EMPTY_MATERIAL_ID);
    int countNeg = 0;
    for (int y = 15; y <= 45; ++y) {
        for (int x = 15; x <= 45; ++x) {
            if (world.GetCellMaterial(x, y) == sand) countNeg++;
        }
    }
    TEST_ASSERT(countNeg == 21 && "Negative direction diagonal stroke must paint exactly 21 cells");
    TEST_ASSERT(world.GetTotalActiveCells() == 21);

    // =========================================================================
    // 8.7 Radius Scaling: R=3 Disk (29 cells, diameter 7) & Endpoint Cap
    // =========================================================================
    world.Clear();
    brush.SetRadius(3);
    brush.PaintDisk(world, 100, 100);

    int diskCount = 0;
    int minX = 9999, maxX = -1, minY = 9999, maxY = -1;
    for (int dy = -5; dy <= 5; ++dy) {
        for (int dx = -5; dx <= 5; ++dx) {
            int cx = 100 + dx;
            int cy = 100 + dy;
            bool insideDisk = (dx * dx + dy * dy <= 9); // R=3 => R^2=9
            MaterialId cellMat = world.GetCellMaterial(cx, cy);

            if (insideDisk) {
                TEST_ASSERT(cellMat == sand && "Cell inside Euclidean disk R=3 must be filled");
                diskCount++;
                if (cx < minX) minX = cx;
                if (cx > maxX) maxX = cx;
                if (cy < minY) minY = cy;
                if (cy > maxY) maxY = cy;
            } else {
                TEST_ASSERT(cellMat == EMPTY_MATERIAL_ID && "Cell outside Euclidean disk R=3 must be empty");
            }
        }
    }
    TEST_ASSERT(diskCount == 29 && "Disk of radius 3 must contain exactly 29 filled cells");
    TEST_ASSERT(maxX - minX + 1 == 7 && "Disk horizontal diameter must be exactly 7 (97 to 103)");
    TEST_ASSERT(maxY - minY + 1 == 7 && "Disk vertical diameter must be exactly 7 (97 to 103)");

    // Stroke endpoint check with R=3: (80, 100) -> (100, 100)
    world.Clear();
    brush.PaintStroke(world, 80, 100, 100, 100);
    TEST_ASSERT(world.GetCellMaterial(103, 100) == sand);
    TEST_ASSERT(world.GetCellMaterial(104, 100) == EMPTY_MATERIAL_ID);
    for (int y = 97; y <= 103; ++y) {
        TEST_ASSERT(world.GetCellMaterial(100, y) == sand && "Stroke endpoint cap must have full diameter 7");
    }
    TEST_ASSERT(world.GetCellMaterial(100, 96) == EMPTY_MATERIAL_ID);
    TEST_ASSERT(world.GetCellMaterial(100, 104) == EMPTY_MATERIAL_ID);

    // =========================================================================
    // 8.8 Programmatic Erase Stroke
    // =========================================================================
    brush.SetErasing(true);
    TEST_ASSERT(brush.IsErasing());
    brush.PaintStroke(world, 80, 100, 100, 100);
    TEST_ASSERT(world.GetTotalActiveCells() == 0 && "Erasing stroke with SetErasing(true) must clear all cells");
    brush.SetErasing(false);
    TEST_ASSERT(!brush.IsErasing());

    // Test erase via EMPTY_MATERIAL_ID
    brush.SetRadius(1);
    brush.PaintStroke(world, 10, 10, 20, 20, sand, 1);
    TEST_ASSERT(world.GetTotalActiveCells() == 11);
    brush.PaintStroke(world, 10, 10, 20, 20, EMPTY_MATERIAL_ID, 1);
    TEST_ASSERT(world.GetTotalActiveCells() == 0 && "Erase stroke with EMPTY_MATERIAL_ID must clear cells");

    std::cout << "  -> Bresenham trajectories (H, V, D, Steep, Neg), continuity, R=3 scaling & erasing verified. PASSED." << std::endl;
}

void TestActiveBoundsAndSwap() {
    std::cout << "[TEST] 9. Chunk Active Bounds Tracking & World::SwapCells..." << std::endl;

    World world(1280, 768);
    Physics physics;
    auto& reg = MaterialRegistry::Instance();
    MaterialId sand = reg.FindByName("sand");
    TEST_ASSERT(sand != EMPTY_MATERIAL_ID);

    // 9.1 Empty chunk initial state has minActiveY = 64, maxActiveY = -1 (inverted)
    Chunk* c00 = world.GetChunk(0, 0);
    TEST_ASSERT(c00 != nullptr);
    TEST_ASSERT(c00->activeCellCount == 0);
    TEST_ASSERT(c00->minActiveY == 64 && c00->maxActiveY == -1 && "Empty chunk must have minActiveY=64, maxActiveY=-1");

    // 9.2 SetCell expands bounds to exact row
    world.SetCell(10, 25, sand);
    TEST_ASSERT(c00->activeCellCount == 1);
    TEST_ASSERT(c00->minActiveY == 25 && c00->maxActiveY == 25);

    // 9.3 Intra-chunk SwapCells via physics step expands maxActiveY
    physics.Step(world);
    world.AdvanceTick();
    TEST_ASSERT(world.GetCellMaterial(10, 26) == sand);
    TEST_ASSERT(c00->minActiveY <= 25 && c00->maxActiveY >= 26 && "Falling within chunk must expand maxActiveY");

    // 9.4 Inter-chunk SwapCells into empty neighbor at ly = 0
    world.Clear();
    Chunk* c01 = world.GetChunk(0, 1);
    TEST_ASSERT(c01 != nullptr);
    // Put sand at bottom-most row of Chunk (0, 0): world y = 63 (lx = 10, ly = 63)
    world.SetCell(10, 63, sand);
    TEST_ASSERT(c00->minActiveY == 63 && c00->maxActiveY == 63);
    TEST_ASSERT(c01->minActiveY == 64 && c01->maxActiveY == -1);

    // Step physics: sand falls from (10, 63) to (10, 64) -> Chunk(0, 1) local ly = 0
    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(10, 64) == sand);
    TEST_ASSERT(c01->activeCellCount == 1);
    TEST_ASSERT(c01->minActiveY == 0 && c01->maxActiveY == 0 && "Destination chunk bounds must encompass row 0");

    // Verify StepChunk on Chunk(0, 1) correctly simulates row 0 down to row 1
    physics.StepChunk(world, *c01);
    world.AdvanceTick();
    TEST_ASSERT(world.GetCellMaterial(10, 65) == sand && "StepChunk must not skip row 0");
    TEST_ASSERT(c01->maxActiveY >= 1);

    // 9.5 Boundary Wake Cascade: Put Chunk(0, 1) to sleep, move particle to border ly = 63
    world.Clear();
    c01->isSleeping = true;
    world.SetCell(10, 62, sand);
    physics.Step(world); // Falls to row 63 (bottom border of Chunk(0, 0))
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(10, 63) == sand);
    TEST_ASSERT(!c01->isSleeping && "Chunk below must be woken when active particle reaches ly = 63");

    std::cout << "  -> Active bounds tracking & World::SwapCells cascades verified. PASSED." << std::endl;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "========================================" << std::endl;
    std::cout << "  PIXEL SANDBOX 2D - MILESTONE 1 TESTS   " << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        TestChunkLayout();
        TestMaterialRegistry();
        TestWorldCoordinates();
        TestPowderPhysics();
        TestLiquidPhysics();
        TestMutualDensityDisplacement();
        TestDiagonalWallProtection();
        TestBrushBresenham();
        TestActiveBoundsAndSwap();

        std::cout << "========================================" << std::endl;
        std::cout << "  ALL MILESTONE 1 TESTS PASSED! (9/9)   " << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
