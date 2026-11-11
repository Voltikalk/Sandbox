# Milestone 1 Empirical Adversarial Challenge Report (Challenger 2)

**Author**: Challenger 2 (`teamwork_challenger_m1_2`)  
**Domain**: Chunk Alignment, SoA Sizing, Parity Tracking & Brush Stress Verification  
**Date**: 2026-11-11  
**Target Recipient**: Parent Orchestrator (`ddc9ae7d-022c-473d-9787-f1fcb28efe6f`)  
**Verdict**: **APPROVE**  

---

## Challenge Summary

**Overall risk assessment**: **LOW**  
All empirical stress tests and mathematical oracles passed without defects. The data layout, parity double-update prevention, continuous Bresenham stroke interpolation, and nearest-neighbor rendering point-sampling conform strictly to the architectural contracts defined in `PROJECT.md` and `SCOPE.md`.

---

## 1. Observation

### 1.1 Direct Source Code Observations
1. **64x64 SoA Chunk Alignment & Sizing (`src/sim/Chunk.hpp:20-31, 93-99`)**:
   - `class alignas(core::CACHE_LINE_SIZE) Chunk` enforces 64-byte alignment on the class definition.
   - Core arrays:
     * `alignas(core::CACHE_LINE_SIZE) MaterialId material[AREA]{0};` (4096 * 2 = 8,192 bytes)
     * `alignas(core::CACHE_LINE_SIZE) core::f32  temperature[AREA]{};` (4096 * 4 = 16,384 bytes)
     * `alignas(core::CACHE_LINE_SIZE) core::u8   flags[AREA]{0};` (4096 * 1 = 4,096 bytes)
     * `alignas(core::CACHE_LINE_SIZE) core::u8   lifetime[AREA]{0};` (4096 * 1 = 4,096 bytes)
   - Core arrays sum to exactly: $8192 + 16384 + 4096 + 4096 = 32,768$ bytes (32 KB, L1 data cache resident).
   - Bitwise indexing functions:
     * `Index(lx, ly) = (ly << 6) | lx` (`src/sim/Chunk.hpp:47-49`)
     * `FromIndex(index, lx, ly)`: `lx = index & 63; ly = index >> 6;` (`src/sim/Chunk.hpp:52-55`)

2. **Parity Double-Update Prevention (`src/sim/Physics.cpp:56-74` & `src/sim/World.cpp:158-161, 225-227`)**:
   - In `Physics::UpdateCell`:
     ```cpp
     // Check parity bit: skip if already updated during this tick
     core::u8 flags = world.GetCellFlags(x, y);
     if ((flags & FLAG_PARITY) == currentParity) return;

     // Mark as visited in this tick so resting cells toggle parity properly
     world.SetCellFlags(x, y, (flags & ~FLAG_PARITY) | currentParity);
     ```
   - In `World::SwapCells`:
     ```cpp
     // Stamp current tick parity to prevent double-updates
     c1->flags[idx1] = (c1->flags[idx1] & ~FLAG_PARITY) | m_currentParity;
     c2->flags[idx2] = (c2->flags[idx2] & ~FLAG_PARITY) | m_currentParity;
     ```
   - In `World::AdvanceTick`:
     ```cpp
     void World::AdvanceTick() noexcept {
         m_currentParity ^= FLAG_PARITY;
     }
     ```
   - In `Physics::Step`: outer traversal loops from bottom to top: `for (core::i32 y = height - 1; y >= 0; --y)`.

3. **Brush Bresenham Stroke & Disk Stencil (`src/ui/Brush.cpp:78-131`)**:
   - `PaintStroke` implements Alois Zingl's integer Bresenham algorithm:
     ```cpp
     core::i32 dx = std::abs(x1 - x0);
     core::i32 dy = -std::abs(y1 - y0);
     core::i32 sx = (x0 < x1) ? 1 : -1;
     core::i32 sy = (y0 < y1) ? 1 : -1;
     core::i32 err = dx + dy;
     while (true) {
         PaintDisk(world, x0, y0, mat, radius);
         if (x0 == x1 && y0 == y1) break;
         core::i32 e2 = 2 * err;
         if (e2 >= dy) { err += dy; x0 += sx; }
         if (e2 <= dx) { err += dx; y0 += sy; }
     }
     ```
   - `PaintDisk` tests Euclidean distance squared against radius squared:
     `if (dx * dx + dy * dy <= r2) { world.SetCell(wx, wy, mat); }`

4. **WorldRenderer Point Sampling (`src/ui/Renderer.cpp:26-28`)**:
   - Streaming texture created with `SDL_PIXELFORMAT_RGBA32`, `SDL_TEXTUREACCESS_STREAMING`.
   - Scalemode explicitly configured:
     `SDL_SetTextureScaleMode(m_texture, SDL_ScaleModeNearest);`

---

### 1.2 Tool Execution & Empirical Results
An independent empirical stress test binary (`tests/test_challenger2_stress.cpp`) was written, compiled with MSVC x64 in Release configuration, and executed.

**Build Command**:
```powershell
cmake --build build --config Release --target test_challenger2_stress
```
*Result*: Exit code 0, 0 compiler warnings, 0 linker warnings.

**Harness Execution**:
```powershell
.\build\Release\test_challenger2_stress.exe
```
**Verbatim Output**:
```
================================================================
  CHALLENGER 2 EMPIRICAL ADVERSARIAL STRESS TEST HARNESS
================================================================
[SUITE 1] 64x64 SoA Chunk Layout, Alignment & Bitwise Indexing...
  [1.1] Stack Chunk aligned: base=893937220096 offMat=0 offTemp=8192 offFlags=24576 offLife=28672 coreBytes=32768 [PASS]
  [1.2] Heap allocation alignment verified across 100 dynamic Chunks [PASS]
  [1.3] All 240 Chunks in World(1280, 768) verified 64-byte aligned [PASS]
  [1.4] Bitwise indexing bijection verified across all 4,096 coordinate pairs [PASS]
[SUITE 2] Double-Update Prevention (FLAG_PARITY = 0x01) Stress Tests...
[09:48:03.841] [INFO ] [MaterialRegistry] Successfully loaded 18 materials from assets/materials
  [2.1] 1280 top-row particles advanced by EXACTLY 1 cell in 1 tick (zero cascading) [PASS]
  [2.2] 50 consecutive single-tick steps verified 1-to-1 position correspondence [PASS]
  [2.3] Inter-chunk boundary crossing (row 63 -> 64) respects single-tick advance [PASS]
[SUITE 3] Brush Bresenham Stroke Continuity & Radius Scaling Stress Tests...
  [3.1] Generating and verifying 1000 random fast cursor jumps...
  -> 1,000 random cursor jumps: 100% strictly 8-connected continuous, 0 gaps [PASS]
  [3.2] Extreme cardinal, diagonal, and single-pixel trajectories verified [PASS]
  [3.3] Verifying Disk Bounds & Cell Count for all R in [1, 50]...
  -> All 50 radii (R=1..50) matched exact Euclidean disk mathematical oracle [PASS]
  [3.4] Boundary clipping and out-of-bounds painting safety verified [PASS]
[SUITE 4] WorldRenderer Streaming Texture & Point Sampling (GL_NEAREST)...
  [4.1] SDL streaming texture verified with SDL_ScaleModeNearest (GL_NEAREST point sampling) [PASS]
  [4.2] WorldRenderer Render() execution and ScreenToWorld transforms verified [PASS]

================================================================
  ALL EMPIRICAL CHALLENGER 2 STRESS TESTS PASSED (4/4 SUITES)
================================================================
```

**CTest Execution**:
```powershell
ctest --test-dir build -C Release --output-on-failure
```
**Verbatim Output**:
```
Test project C:/Users/Drilla/Desktop/Sandbox/build
    Start 1: e2e_all
1/7 Test #1: e2e_all ..........................   Passed    0.01 sec
    Start 2: e2e_tier1
2/7 Test #2: e2e_tier1 ........................   Passed    0.01 sec
    Start 3: e2e_tier2
3/7 Test #3: e2e_tier2 ........................   Passed    0.01 sec
    Start 4: e2e_tier3
4/7 Test #4: e2e_tier3 ........................   Passed    0.01 sec
    Start 5: e2e_tier4
5/7 Test #5: e2e_tier4 ........................   Passed    0.01 sec
    Start 6: unit_test_physics
6/7 Test #6: unit_test_physics ................   Passed    0.07 sec
    Start 7: unit_test_challenger2_stress
7/7 Test #7: unit_test_challenger2_stress .....   Passed    0.17 sec

100% tests passed, 0 tests failed out of 7

Total Test time (real) =   0.30 sec
```

---

## 2. Logic Chain

1. **Alignment and Sizing Logic (Suite 1)**:
   - *Observation 1.1 & 1.2*: `alignof(Chunk)` is verified at compile-time via `static_assert(alignof(Chunk) == 64)` and at runtime across stack instances, 100 dynamic heap allocations (`std::make_unique<Chunk>`), and all 240 chunks inside `World(1280, 768)`.
   - *Field Offsets*: Measured offsets `offMat = 0`, `offTemp = 8192`, `offFlags = 24576`, `offLife = 28672`. Each offset modulo 64 is strictly 0.
   - *Core Payload Size*: $8192 + 16384 + 4096 + 4096 = 32,768$ bytes ($32.0$ KB), matching L1 data cache capacity.
   - *Bitwise Indexing*: Evaluated `(ly << 6) | lx` against `ly * 64 + lx` for all 4,096 coordinate pairs $(lx, ly) \in [0, 63] \times [0, 63]$. The mapping is strictly bijective: zero collisions, zero unmapped indices, and `FromIndex` recovers original coordinates with 100% fidelity.

2. **Double-Update Prevention & Parity Bit Logic (Suite 2)**:
   - *Observation 1.1 & 1.2*: Placed 1,280 sand particles across the entire top row $y = 0$.
   - Stepped simulation by 1 tick: `physics.Step(world); world.AdvanceTick();`.
   - Results: Row 0 was 100% emptied (`countRow0 == 0`). Row 1 received all 1,280 particles (`countRow1 == 1280`). All lower rows $y \in [2, 767]$ contained exactly zero particles (`countRowBelow == 0`).
   - Conclusion: Falling sand strictly advances by at most 1 cell per tick. Double-update cascades within a single tick are completely eliminated by the combination of bottom-to-top traversal and `FLAG_PARITY` stamping in `World::SwapCells` and `Physics::UpdateCell`.
   - 50 consecutive single-tick steps and an inter-chunk boundary transition ($y=63 \to y=64$) also verified 1-to-1 position correspondence without freeze or cascade.

3. **Bresenham Continuous Stroke & Radius Stencil Logic (Suite 3)**:
   - *Observation 1.1 & 1.2*: Generated 1,000 randomized cursor jumps across the entire $1280 \times 768$ canvas using deterministic PRNG.
   - For every trajectory, evaluated step-by-step continuity:
     $\Delta x = x_{k+1} - x_k \in \{-1, 0, 1\}$, $\Delta y = y_{k+1} - y_k \in \{-1, 0, 1\}$, with $\max(|\Delta x|, |\Delta y|) = 1$.
     Zero gaps occurred across all 1,000 jumps (100% strictly 8-connected continuous).
   - Tested radius scaling $R \in [1, 50]$ against an exact discrete Euclidean disk oracle $N(R) = \sum_{dy=-R}^R \sum_{dx=-R}^R [dx^2 + dy^2 \le R^2]$. For every radius from 1 to 50, the painted cell count and bounding stencil matched the oracle with 100% precision.
   - Boundary clipping tests ($R=50$ at screen corners and negative coordinates) confirmed memory safety with zero out-of-bounds writes.

4. **Renderer Nearest-Neighbor Filtering Logic (Suite 4)**:
   - *Observation 1.1 & 1.2*: In `WorldRenderer`, `SDL_SetTextureScaleMode(m_texture, SDL_ScaleModeNearest)` is called immediately upon texture creation.
   - Executed SDL runtime query `SDL_GetTextureScaleMode` in headless test: verified scale mode returns `SDL_ScaleModeNearest` (`GL_NEAREST`), ensuring crisp pixel art rendering without blur.

---

## 3. Caveats

- **No Caveats**: All 4 areas specified in the dispatch instructions were empirically challenged, stress-tested with automated oracles, and verified to pass with 0 errors and 0 warnings.
- *Out of Scope*: Multithreaded 4-phase checkerboard parallel execution and sleeping chunk optimization are scheduled for Milestone 3 (`PROJECT.md` Feature 15 & 16) and are not part of the Milestone 1 single-threaded baseline.

---

## 4. Conclusion

The Milestone 1 implementation satisfies all empirical challenge criteria:
1. **64x64 SoA Chunk**: Guaranteed 64-byte alignment on stack and heap, core arrays total exactly 32,768 bytes (32 KB, L1 data cache resident), and bitwise indexing is a verified collision-free bijection.
2. **Double-Update Prevention**: `FLAG_PARITY = 0x01` and bottom-to-top traversal prevent cascading falls in a single tick across both intra-chunk and inter-chunk movements.
3. **Continuous Brush**: Bresenham stroke interpolation guarantees 8-connected continuity across 1,000 random fast cursor jumps, and radii $R=1 \dots 50$ match the Euclidean disk oracle.
4. **Point Sampling**: Streaming texture uses `SDL_ScaleModeNearest` (`GL_NEAREST`).

**Verdict**: **APPROVE**.

---

## 5. Verification Method

To independently reproduce the empirical findings:

1. **Compile the Challenger 2 Stress Test Target**:
   ```powershell
   cmake --build build --config Release --target test_challenger2_stress
   ```
   *Expected: Clean compilation with 0 warnings and 0 errors.*

2. **Execute the Empirical Stress Test Binary**:
   ```powershell
   .\build\Release\test_challenger2_stress.exe
   ```
   *Expected Output: All 4 suites report `[PASS]` and conclude with `ALL EMPIRICAL CHALLENGER 2 STRESS TESTS PASSED (4/4 SUITES)`.*

3. **Execute Full CTest Suite**:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
   *Expected Output: 100% tests passed (7/7 tests passed).*

4. **Invalidation Conditions**:
   - Any stack or heap chunk with `reinterpret_cast<uintptr_t>(chunk) % 64 != 0`.
   - Core array size deviating from 32,768 bytes.
   - Any particle advancing $> 1$ cell in a single tick.
   - Any consecutive pair of Bresenham stroke cells with $\max(|\Delta x|, |\Delta y|) > 1$.
   - Any texture scale mode other than `SDL_ScaleModeNearest`.
