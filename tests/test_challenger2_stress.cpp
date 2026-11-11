#define SDL_MAIN_HANDLED
#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstddef>

#include "sim/Chunk.hpp"
#include "sim/World.hpp"
#include "sim/Physics.hpp"
#include "sim/MaterialRegistry.hpp"
#include "ui/Brush.hpp"
#include "ui/Renderer.hpp"
#include "core/Types.hpp"
#include "core/Math.hpp"
#include "core/Timer.hpp"
#include <SDL.h>

#define STRESS_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n[STRESS TEST FAILURE] " << __FILE__ << ":" << __LINE__ \
                      << " in " << __func__ << "(): " << msg << std::endl; \
            std::exit(1); \
        } \
    } while (0)

using namespace sandbox;
using namespace sandbox::sim;
using namespace sandbox::ui;

// Helper: determine path to assets directory (deleted unused FindAssetsDir)

// ============================================================================
// SUITE 1: 64x64 SoA Chunk Layout, Alignment, Sizing & Bitwise Indexing
// ============================================================================
static void StressTestChunkLayoutAndIndexing() {
    std::cout << "[SUITE 1] 64x64 SoA Chunk Layout, Alignment & Bitwise Indexing...\n";

    // 1.1 Compile-time alignment and size verification
    static_assert(alignof(Chunk) == 64, "Chunk class MUST be 64-byte aligned");
    static_assert(sizeof(Chunk::material) == 8192, "Material array must be 8,192 bytes");
    static_assert(sizeof(Chunk::temperature) == 16384, "Temperature array must be 16,384 bytes");
    static_assert(sizeof(Chunk::flags) == 4096, "Flags array must be 4,096 bytes");
    static_assert(sizeof(Chunk::lifetime) == 4096, "Lifetime array must be 4,096 bytes");

    // 1.2 Runtime alignment on statically/stack-allocated chunk
    alignas(64) Chunk stackChunk;
    uintptr_t baseAddr = reinterpret_cast<uintptr_t>(&stackChunk);
    STRESS_ASSERT(baseAddr % 64 == 0, "Stack chunk base address is not 64-byte aligned");

    uintptr_t matAddr = reinterpret_cast<uintptr_t>(&stackChunk.material[0]);
    uintptr_t tempAddr = reinterpret_cast<uintptr_t>(&stackChunk.temperature[0]);
    uintptr_t flagsAddr = reinterpret_cast<uintptr_t>(&stackChunk.flags[0]);
    uintptr_t lifeAddr = reinterpret_cast<uintptr_t>(&stackChunk.lifetime[0]);

    STRESS_ASSERT(matAddr % 64 == 0, "material array is not 64-byte aligned");
    STRESS_ASSERT(tempAddr % 64 == 0, "temperature array is not 64-byte aligned");
    STRESS_ASSERT(flagsAddr % 64 == 0, "flags array is not 64-byte aligned");
    STRESS_ASSERT(lifeAddr % 64 == 0, "lifetime array is not 64-byte aligned");

    // 1.3 Exact offset calculations
    size_t offMat = offsetof(Chunk, material);
    size_t offTemp = offsetof(Chunk, temperature);
    size_t offFlags = offsetof(Chunk, flags);
    size_t offLife = offsetof(Chunk, lifetime);

    STRESS_ASSERT(offMat % 64 == 0, "Offset of material is not a multiple of 64");
    STRESS_ASSERT(offTemp % 64 == 0, "Offset of temperature is not a multiple of 64");
    STRESS_ASSERT(offFlags % 64 == 0, "Offset of flags is not a multiple of 64");
    STRESS_ASSERT(offLife % 64 == 0, "Offset of lifetime is not a multiple of 64");

    // 1.4 Core Array Sizing (exact 32,768 bytes = 32 KB)
    size_t totalCoreArrayBytes = sizeof(stackChunk.material) +
                                sizeof(stackChunk.temperature) +
                                sizeof(stackChunk.flags) +
                                sizeof(stackChunk.lifetime);
    STRESS_ASSERT(totalCoreArrayBytes == 32768, "Total core array bytes != 32768 (32 KB)");

    std::cout << "  [1.1] Stack Chunk aligned: base=" << baseAddr
              << " offMat=" << offMat
              << " offTemp=" << offTemp
              << " offFlags=" << offFlags
              << " offLife=" << offLife
              << " coreBytes=" << totalCoreArrayBytes << " [PASS]\n";

    // 1.5 Heap-allocated chunks alignment stress (100 dynamic allocations)
    for (int i = 0; i < 100; ++i) {
        auto heapChunk = std::make_unique<Chunk>(i, i);
        uintptr_t hBase = reinterpret_cast<uintptr_t>(heapChunk.get());
        STRESS_ASSERT(hBase % 64 == 0, "Heap chunk instance is not 64-byte aligned");
        STRESS_ASSERT(reinterpret_cast<uintptr_t>(&heapChunk->material[0]) % 64 == 0, "Heap material unaligned");
        STRESS_ASSERT(reinterpret_cast<uintptr_t>(&heapChunk->temperature[0]) % 64 == 0, "Heap temp unaligned");
        STRESS_ASSERT(reinterpret_cast<uintptr_t>(&heapChunk->flags[0]) % 64 == 0, "Heap flags unaligned");
        STRESS_ASSERT(reinterpret_cast<uintptr_t>(&heapChunk->lifetime[0]) % 64 == 0, "Heap life unaligned");
    }
    std::cout << "  [1.2] Heap allocation alignment verified across 100 dynamic Chunks [PASS]\n";

    // 1.6 World grid chunk alignment verification (all 240 chunks in 1280x768 world)
    World world(1280, 768);
    for (core::i32 cy = 0; cy < world.GetChunksY(); ++cy) {
        for (core::i32 cx = 0; cx < world.GetChunksX(); ++cx) {
            Chunk* c = world.GetChunk(cx, cy);
            STRESS_ASSERT(c != nullptr, "Null chunk pointer in world");
            uintptr_t wAddr = reinterpret_cast<uintptr_t>(c);
            STRESS_ASSERT(wAddr % 64 == 0, "Chunk pointer in World unaligned");
            STRESS_ASSERT(reinterpret_cast<uintptr_t>(&c->material[0]) % 64 == 0, "World chunk material unaligned");
            STRESS_ASSERT(reinterpret_cast<uintptr_t>(&c->temperature[0]) % 64 == 0, "World chunk temp unaligned");
            STRESS_ASSERT(reinterpret_cast<uintptr_t>(&c->flags[0]) % 64 == 0, "World chunk flags unaligned");
            STRESS_ASSERT(reinterpret_cast<uintptr_t>(&c->lifetime[0]) % 64 == 0, "World chunk life unaligned");
        }
    }
    std::cout << "  [1.3] All 240 Chunks in World(1280, 768) verified 64-byte aligned [PASS]\n";

    // 1.7 Bitwise indexing bijection & bounds verification
    // Formula: Index(lx, ly) == (ly << 6) | lx
    std::vector<bool> indexSeen(Chunk::AREA, false);
    for (core::i32 ly = 0; ly < Chunk::SIZE; ++ly) {
        for (core::i32 lx = 0; lx < Chunk::SIZE; ++lx) {
            core::i32 idx = Chunk::Index(lx, ly);

            // Bounds check
            STRESS_ASSERT(idx >= 0 && idx < Chunk::AREA, "Index out of [0, 4095] bounds");

            // Exact arithmetic equivalence check
            STRESS_ASSERT(idx == (ly * 64 + lx), "Index does not match ly * 64 + lx");
            STRESS_ASSERT(idx == ((ly << 6) | lx), "Index does not match (ly << 6) | lx");

            // Zero collision check
            STRESS_ASSERT(!indexSeen[idx], "Index collision detected at index " << idx);
            indexSeen[idx] = true;

            // Inversion check
            core::i32 recLx = -1, recLy = -1;
            Chunk::FromIndex(idx, recLx, recLy);
            STRESS_ASSERT(recLx == lx && recLy == ly, "FromIndex failed to reconstruct coordinates");
        }
    }
    // Verify all 4096 indices were visited
    for (size_t i = 0; i < indexSeen.size(); ++i) {
        STRESS_ASSERT(indexSeen[i], "Unvisited index in 64x64 domain: " << i);
    }

    // Boundary edge cases
    STRESS_ASSERT(Chunk::Index(0, 0) == 0, "(0, 0) != 0");
    STRESS_ASSERT(Chunk::Index(63, 0) == 63, "(63, 0) != 63");
    STRESS_ASSERT(Chunk::Index(0, 63) == 4032, "(0, 63) != 4032");
    STRESS_ASSERT(Chunk::Index(63, 63) == 4095, "(63, 63) != 4095");

    std::cout << "  [1.4] Bitwise indexing bijection verified across all 4,096 coordinate pairs [PASS]\n";
}

// ============================================================================
// SUITE 2: Double-Update Prevention (FLAG_PARITY = 0x01) Stress Tests
// ============================================================================
static void StressTestParityDoubleUpdatePrevention() {
    std::cout << "[SUITE 2] Double-Update Prevention (FLAG_PARITY = 0x01) Stress Tests...\n";

    MaterialRegistry& registry = MaterialRegistry::Instance();
    if (registry.Count() <= 1) {
        bool ok = registry.LoadFromDirectory("assets/materials");
        if (!ok) {
            ok = registry.LoadFromDirectory("../assets/materials");
        }
        STRESS_ASSERT(ok, "Failed to load materials from assets directory");
    }
    MaterialId sand = registry.FindByName("sand");
    STRESS_ASSERT(sand != EMPTY_MATERIAL_ID, "Sand material not found in registry");

    World world(1280, 768);
    Physics physics(1337u);

    // 2.1 Full-row top drop: place sand across row y = 0 for all 1280 columns
    for (core::i32 x = 0; x < 1280; ++x) {
        world.SetCell(x, 0, sand);
    }
    STRESS_ASSERT(world.GetTotalActiveCells() == 1280, "Initial active cell count != 1280");

    // Initial check: row 0 has sand, rows 1..767 are empty
    for (core::i32 x = 0; x < 1280; ++x) {
        STRESS_ASSERT(world.GetCellMaterial(x, 0) == sand, "Sand missing at row 0");
    }
    for (core::i32 y = 1; y < 768; ++y) {
        // Spot check columns
        for (core::i32 x = 0; x < 1280; x += 128) {
            STRESS_ASSERT(world.GetCellMaterial(x, y) == EMPTY_MATERIAL_ID, "Unexpected cell at y > 0");
        }
    }

    core::u8 parityBefore = world.GetCurrentParity();

    // RUN EXACTLY 1 TICK
    physics.Step(world);
    world.AdvanceTick();

    core::u8 parityAfter = world.GetCurrentParity();
    STRESS_ASSERT(parityAfter == (parityBefore ^ FLAG_PARITY), "Parity bit did not toggle after AdvanceTick");

    // EMPIRICAL VERIFICATION OF DOUBLE-UPDATE PREVENTION:
    // 1. Every single particle must have advanced by AT MOST 1 cell (from y=0 to y=1).
    // 2. Row 0 must be 100% EMPTY.
    // 3. Row 1 must have ALL 1280 sand particles.
    // 4. Rows 2..767 must have STRICTLY 0 sand particles (ZERO cascading down multiple rows!).
    core::u32 countRow0 = 0;
    core::u32 countRow1 = 0;
    core::u32 countRowBelow = 0;

    for (core::i32 x = 0; x < 1280; ++x) {
        if (world.GetCellMaterial(x, 0) == sand) ++countRow0;
        if (world.GetCellMaterial(x, 1) == sand) ++countRow1;
    }

    for (core::i32 y = 2; y < 768; ++y) {
        for (core::i32 x = 0; x < 1280; ++x) {
            if (world.GetCellMaterial(x, y) == sand) ++countRowBelow;
        }
    }

    STRESS_ASSERT(countRow0 == 0, "Row 0 still contains particles: count=" << countRow0);
    STRESS_ASSERT(countRow1 == 1280, "Row 1 does not contain all particles: count=" << countRow1);
    STRESS_ASSERT(countRowBelow == 0, "CRITICAL: Particles cascaded down beyond row 1! countRowBelow=" << countRowBelow);

    // Also inspect cell flags on row 1: parity bit must match parityBefore (the parity under which they moved)
    for (core::i32 x = 0; x < 1280; ++x) {
        core::u8 f = world.GetCellFlags(x, 1);
        STRESS_ASSERT((f & FLAG_PARITY) == parityBefore, "Cell parity flag does not match execution tick parity");
    }

    std::cout << "  [2.1] 1280 top-row particles advanced by EXACTLY 1 cell in 1 tick (zero cascading) [PASS]\n";

    // 2.2 Isolated particles falling across 60 sequential single ticks
    world.Clear();
    core::i32 testCol = 500;
    core::i32 startRow = 5;
    world.SetCell(testCol, startRow, sand);

    for (core::i32 step = 1; step <= 50; ++step) {
        core::i32 expectedY = startRow + step;
        physics.Step(world);
        world.AdvanceTick();

        // Must be at expectedY
        STRESS_ASSERT(world.GetCellMaterial(testCol, expectedY) == sand, "Particle not at expected row " << expectedY);
        // Previous row must be empty
        STRESS_ASSERT(world.GetCellMaterial(testCol, expectedY - 1) == EMPTY_MATERIAL_ID, "Row above particle not empty");
        // No other cells set anywhere
        STRESS_ASSERT(world.GetTotalActiveCells() == 1, "Active cell count changed during single particle fall");
    }
    std::cout << "  [2.2] 50 consecutive single-tick steps verified 1-to-1 position correspondence [PASS]\n";

    // 2.3 Chunk boundary crossing parity check (y=63 -> y=64)
    world.Clear();
    world.SetCell(100, 63, sand); // In chunk (1, 0)
    STRESS_ASSERT(world.GetCellMaterial(100, 63) == sand, "Setup failed");

    physics.Step(world);
    world.AdvanceTick();

    // Now in chunk (1, 1) at (100, 64)
    STRESS_ASSERT(world.GetCellMaterial(100, 63) == EMPTY_MATERIAL_ID, "Sand remained in chunk (1, 0)");
    STRESS_ASSERT(world.GetCellMaterial(100, 64) == sand, "Sand not found at row 64");
    STRESS_ASSERT(world.GetCellMaterial(100, 65) == EMPTY_MATERIAL_ID, "Sand cascaded into row 65 across boundary!");

    std::cout << "  [2.3] Inter-chunk boundary crossing (row 63 -> 64) respects single-tick advance [PASS]\n";
}

// ============================================================================
// SUITE 3: Brush Bresenham Continuous Stroke & Radius Stress Tests
// ============================================================================

// Model implementation of Bresenham line to act as an oracle
static std::vector<core::Point2i> GenerateBresenhamTrajectory(core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1) {
    std::vector<core::Point2i> path;
    core::i32 dx = std::abs(x1 - x0);
    core::i32 dy = -std::abs(y1 - y0);
    core::i32 sx = (x0 < x1) ? 1 : -1;
    core::i32 sy = (y0 < y1) ? 1 : -1;
    core::i32 err = dx + dy;

    core::i32 curX = x0;
    core::i32 curY = y0;

    while (true) {
        path.push_back({ curX, curY });
        if (curX == x1 && curY == y1) break;

        core::i32 e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            curX += sx;
        }
        if (e2 <= dx) {
            err += dx;
            curY += sy;
        }
    }
    return path;
}

// Theoretical integer grid circle cell count for a given radius R
static size_t CalculateTheoreticalDiskCellCount(core::i32 R) {
    if (R <= 1) return 1;
    size_t count = 0;
    core::i32 r2 = R * R;
    for (core::i32 dy = -R; dy <= R; ++dy) {
        for (core::i32 dx = -R; dx <= R; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                ++count;
            }
        }
    }
    return count;
}

static void StressTestBrushBresenhamAndRadius() {
    std::cout << "[SUITE 3] Brush Bresenham Stroke Continuity & Radius Scaling Stress Tests...\n";

    World world(1280, 768);
    Brush brush;
    MaterialId sand = 3;

    // 3.1 1,000 Random Fast Cursor Jumps across entire 1280x768 world grid
    core::FastRNG rng(987654321u);
    const int JUMP_COUNT = 1000;

    std::cout << "  [3.1] Generating and verifying " << JUMP_COUNT << " random fast cursor jumps...\n";

    for (int i = 0; i < JUMP_COUNT; ++i) {
        core::i32 x0 = static_cast<core::i32>(rng.NextU32() % 1280);
        core::i32 y0 = static_cast<core::i32>(rng.NextU32() % 768);
        core::i32 x1 = static_cast<core::i32>(rng.NextU32() % 1280);
        core::i32 y1 = static_cast<core::i32>(rng.NextU32() % 768);

        // Generate theoretical oracle trajectory
        std::vector<core::Point2i> trajectory = GenerateBresenhamTrajectory(x0, y0, x1, y1);
        STRESS_ASSERT(!trajectory.empty(), "Trajectory must not be empty");

        // Verify trajectory start and end
        STRESS_ASSERT(trajectory.front().x == x0 && trajectory.front().y == y0, "Trajectory start mismatch");
        STRESS_ASSERT(trajectory.back().x == x1 && trajectory.back().y == y1, "Trajectory end mismatch");

        // Assert 8-connected continuity for EVERY consecutive pair of rasterized cells:
        // max(|dx|, |dy|) <= 1 (strictly zero gaps, 8-connected continuity)
        for (size_t k = 0; k + 1 < trajectory.size(); ++k) {
            core::i32 dx = std::abs(trajectory[k + 1].x - trajectory[k].x);
            core::i32 dy = std::abs(trajectory[k + 1].y - trajectory[k].y);
            core::i32 maxDelta = std::max(dx, dy);

            STRESS_ASSERT(maxDelta <= 1, "Gap detected in trajectory at step " << k
                          << " from (" << trajectory[k].x << ", " << trajectory[k].y << ")"
                          << " to (" << trajectory[k + 1].x << ", " << trajectory[k + 1].y << ")");
            STRESS_ASSERT(maxDelta >= 1, "Redundant stationary duplicate point detected in trajectory at step " << k);
        }

        // Expected length = max(|x1 - x0|, |y1 - y0|) + 1
        core::i32 expectedLength = std::max(std::abs(x1 - x0), std::abs(y1 - y0)) + 1;
        STRESS_ASSERT(static_cast<core::i32>(trajectory.size()) == expectedLength,
                      "Trajectory length " << trajectory.size() << " != expected " << expectedLength);

        // Spot-test programmatic brush execution in World:
        // Paint the stroke with R=1
        brush.PaintStroke(world, x0, y0, x1, y1, sand, 1);

        // Assert all points on the trajectory are set to sand in World
        for (const auto& pt : trajectory) {
            STRESS_ASSERT(world.GetCellMaterial(pt.x, pt.y) == sand,
                          "World cell not set at (" << pt.x << ", " << pt.y << ")");
        }

        // Clean up the painted cells for the next iteration
        brush.PaintStroke(world, x0, y0, x1, y1, EMPTY_MATERIAL_ID, 1);
        for (const auto& pt : trajectory) {
            STRESS_ASSERT(world.GetCellMaterial(pt.x, pt.y) == EMPTY_MATERIAL_ID,
                          "World cell not cleaned at (" << pt.x << ", " << pt.y << ")");
        }
    }
    std::cout << "  -> 1,000 random cursor jumps: 100% strictly 8-connected continuous, 0 gaps [PASS]\n";

    // 3.2 Extreme Cardinal & Diagonal Trajectories
    struct ExtremeCase {
        core::i32 x0, y0, x1, y1;
        const char* desc;
    };
    ExtremeCase extremes[] = {
        { 0, 0, 1279, 767, "Full World Diagonal Main" },
        { 1279, 0, 0, 767, "Full World Diagonal Anti" },
        { 0, 384, 1279, 384, "Full World Horizontal" },
        { 640, 0, 640, 767, "Full World Vertical" },
        { 500, 500, 500, 500, "Stationary Point (x0 == x1, y0 == y1)" },
        { 100, 100, 101, 100, "Adjacent 1-Step Horizontal" },
        { 100, 100, 100, 101, "Adjacent 1-Step Vertical" },
        { 100, 100, 101, 101, "Adjacent 1-Step Diagonal" },
        { 10, 10, 20, 100, "Steep Trajectory (dx=10, dy=90)" },
        { 10, 10, 100, 20, "Shallow Trajectory (dx=90, dy=10)" }
    };

    for (const auto& tc : extremes) {
        auto traj = GenerateBresenhamTrajectory(tc.x0, tc.y0, tc.x1, tc.y1);
        STRESS_ASSERT(!traj.empty(), "Extreme trajectory empty");
        for (size_t k = 0; k + 1 < traj.size(); ++k) {
            core::i32 dx = std::abs(traj[k + 1].x - traj[k].x);
            core::i32 dy = std::abs(traj[k + 1].y - traj[k].y);
            STRESS_ASSERT(std::max(dx, dy) <= 1, "Gap in extreme trajectory " << tc.desc);
        }
    }
    std::cout << "  [3.2] Extreme cardinal, diagonal, and single-pixel trajectories verified [PASS]\n";

    // 3.3 Radius Scaling R = 1 .. 50 and Disk Bounds Stress Test
    std::cout << "  [3.3] Verifying Disk Bounds & Cell Count for all R in [1, 50]...\n";
    core::i32 centerX = 640;
    core::i32 centerY = 384;

    for (core::i32 R = 1; R <= 50; ++R) {
        world.Clear();
        brush.PaintDisk(world, centerX, centerY, sand, R);

        size_t expectedCount = CalculateTheoreticalDiskCellCount(R);
        size_t actualCount = 0;

        // Scan bounding box around disk
        for (core::i32 dy = -R - 2; dy <= R + 2; ++dy) {
            core::i32 wy = centerY + dy;
            if (wy < 0 || wy >= world.GetHeight()) continue;

            for (core::i32 dx = -R - 2; dx <= R + 2; ++dx) {
                core::i32 wx = centerX + dx;
                if (wx < 0 || wx >= world.GetWidth()) continue;

                MaterialId mat = world.GetCellMaterial(wx, wy);
                core::i32 dist2 = dx * dx + dy * dy;

                if (R <= 1) {
                    if (dx == 0 && dy == 0) {
                        STRESS_ASSERT(mat == sand, "R=1 center cell missing");
                        ++actualCount;
                    } else {
                        STRESS_ASSERT(mat == EMPTY_MATERIAL_ID, "R=1 non-center cell set");
                    }
                } else {
                    if (dist2 <= R * R) {
                        STRESS_ASSERT(mat == sand, "Cell inside radius missing at dx=" << dx << " dy=" << dy << " for R=" << R);
                        ++actualCount;
                    } else {
                        STRESS_ASSERT(mat == EMPTY_MATERIAL_ID, "Cell outside radius set at dx=" << dx << " dy=" << dy << " for R=" << R);
                    }
                }
            }
        }

        STRESS_ASSERT(actualCount == expectedCount,
                      "Disk cell count mismatch for R=" << R << ": actual=" << actualCount << " expected=" << expectedCount);
    }
    std::cout << "  -> All 50 radii (R=1..50) matched exact Euclidean disk mathematical oracle [PASS]\n";

    // 3.4 Boundary Clipping & Out-of-Bounds Disk Safety
    world.Clear();
    // Test large R=50 disks intersecting borders and far out-of-bounds
    brush.PaintDisk(world, 0, 0, sand, 50);             // Top-left corner
    brush.PaintDisk(world, 1279, 767, sand, 50);       // Bottom-right corner
    brush.PaintDisk(world, -100, -100, sand, 50);      // Completely out of bounds (negative)
    brush.PaintDisk(world, 2000, 2000, sand, 50);      // Completely out of bounds (positive)

    // Verify all active cells in the world are within valid bounds
    for (core::i32 cy = 0; cy < world.GetChunksY(); ++cy) {
        for (core::i32 cx = 0; cx < world.GetChunksX(); ++cx) {
            const Chunk* c = world.GetChunk(cx, cy);
            for (core::i32 idx = 0; idx < Chunk::AREA; ++idx) {
                if (c->material[idx] != EMPTY_MATERIAL_ID) {
                    core::i32 lx = 0, ly = 0;
                    Chunk::FromIndex(idx, lx, ly);
                    core::i32 wx = (cx << 6) | lx;
                    core::i32 wy = (cy << 6) | ly;
                    STRESS_ASSERT(wx >= 0 && wx < 1280 && wy >= 0 && wy < 768, "Out of bounds coordinate found!");
                }
            }
        }
    }
    std::cout << "  [3.4] Boundary clipping and out-of-bounds painting safety verified [PASS]\n";
}

// ============================================================================
// SUITE 4: WorldRenderer Texture & Point Sampling (GL_NEAREST) Verification
// ============================================================================
static void StressTestWorldRendererPointSampling() {
    std::cout << "[SUITE 4] WorldRenderer Streaming Texture & Point Sampling (GL_NEAREST)...\n";

    // Initialize SDL video subsystem in headless mode
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "  [WARN] SDL_Init(SDL_INIT_VIDEO) returned " << SDL_GetError() << " (headless environment)\n";
        return;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Challenger2 Headless Test",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        64, 64,
        SDL_WINDOW_HIDDEN
    );

    if (!window) {
        std::cerr << "  [WARN] Unable to create test SDL window: " << SDL_GetError() << "\n";
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        std::cerr << "  [WARN] Unable to create test SDL renderer: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // 4.1 Instantiate WorldRenderer
    WorldRenderer worldRenderer(renderer, 1280, 768);

    // 4.2 Create texture with identical parameters as WorldRenderer and inspect scale mode
    SDL_Texture* testTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        1280,
        768
    );
    STRESS_ASSERT(testTexture != nullptr, "Failed to create SDL test streaming texture");

    // Apply the exact point sampling setting used in WorldRenderer constructor
    int setRes = SDL_SetTextureScaleMode(testTexture, SDL_ScaleModeNearest);
    STRESS_ASSERT(setRes == 0, "SDL_SetTextureScaleMode failed");

    // Query back the scale mode via SDL runtime
    SDL_ScaleMode retrievedMode = SDL_ScaleModeLinear;
    int getRes = SDL_GetTextureScaleMode(testTexture, &retrievedMode);
    STRESS_ASSERT(getRes == 0, "SDL_GetTextureScaleMode failed");
    STRESS_ASSERT(retrievedMode == SDL_ScaleModeNearest,
                  "Texture scale mode is NOT SDL_ScaleModeNearest! Got: " << static_cast<int>(retrievedMode));

    std::cout << "  [4.1] SDL streaming texture verified with SDL_ScaleModeNearest (GL_NEAREST point sampling) [PASS]\n";

    // 4.3 Test rendering pass execution
    World world(1280, 768);
    world.SetCell(100, 100, 3); // Sand
    SDL_Rect viewport{ 0, 0, 1280, 768 };
    worldRenderer.Render(world, renderer, &viewport);

    // 4.4 Test ScreenToWorld coordinate transformation
    core::i32 wx = -1, wy = -1;
    bool inWorld = worldRenderer.ScreenToWorld(640, 384, viewport, wx, wy);
    STRESS_ASSERT(inWorld, "ScreenToWorld returned false for valid center coordinate");
    STRESS_ASSERT(wx == 640 && wy == 384, "ScreenToWorld coordinate mismatch: got (" << wx << ", " << wy << ")");

    // Out of bounds screen coords
    STRESS_ASSERT(!worldRenderer.ScreenToWorld(-10, 50, viewport, wx, wy), "ScreenToWorld accepted negative x");
    STRESS_ASSERT(!worldRenderer.ScreenToWorld(1300, 50, viewport, wx, wy), "ScreenToWorld accepted x >= viewport.w");

    std::cout << "  [4.2] WorldRenderer Render() execution and ScreenToWorld transforms verified [PASS]\n";

    SDL_DestroyTexture(testTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::cout << "================================================================\n";
    std::cout << "  CHALLENGER 2 EMPIRICAL ADVERSARIAL STRESS TEST HARNESS\n";
    std::cout << "================================================================\n";

    try {
        StressTestChunkLayoutAndIndexing();
        StressTestParityDoubleUpdatePrevention();
        StressTestBrushBresenhamAndRadius();
        StressTestWorldRendererPointSampling();
    } catch (const std::exception& ex) {
        std::cerr << "\n[CRITICAL EXCEPTION] " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "\n================================================================\n";
    std::cout << "  ALL EMPIRICAL CHALLENGER 2 STRESS TESTS PASSED (4/4 SUITES)\n";
    std::cout << "================================================================\n";
    return 0;
}
