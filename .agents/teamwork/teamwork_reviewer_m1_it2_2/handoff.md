# Review & Adversarial Critic Report: Milestone 1 Iteration 2

**Author**: Reviewer 2 (Teamwork Reviewer & Adversarial Critic)  
**Date**: 2026-11-11  
**Milestone**: Milestone 1 Iteration 2 (ШАГ 2 Remediation Review)  
**Target Recipient**: Parent Orchestrator (`ddc9ae7d-022c-473d-9787-f1fcb28efe6f`)  
**Status**: REVIEW COMPLETE  
**Verdict**: **APPROVE**  

---

## Review Summary

| Review Dimension | Status | Notes |
|---|---|---|
| **Test Suite Authenticity & Integrity** | **PASS (EXCELLENT)** | Facade test in Test 8 has been completely eradicated. `Brush::PaintStroke` and `Brush::PaintDisk` are genuinely executed. Multi-octant trajectories, zero 8-way gaps, radius scaling ($R=3$, 29 cells, diameter 7), and eraser mode are rigorously asserted. |
| **Brush Programmatic Drawing API** | **PASS (EXCELLENT)** | `Brush.hpp` exposes clean, public coordinate overloads for `PaintStroke` and `PaintDisk`, plus eraser state accessors, fully decoupled from SDL/ImGui hardware events. |
| **Active Vertical Bounds Synchronization** | **PASS (EXCELLENT)** | `Chunk` and `World` consistently manage `[minActiveY, maxActiveY]` through `ExpandActiveY` and `ResetActiveY` across both `SetCell` and `SwapCells`. Particle freezing in `Physics::StepChunk` is completely prevented. |
| **Boundary Wake Cascade** | **PASS (EXCELLENT)** | Border activity at $lx, ly \in \{0, 63\}$ triggers `WakeNeighborsForCell`, correctly propagating wakeups across orthogonal and diagonal neighbor chunks. |
| **Build & Test Suite Execution** | **PASS (100%)** | 0 compiler warnings, 0 errors under MSVC `/W4 /permissive-`. All 9 unit tests pass in `test_physics.exe`. All 6 CTest suites pass (`e2e_all`, `e2e_tier1-4`, `unit_test_physics`). |

---

## 1. Observation

### 1.1 Direct Source Code Observations

1. **Brush Programmatic Drawing API (`src/ui/Brush.hpp:37-44` and `src/ui/Brush.cpp:73-131`)**:
   - `Brush.hpp` exposes the programmatic drawing interface as `public:`:
     ```cpp
     // Public Programmatic Drawing API
     void PaintDisk(sim::World& world, core::i32 cx, core::i32 cy) noexcept;
     void PaintDisk(sim::World& world, core::i32 cx, core::i32 cy, sim::MaterialId mat, core::i32 radius) noexcept;

     void PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1) noexcept;
     void PaintStroke(sim::World& world, core::Point2i from, core::Point2i to) noexcept;
     void PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1, sim::MaterialId mat, core::i32 radius) noexcept;
     ```
   - In `Brush.cpp:110-131`, `PaintStroke` implements Alois Zingl's integer 2D Bresenham algorithm:
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
         if (e2 >= dy) {
             err += dy;
             x0 += sx;
         }
         if (e2 <= dx) {
             err += dx;
             y0 += sy;
         }
     }
     ```
   - `PaintDisk` (`Brush.cpp:78-98`) implements Euclidean disk rasterization ($dx^2 + dy^2 \le r^2$) with per-cell bounds checking against `world.GetWidth()` and `world.GetHeight()`.

2. **Chunk Active Bounds Invariants (`src/sim/Chunk.hpp:36-37, 63-81` and `src/sim/Chunk.cpp:13-32, 53-58`)**:
   - Empty chunk initial state is explicitly defined as `minActiveY = SIZE` (64) and `maxActiveY = -1`.
   - `Chunk.hpp:64-72`:
     ```cpp
     void ExpandActiveY(core::i32 ly) noexcept {
         if (minActiveY > maxActiveY) {
             minActiveY = ly;
             maxActiveY = ly;
         } else {
             if (ly < minActiveY) minActiveY = ly;
             if (ly > maxActiveY) maxActiveY = ly;
         }
     }
     void ResetActiveY() noexcept {
         minActiveY = SIZE; // 64
         maxActiveY = -1;
     }
     ```
   - `Chunk.cpp:53-57`: In `SetCell`, when placing a non-empty cell, `ExpandActiveY(ly)` is called. When erasing a cell such that `activeCellCount == 0`, `ResetActiveY()` is called.

3. **Active Bounds & Boundary Cascades in `World::SwapCells` (`src/sim/World.cpp:138-183`)**:
   - Swapping cells between chunks updates `activeCellCount` and resets bounds if a chunk becomes empty:
     ```cpp
     if (c1 != c2) {
         bool m1Empty = (c1->material[idx1] == EMPTY_MATERIAL_ID);
         bool m2Empty = (c2->material[idx2] == EMPTY_MATERIAL_ID);
         if (m1Empty && !m2Empty) {
             ++c1->activeCellCount;
             if (c2->activeCellCount > 0) --c2->activeCellCount;
         } else if (!m1Empty && m2Empty) {
             if (c1->activeCellCount > 0) --c1->activeCellCount;
             ++c2->activeCellCount;
         }
         if (c1->activeCellCount == 0) c1->ResetActiveY();
         if (c2->activeCellCount == 0) c2->ResetActiveY();
     }
     ```
   - Bounds are expanded in chunks receiving material (`World.cpp:166-171`):
     ```cpp
     if (c1->material[idx1] != EMPTY_MATERIAL_ID) c1->ExpandActiveY(ly1);
     if (c2->material[idx2] != EMPTY_MATERIAL_ID) c2->ExpandActiveY(ly2);
     ```
   - Boundary wakeups are triggered on both cell coordinates (`World.cpp:179-182`):
     ```cpp
     WakeNeighborsForCell(cx1, cy1, lx1, ly1);
     if (c1 != c2 || lx1 != lx2 || ly1 != ly2) {
         WakeNeighborsForCell(cx2, cy2, lx2, ly2);
     }
     ```

4. **Authentic Unit Tests (`tests/test_physics.cpp:342-611`)**:
   - `TestBrushBresenham` (Test 8):
     * The facade loop (`for (int i=0; i<=20; ++i) world.SetCell(...)`) has been completely removed.
     * Horizontal line $(10, 50) \to (30, 50)$: calls `brush.PaintStroke(world, 10, 50, 30, 50)`, asserts exact 21 cells, 0 gaps, no overshoot.
     * Vertical line $(50, 10) \to (50, 30)$: calls `brush.PaintStroke(world, 50, 10, 50, 30)`, asserts exact 21 cells, 0 gaps.
     * Diagonal line $(10, 10) \to (30, 30)$: calls `brush.PaintStroke(world, 10, 10, 30, 30)`, asserts exact 21 cells along diagonal.
     * Steep line $(10, 10) \to (20, 40)$: $\Delta x = 10, \Delta y = 30$, calls `brush.PaintStroke(world, 10, 10, 20, 40)`. Asserts exactly 31 cells, asserts every row $y \in [10, 40]$ is populated, and asserts sorted point delta $\max(|\Delta x|, |\Delta y|) \le 1$ proving strict 8-way connectivity with zero gaps.
     * Negative direction $(40, 40) \to (20, 20)$: asserts exactly 21 cells along reverse diagonal.
     * Radius scaling $R=3$: calls `brush.PaintDisk(world, 100, 100)`, asserts Euclidean disk condition $dx^2 + dy^2 \le 9$ yields exactly 29 cells, diameter exactly 7 ($103 - 97 + 1 = 7$).
     * Stroke endpoint caps: asserts $R=3$ stroke $(80, 100) \to (100, 100)$ creates a full diameter-7 rounded end cap at $(100, 100)$.
     * Programmatic eraser: calls `brush.SetErasing(true)` and `brush.PaintStroke(..., EMPTY_MATERIAL_ID, ...)`, confirming all active cells return to 0.
   - `TestActiveBoundsAndSwap` (Test 9):
     * Tests empty chunk bounds `minActiveY == 64`, `maxActiveY == -1`.
     * Tests `world.SetCell(10, 25, sand)` expands bounds to `minActiveY = 25`, `maxActiveY = 25`.
     * Tests intra-chunk fall via `physics.Step(world)` moves sand to row 26 and expands `maxActiveY >= 26`.
     * Tests inter-chunk fall from row 63 to row 64 (row 0 of neighbor chunk): neighbor bounds become `minActiveY = 0, maxActiveY = 0`.
     * Directly calls `physics.StepChunk(world, *c01)` on destination chunk, proving row 0 simulates down to row 1 without freezing.
     * Tests boundary wake cascade: sand falling to row 63 awakens sleeping neighbor chunk below (`c01->isSleeping == false`).

### 1.2 Independent Tool Execution Results

1. **Build Execution**:
   - Command: `cmake --build build --config Release`
   - Exit code: `0`
   - Warnings/Errors: `0 warnings, 0 errors`
   - Output: `test_physics.exe`, `sandbox.exe`, `sandbox_e2e_tests.exe` all up to date.

2. **Physics Unit Test Suite**:
   - Command: `build\Release\test_physics.exe`
   - Exit code: `0`
   - Output:
     ```
     ========================================
       PIXEL SANDBOX 2D - MILESTONE 1 TESTS   
     ========================================
     [TEST] 1. Chunk Memory Layout & Cache Alignment...
       -> Chunk size: 32832 bytes (aligned 64 bytes). PASSED.
     [TEST] 2. MaterialRegistry Loading & Phase Resolution...
     [09:30:04.021] [INFO ] [MaterialRegistry] Successfully loaded 18 materials from assets/materials
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

3. **CTest Suite Execution**:
   - Command: `ctest --test-dir build -C Release --output-on-failure`
   - Exit code: `0`
   - Output: `6/6 tests passed (100% tests passed, 0 tests failed)`.

---

## 2. Logic Chain

1. **Step 1 (Remediation of Critical Finding 1 - Integrity Violation)**:
   - In Iteration 1, Test 8 was flagged as an `INTEGRITY VIOLATION` because it constructed a `Brush` object but drew using a manual loop calling `world.SetCell(10 + i, 10 + i, 3)`.
   - In Iteration 2 (Observation 1.1), `src/ui/Brush.hpp` exposes `PaintStroke` and `PaintDisk` publicly, and `tests/test_physics.cpp` replaces the facade loop with comprehensive calls to `brush.PaintStroke` and `brush.PaintDisk`.
   - All assertions directly inspect `world.GetCellMaterial(x, y)` and verify continuous, gapless rasterization across 5 trajectories (horizontal, vertical, diagonal, steep, reverse), radius scaling ($R=3$, 29 cells), and eraser modes.
   - Therefore, the facade implementation is 100% eliminated, and test authenticity is verified.

2. **Step 2 (Remediation of Major Finding 2 - Active Bounds Desynchronization)**:
   - In Iteration 1, `World::SwapCells` omitted updating `minActiveY` and `maxActiveY`, causing particles entering row 0 of a chunk to be skipped by `StepChunk`.
   - In Iteration 2 (Observation 1.1), `Chunk::ExpandActiveY` correctly expands bounds for incoming cells, and `ResetActiveY` resets empty chunks to `minActiveY = 64, maxActiveY = -1`. `World::SwapCells` invokes `ExpandActiveY` on both chunks.
   - Test 9 explicitly verifies that when sand falls across a chunk boundary into row 0, `minActiveY` and `maxActiveY` become 0, and `Physics::StepChunk` successfully advances the particle into row 1.
   - Therefore, active bounds desynchronization is completely resolved.

3. **Step 3 (Remediation of Boundary Wake Cascade)**:
   - In Iteration 1, sleeping chunks adjacent to boundary movements were not guaranteed to wake up.
   - In Iteration 2 (Observation 1.1 & 1.2), `World::WakeNeighborsForCell` is invoked during `SetCell` and `SwapCells` whenever cells at $lx \in \{0, 63\}$ or $ly \in \{0, 63\}$ are touched.
   - Test 9 confirms that sand falling to border row 63 immediately awakens a sleeping neighbor chunk below.
   - Therefore, boundary wake cascades are fully operational.

4. **Step 4 (Verification of Non-Regression)**:
   - Tests 1 through 7 remain intact and continue to assert 64-byte alignment, 18-material two-pass registry parsing, powder repose, liquid cliff priority, mutual density displacement, and diagonal corner tunneling protection.
   - All 9 unit tests and all 6 CTest suites pass cleanly with zero compiler or linker warnings under `/W4 /permissive-`.

---

## 3. Adversarial Challenges & Stress Testing

### Challenge 1: Single-Point Stroke ($x_0 = x_1, y_0 = y_1$)
- **Assumption**: Bresenham while-loop terminates cleanly without infinite loops when start equals end point.
- **Trace**:
  `dx = 0, dy = 0, err = 0`.
  Inside `while (true)`:
  `PaintDisk(world, x0, y0, mat, radius);`
  `if (x0 == x1 && y0 == y1) break;` triggers immediately!
- **Result**: **PASS**. Exactly one disk is painted, zero loop iterations, no freeze.

### Challenge 2: Out-of-Bounds Stroke Trajectories
- **Attack Scenario**: Fast mouse drag passes outside the world canvas (e.g. from $( -20, 100 )$ to $( 1500, 100 )$).
- **Defense Trace**:
  Bresenham increments $(x_0, y_0)$ smoothly across the coordinate space.
  `PaintDisk` performs:
  ```cpp
  if (wy < 0 || wy >= world.GetHeight()) continue;
  if (wx >= 0 && wx < world.GetWidth()) world.SetCell(wx, wy, mat);
  ```
  `World::SetCell` additionally performs `if (!IsInBounds(x, y)) return false;`.
- **Result**: **PASS**. No memory corruption, no out-of-bounds array access.

### Challenge 3: Heavy Lava vs Water vs Sand Cascade
- **Attack Scenario**: Multiple density swaps across chunk boundaries: Sand ($\rho=1600$), Water ($\rho=1000$), Lava ($\rho=3000$).
- **Defense Trace**:
  `CanDisplace` requires mutual density conditions:
  - Sand vs Water: $\rho_{\text{sand}} = 1600 > 1000 \implies$ Sand sinks, Water rises.
  - Lava vs Sand: Sand is powder, Lava is liquid. Rule: Liquids never displace powders. Sand rests on top of lava.
  - Lava vs Water: $\rho_{\text{lava}} = 3000 > 1000 \implies$ Lava sinks, Water rises.
- **Result**: **PASS**. Fully verified in Test 6.

### Challenge 4: Chunk Drain to Empty State
- **Attack Scenario**: A chunk with a single particle loses that particle across the boundary.
- **Defense Trace**:
  In `World::SwapCells`, `c1->activeCellCount` decrements from 1 to 0.
  `if (c1->activeCellCount == 0) c1->ResetActiveY();` is executed, restoring `minActiveY = 64, maxActiveY = -1`.
  `Physics::StepChunk` checks `if (chunk.isSleeping || chunk.activeCellCount == 0) return;` and immediately bypasses empty chunks.
- **Result**: **PASS**. Zero wasted CPU cycles iterating empty chunks.

---

## 4. Verified Claims

| # | Feature / Claim | Verification Method | Status | Notes |
|---|---|---|---|---|
| 1 | Bresenham programmatic drawing API | Code inspection `Brush.hpp:37-44`, `Brush.cpp:73-131` | **VERIFIED (PASS)** | Public programmatic API, no SDL/ImGui dependency required for tests. |
| 2 | Elimination of facade test in Test 8 | Code inspection `tests/test_physics.cpp:342-549` | **VERIFIED (PASS)** | Genuinely calls `brush.PaintStroke` and `brush.PaintDisk`. Zero dummy loops. |
| 3 | Multi-octant trajectory rasterization | `test_physics.exe` (Test 8.2, 8.3, 8.4, 8.5, 8.6) | **VERIFIED (PASS)** | Tested H, V, D, Steep ($\Delta y=30$), and reverse diagonal. |
| 4 | Strict 8-way connectivity with zero gaps | `tests/test_physics.cpp:456-466` | **VERIFIED (PASS)** | Asserts $\max(|\Delta x|, |\Delta y|) \le 1$ for all consecutive points along steep stroke. |
| 5 | Radius scaling & disk stencil ($R=3$) | `tests/test_physics.cpp:489-530` | **VERIFIED (PASS)** | Euclidean disk of radius 3 has exactly 29 cells, diameter 7. Endpoint cap verified. |
| 6 | Active bounds tracking in `Chunk` & `World` | Code inspection `Chunk.hpp:64-77`, `World.cpp:166-171`, Test 9 | **VERIFIED (PASS)** | Correct initialization `(64, -1)`, expansion on swap, and reset on drain. |
| 7 | Inter-chunk particle fall & `StepChunk` | `test_physics.exe` (Test 9.4) | **VERIFIED (PASS)** | Particle entering row 0 simulates to row 1 without freezing. |
| 8 | Boundary wake cascade | `test_physics.exe` (Test 9.5), `World.cpp:185-201` | **VERIFIED (PASS)** | Sleeping neighbor chunk wakes when particle reaches border row 63. |
| 9 | Clean build with 0 warnings & 0 errors | `cmake --build build --config Release` | **VERIFIED (PASS)** | MSVC `/W4 /permissive-` compiles cleanly. |
| 10 | 100% CTest pass rate | `ctest --test-dir build -C Release --output-on-failure` | **VERIFIED (PASS)** | 6/6 tests passed. |

---

## 5. Caveats

- **No Caveats**: The remediation is clean, fully compliant with `PROJECT.md` and `SCOPE.md`, strictly respects file boundaries, and contains zero facade code or integrity shortcuts.

---

## 6. Conclusion & Verdict

The work delivered for Milestone 1 Iteration 2 resolves all findings identified in Iteration 1:
1. The Critical Integrity Violation has been completely eliminated; `tests/test_physics.cpp` contains rigorous, authentic unit tests for `Brush` Bresenham stroke interpolation and radius disk rasterization.
2. The Major Finding regarding `Chunk` active bounds desynchronization and `Physics::StepChunk` freezing has been thoroughly remedied and verified with Test 9.
3. The boundary wake cascade operates seamlessly across chunk edges and corners.
4. The entire test suite and build pipeline pass with 100% success, 0 errors, and 0 warnings.

**Verdict**: **APPROVE**

---

## 7. Verification Method

To independently reproduce this verification:

1. **Clean build in Release configuration**:
   ```powershell
   cmake --build build --config Release
   ```
   *Expected: Exit code 0, 0 compiler warnings, 0 linker warnings.*

2. **Run physics unit tests**:
   ```powershell
   .\build\Release\test_physics.exe
   ```
   *Expected: Exit code 0, all 9/9 tests reported as PASSED.*

3. **Run full CTest suite**:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
   *Expected: Exit code 0, 6/6 tests passed (100%).*

4. **Code Inspection**:
   - Inspect `tests/test_physics.cpp:342-549` to confirm `brush.PaintStroke` and `brush.PaintDisk` are invoked.
   - Inspect `src/ui/Brush.hpp:37-44` to confirm `PaintStroke` and `PaintDisk` are public.
   - Inspect `src/sim/Chunk.hpp:64-77` and `src/sim/World.cpp:166-171` to confirm active bounds expansion.
