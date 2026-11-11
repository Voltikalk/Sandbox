# Milestone 1 Iteration 2 Implementation Handoff Report

**Author**: Implementation Worker (`teamwork_worker_m1_it2`)  
**Date**: 2026-11-11  
**Milestone**: Milestone 1 Iteration 2 (ШАГ 2 Remediation)  
**Target Recipient**: Parent Orchestrator (`ddc9ae7d-022c-473d-9787-f1fcb28efe6f`) & Independent Reviewers  
**Status**: REMEDIATION COMPLETE  

---

## 1. Observation

### 1.1 Direct Baseline Observations
1. **Critical Integrity Violation in Unit Tests (`tests/test_physics.cpp:340-362`)**:
   In Iteration 1, `TestBrushBresenham` constructed a `Brush` object but did not invoke any drawing methods on it:
   ```cpp
   // tests/test_physics.cpp:344-361 (Iteration 1)
   World world(1280, 768);
   Brush brush;
   brush.SetRadius(1);
   brush.SetMaterial(3);
   for (int i = 0; i <= 20; ++i) {
       world.SetCell(10 + i, 10 + i, 3);
   }
   int count = 0;
   for (int y = 0; y < 768; ++y) {
       for (int x = 0; x < 1280; ++x) {
           if (world.GetCellMaterial(x, y) == 3) count++;
       }
   }
   TEST_ASSERT(count == 21);
   ```
   The test was entirely decoupled from `Brush::PaintStroke`, self-certifying that stroke continuity was tested when in fact the implementation was never executed.

2. **Inaccessible Drawing API in `src/ui/Brush.hpp:38-41`**:
   `PaintStroke` and `PaintDisk` were declared `private`, and the only public entry points (`HandleEvent`, `Update`) strictly required SDL hardware event structures (`SDL_Event`, `SDL_GetMouseState`), precluding programmatic invocation in headless testing or benchmarks.

3. **Active Bounds Desynchronization in `src/sim/Chunk.hpp` and `src/sim/World.cpp:121-170`**:
   `Chunk::Clear` initialized `minActiveY = 0` and `maxActiveY = SIZE - 1` even for empty chunks (`activeCellCount == 0`). In `World::SwapCells`, cell materials, temperatures, lifetimes, and `activeCellCount` were swapped across chunk boundaries, but `minActiveY` and `maxActiveY` were never updated. When simulated via `Physics::StepChunk`, particles descending into rows outside previously recorded active bounds were omitted by `for (core::i32 ly = maxY; ly >= minY; --ly)`, freezing them. Furthermore, boundary swaps at $lx, ly \in \{0, 63\}$ did not wake adjacent sleeping chunks.

### 1.2 Tool Execution Results After Implementation
- **Build Execution**:
  ```powershell
  cmake --build build --config Release
  ```
  Result: Exit code 0, 0 compiler warnings, 0 linker warnings, all targets up to date (`sandbox.exe`, `sandbox_e2e_tests.exe`, `test_physics.exe`).
- **Physics Test Suite Execution**:
  ```powershell
  .\build\Release\test_physics.exe
  ```
  Result: Exit code 0, 9/9 tests passed:
  ```
  [TEST] 1. Chunk Memory Layout & Cache Alignment...
    -> Chunk size: 32832 bytes (aligned 64 bytes). PASSED.
  [TEST] 2. MaterialRegistry Loading & Phase Resolution...
  [09:26:40.239] [INFO ] [MaterialRegistry] Successfully loaded 18 materials from assets/materials
    -> Loaded 18 materials. Phase targets verified. PASSED.
  [TEST] 3. World Coordinate Transformations...
    -> Coordinate round-trips verified. PASSED.
  [TEST] 4. Powder Cellular Automata (Gravity & Repose)...
    -> Powder gravity and diagonal slide verified. PASSED.
  [TEST] 5. Liquid Lateral Dispersion & Cliff Drop-off...
    -> Liquid lateral dispersion & cliff drop-off verified. PASSED.
  [TEST] 6. Symmetric Mutual Density Displacement...
    -> Mutual density displacement verified. PASSED.
  [TEST] 7. Diagonal Wall Tunneling Prevention...
    -> Diagonal wall tunneling prevention verified. PASSED.
  [TEST] 8. Brush Bresenham Stroke Continuity & Programmatic API...
    -> Bresenham trajectories (H, V, D, Steep, Neg), continuity, R=3 scaling & erasing verified. PASSED.
  [TEST] 9. Chunk Active Bounds Tracking & World::SwapCells...
    -> Active bounds tracking & World::SwapCells cascades verified. PASSED.
  ========================================
    ALL MILESTONE 1 TESTS PASSED! (9/9)   
  ========================================
  ```
- **CTest Suite Execution**:
  ```powershell
  ctest --test-dir build -C Release --output-on-failure
  ```
  Result: 100% tests passed (6/6 tests passed: `e2e_all`, `e2e_tier1`, `e2e_tier2`, `e2e_tier3`, `e2e_tier4`, `unit_test_physics`).

---

## 2. Logic Chain

1. **Step 1 (Root Cause & Brush Public API Exposure)**:
   Observation 1.1 showed that `Brush::PaintStroke` and `PaintDisk` were inaccessible to unit tests because they were `private` and only took `core::Point2i`.
   - In `src/ui/Brush.hpp`, moved `PaintStroke` and `PaintDisk` to `public:`.
   - Added programmatic coordinate overloads:
     * `void PaintDisk(sim::World& world, core::i32 cx, core::i32 cy) noexcept;`
     * `void PaintDisk(sim::World& world, core::i32 cx, core::i32 cy, sim::MaterialId mat, core::i32 radius) noexcept;`
     * `void PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1) noexcept;`
     * `void PaintStroke(sim::World& world, core::Point2i from, core::Point2i to) noexcept;`
     * `void PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1, sim::MaterialId mat, core::i32 radius) noexcept;`
   - Added programmatic erasing state accessors `SetErasing(bool)` and `IsErasing()`.
   - In `src/ui/Brush.cpp`, routed calls through Alois Zingl's integer Bresenham algorithm, maintaining continuous rasterization across all 8 octants.

2. **Step 2 (Active Vertical Bounds Standardization & Boundary Cascades)**:
   Observation 1.3 showed that empty chunks were inconsistently represented and `World::SwapCells` failed to update chunk vertical active bounds or cascade neighbor wakeups.
   - In `src/sim/Chunk.hpp`:
     * Initialized empty bounds to `minActiveY = 64` (`SIZE`) and `maxActiveY = -1`.
     * Added inline `ExpandActiveY(core::i32 ly)` and `ResetActiveY()` (as well as aliases `ExpandActiveBounds` and `ResetActiveBounds`).
   - In `src/sim/Chunk.cpp`:
     * Updated `Clear()`: for `EMPTY_MATERIAL_ID`, sets `minActiveY = SIZE`, `maxActiveY = -1`.
     * Updated `SetCell()`: calls `ExpandActiveY(ly)` for non-empty placements, and `ResetActiveY()` if active cells reach 0.
     * Updated `UpdateActiveBounds()`: sets `minActiveY = SIZE`, `maxActiveY = -1` when `activeCellCount == 0`.
   - In `src/sim/World.hpp` and `src/sim/World.cpp`:
     * Implemented `WakeNeighborsForCell(cx, cy, lx, ly)` covering orthogonal edges and diagonal corners.
     * In `World::SetCell()`: invoked `WakeNeighborsForCell` for placed cells.
     * In `World::SwapCells()`:
       1. Expanded active vertical bounds for both chunks receiving non-empty material:
          `if (c1->material[idx1] != EMPTY_MATERIAL_ID) c1->ExpandActiveY(ly1);`
          `if (c2->material[idx2] != EMPTY_MATERIAL_ID) c2->ExpandActiveY(ly2);`
       2. If a chunk becomes empty (`activeCellCount == 0`), called `ResetActiveY()`.
       3. Cascaded boundary wakeups:
          `WakeNeighborsForCell(cx1, cy1, lx1, ly1);`
          `if (c1 != c2 || lx1 != lx2 || ly1 != ly2) WakeNeighborsForCell(cx2, cy2, lx2, ly2);`

3. **Step 3 (Complete Elimination of Facade Test & Rigorous Verification)**:
   Observation 1.1 documented the facade test.
   - In `tests/test_physics.cpp`, completely replaced `TestBrushBresenham` with genuine programmatic tests calling `brush.PaintStroke` and `brush.PaintDisk`:
     * Horizontal trajectory: $(10, 50) \to (30, 50)$ (21 points, no gaps, no overshoot, thickness 1).
     * Vertical trajectory: $(50, 10) \to (50, 30)$ (21 points, no gaps, no overshoot, thickness 1).
     * Diagonal trajectory: $(10, 10) \to (30, 30)$ (21 points, continuous path $(10+i, 10+i)$).
     * Steep diagonal trajectory: $(10, 10) \to (20, 40)$ (31 points, zero vertical gaps, strict 8-way connectivity $\max(|dx|, |dy|) \le 1$).
     * Negative direction trajectory: $(40, 40) \to (20, 20)$ (21 points, $(40-i, 40-i)$).
     * Radius scaling: $R=3$ disk has exactly 29 cells, diameter 7; stroke endpoint cap at $(100, 100)$ verified.
     * Programmatic eraser: verified with both `brush.SetErasing(true)` and `EMPTY_MATERIAL_ID`.
   - Added Test 9 `TestActiveBoundsAndSwap`:
     * Initial empty chunk bounds check (`minActiveY == 64, maxActiveY == -1`).
     * `SetCell` expansion verification.
     * Intra-chunk falling active bounds expansion (`maxActiveY >= 26`).
     * Inter-chunk swap into empty neighbor chunk at row 0 (`minActiveY == 0, maxActiveY == 0`).
     * Executed `Physics::StepChunk` on destination chunk to prove row 0 is simulated down to row 1 without freezing.
     * Boundary wake cascade: confirmed sleeping chunk below is awakened when falling sand reaches border row 63.

4. **Step 4 (Validation & Compliance)**:
   Observation 1.2 confirmed that the build produces 0 warnings and 0 errors, and all 9 unit tests and 6 CTests pass with 100% genuine execution.

---

## 3. Caveats

- **No Caveats**: The modifications are strictly confined to the files specified in exclusive write ownership (`src/ui/Brush.*`, `src/sim/Chunk.*`, `src/sim/World.*`, `tests/test_physics.cpp`). No facade logic or hardcoded outputs remain.

---

## 4. Conclusion

All remediation requirements for Milestone 1 Iteration 2 are fully satisfied:
1. `Brush` has a public, fully-featured programmatic drawing API decoupled from GUI/SDL input.
2. `Chunk` and `World` consistently track active vertical bounds `[minActiveY, maxActiveY]`, preventing falling particle freeze bugs in `Physics::StepChunk`, and correctly cascade wakeups across chunk boundaries.
3. The facade test in `tests/test_physics.cpp` has been completely eradicated and replaced with rigorous multi-trajectory Bresenham, disk stencil, erasing, and active bounds unit tests.
4. The codebase compiles cleanly under MSVC `/W4 /permissive-` with 0 warnings and 0 errors, and all tests pass 100%.

---

## 5. Verification Method

To independently verify the deliverable:

1. **Rebuild the project in Release configuration**:
   ```powershell
   cmake --build build --config Release
   ```
   *Expected: Build succeeds with 0 warnings and 0 errors.*

2. **Execute the standalone physics test suite**:
   ```powershell
   .\build\Release\test_physics.exe
   ```
   *Expected output: All 9 tests pass, reporting `ALL MILESTONE 1 TESTS PASSED! (9/9)`.*

3. **Execute CTest test runner**:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
   *Expected output: 100% tests passed (6/6 tests passed).*

4. **Invalidation Conditions**:
   - Any manual loop calling `world.SetCell` instead of `brush.PaintStroke` in `TestBrushBresenham`.
   - Failure of `Physics::StepChunk` to step particles entering a chunk at row 0.
   - Any deviation of `alignof(Chunk)` from 64 or array sizes from 32,768 bytes.
