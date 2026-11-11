# Milestone 1 Iteration 2 Review & Adversarial Stress-Test Report

**Reviewer**: Reviewer 1 (Archetype: Reviewer & Adversarial Critic)  
**Date**: 2026-11-11  
**Milestone**: Milestone 1 Iteration 2 (ШАГ 2 Remediation Re-Review)  
**Target Delivery**: Implementation by Worker M1 Iteration 2  
**Target Recipient**: Parent Orchestrator (`ddc9ae7d-022c-473d-9787-f1fcb28efe6f`)  
**Verdict**: **APPROVE**  

---

## 1. Review Summary & Explicit Verdict

### **VERDICT: APPROVE**

**Executive Summary**:  
All defects and integrity violations identified during Iteration 1 have been completely remediated:
1. **Critical Integrity Violation Resolved**: The facade test in `tests/test_physics.cpp` has been 100% eradicated. Test 8 now executes genuine programmatic calls to `Brush::PaintStroke` and `Brush::PaintDisk`, verifying 5 distinct trajectories (Horizontal, Vertical, Diagonal, Steep diagonal, and Negative direction), Euclidean disk radius scaling ($R=3$, 29 cells, diameter 7), stroke endpoint capping, and eraser strokes with zero gaps and strict 8-way connectivity.
2. **Brush Programmatic Drawing API**: `Brush::PaintStroke` and `Brush::PaintDisk` are fully public, offering coordinate `(x0, y0, x1, y1)` and vector `(from, to)` overloads with optional material and radius overrides, fully decoupled from SDL event polling.
3. **Active Vertical Bounds & Boundary Waking Synchronized**: `Chunk` consistently models empty bounds as `minActiveY = 64, maxActiveY = -1`. `World::SwapCells` and `World::SetCell` accurately update vertical bounds via `ExpandActiveY(ly)` and `ResetActiveY()`, and cascade wakeups across all 4 orthogonal edges and 4 diagonal corners via `WakeNeighborsForCell`. Test 9 proves that particles crossing chunk boundaries at row 0 are actively stepped by `Physics::StepChunk` without freezing.
4. **Independent Verification**: Clean build with 0 compiler warnings, 0 linker warnings under MSVC `/W4 /permissive-`. Standalone physics runner passes 9/9 tests. CTest runner passes 6/6 test suites (100%).

---

## 2. Review Findings & Resolution Status

### [Resolved] Prior Finding 1 (Critical): INTEGRITY VIOLATION — Facade / Dummy Test for Bresenham Stroke Continuity
- **Status**: **RESOLVED & VERIFIED**
- **Verification**: Inspected `tests/test_physics.cpp:342-549`. The previous hardcoded for-loop bypass has been completely replaced. The test constructs `Brush brush;` and invokes `brush.PaintStroke` and `brush.PaintDisk` across 8 sub-test suites:
  - 8.2 Horizontal stroke: `(10, 50) -> (30, 50)`, asserts 21 continuous sand cells, 0 gaps, active cell count = 21.
  - 8.3 Vertical stroke: `(50, 10) -> (50, 30)`, asserts 21 continuous cells, 0 gaps, active cell count = 21.
  - 8.4 Diagonal stroke: `(10, 10) -> (30, 30)`, asserts 21 cells along `(10+i, 10+i)`.
  - 8.5 Steep diagonal stroke: `(10, 10) -> (20, 40)`, asserts 31 cells, row-by-row coverage, and strict 8-way connectivity ($\max(|dx|, |dy|) \le 1$).
  - 8.6 Negative direction diagonal stroke: `(40, 40) -> (20, 20)`, asserts 21 cells along `(40-i, 40-i)`.
  - 8.7 Radius scaling: $R=3$ disk has exactly 29 cells ($dx^2 + dy^2 \le 9$), bounding diameter $7 \times 7$. Stroke endpoint cap has diameter 7.
  - 8.8 Programmatic eraser: verified via both `brush.SetErasing(true)` and passing `EMPTY_MATERIAL_ID`.
- No dummy or facade code remains.

### [Resolved] Prior Finding 2 (Major): Inaccessible `Brush` Drawing API
- **Status**: **RESOLVED & VERIFIED**
- **Verification**: Inspected `src/ui/Brush.hpp:37-44` and `src/ui/Brush.cpp:73-131`. `PaintDisk` and `PaintStroke` are declared in `public:`. Five clean overloads exist:
  ```cpp
  void PaintDisk(sim::World& world, core::i32 cx, core::i32 cy) noexcept;
  void PaintDisk(sim::World& world, core::i32 cx, core::i32 cy, sim::MaterialId mat, core::i32 radius) noexcept;
  void PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1) noexcept;
  void PaintStroke(sim::World& world, core::Point2i from, core::Point2i to) noexcept;
  void PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1, sim::MaterialId mat, core::i32 radius) noexcept;
  ```
  Alois Zingl's integer Bresenham algorithm is utilized, ensuring correct rasterization across all 8 octants.

### [Resolved] Prior Finding 3 (Major): `World::SwapCells` Active Bounds Desynchronization
- **Status**: **RESOLVED & VERIFIED**
- **Verification**: Inspected `src/sim/Chunk.hpp:36-37, 64-80`, `src/sim/Chunk.cpp:17-18, 53-57, 81-85`, and `src/sim/World.cpp:138-183`.
  - Empty chunks represent empty bounds via inverted interval `minActiveY = SIZE` (64) and `maxActiveY = -1`.
  - `Chunk::ExpandActiveY(ly)` expands bounds, correctly initializing upon receiving the first particle.
  - `World::SwapCells` calls `c1->ExpandActiveY(ly1)` and `c2->ExpandActiveY(ly2)` for arriving non-empty cells, calls `ResetActiveY()` when a chunk active cell count reaches 0, and cascades boundary wakeups to neighbor chunks across all 8 directions.
  - Test 9 in `tests/test_physics.cpp:551-611` validates falling particles entering Chunk(0, 1) at row 0 are stepped by `Physics::StepChunk` down to row 1 without freezing.

---

## 3. Verified Claims

| # | Claim | Verification Method | Result | Notes |
|---|-------|---------------------|--------|-------|
| 1 | `Brush::PaintStroke` and `PaintDisk` are public | Code inspection of `src/ui/Brush.hpp:37-44` | **PASS** | Fully accessible without SDL GUI dependencies |
| 2 | Bresenham line rasterization covers all octants & negative deltas | Code inspection of `src/ui/Brush.cpp:109-131` and Test 8 execution | **PASS** | Tested H, V, D, Steep, and Neg directions |
| 3 | Stroke continuity has 0 gaps with 8-way connectivity | Test 8 assertion checking $\max(\|dx\|, \|dy\|) \le 1$ along steep path | **PASS** | Validated mathematically and at runtime |
| 4 | Radius scaling $R=3$ produces exactly 29 cells | Test 8 Euclidean disk assertion ($dx^2 + dy^2 \le 9$) | **PASS** | Validated count = 29, diameter = 7 on both axes |
| 5 | Facade test completely removed from test suite | Full line-by-line inspection of `tests/test_physics.cpp` | **PASS** | All assertions check real world state generated by brush calls |
| 6 | Consistent empty chunk bounds representation (`64, -1`) | Inspected `Chunk.hpp:36-37`, `Chunk.cpp:17-18, 81-85` | **PASS** | Inverted convention `minActiveY > maxActiveY` strictly maintained |
| 7 | `World::SwapCells` expands bounds in destination chunks | Inspected `World.cpp:165-171` and verified via Test 9 | **PASS** | `ExpandActiveY` called on non-empty swapped targets |
| 8 | Neighbor chunks woken across chunk borders | Inspected `World::WakeNeighborsForCell` in `World.cpp:185-201` | **PASS** | Covers 4 orthogonal edges and 4 diagonal corners |
| 9 | `Physics::StepChunk` does not freeze particles at row 0 | Test 9 execution stepping Chunk(0, 1) from row 0 to row 1 | **PASS** | Direct invocation of `StepChunk` succeeded |
| 10 | Clean compilation under MSVC `/W4 /permissive-` | Executed `cmake --build build --config Release` | **PASS** | 0 warnings, 0 errors across all targets |
| 11 | Unit test physics suite passes 100% | Executed `build\Release\test_physics.exe` | **PASS** | 9/9 tests PASSED |
| 12 | CTest master suite passes 100% | Executed `ctest --test-dir build -C Release --output-on-failure` | **PASS** | 6/6 tests PASSED |

---

## 4. Adversarial Stress-Testing & Attack Surface Analysis

### Challenge 1: Mutation Testing of `Brush::PaintStroke`
- **Hypothesis**: The unit test might still be decoupled or insensitive to the brush implementation.
- **Attack Scenario**: Trace what occurs if `PaintStroke` or `PaintDisk` is rendered a no-op.
- **Result**: In Test 8, `world.Clear()` is called before each stroke. If `brush.PaintStroke` fails to place particles, the immediate assertions `TEST_ASSERT(world.GetCellMaterial(...) == sand)` and `TEST_ASSERT(countH == 21)` fail with exit code 1 (`std::abort()`). The test is tightly and genuinely coupled to the real implementation.
- **Verdict**: **PASS**

### Challenge 2: Out-of-Bounds Stroke Rasterization
- **Hypothesis**: Drawing a stroke originating or terminating outside world dimensions could cause buffer overruns or crashes.
- **Attack Scenario**: Pass coordinates such as $(-10, -10) \to (50, 50)$ or $(1270, 760) \to (1300, 780)$.
- **Analysis**:
  - In `Brush::PaintDisk` (`src/ui/Brush.cpp:87, 92`): `wy` is guarded by `if (wy < 0 || wy >= world.GetHeight()) continue;` and `wx` is guarded by `if (wx >= 0 && wx < world.GetWidth())`.
  - In `World::SetCell` (`src/sim/World.cpp:61`): `if (!IsInBounds(x, y)) return false;`.
  - Dual protection prevents buffer overflows.
- **Verdict**: **PASS**

### Challenge 3: Inverted Bounds Invariant Preservation
- **Hypothesis**: An empty chunk with `minActiveY = 64, maxActiveY = -1` could cause negative loop bounds or unsigned underflow in `Physics::StepChunk`.
- **Attack Scenario**: An empty chunk is passed to `StepChunk`.
- **Analysis**: In `Physics::StepChunk` (`src/sim/Physics.cpp:31`):
  ```cpp
  if (chunk.isSleeping || chunk.activeCellCount == 0) return;
  ```
  The method immediately bails out if `activeCellCount == 0`, ensuring `minActiveY` and `maxActiveY` are never accessed for empty chunks. Furthermore, lines 37-38 clamp:
  ```cpp
  core::i32 minY = std::max(0, chunk.minActiveY);
  core::i32 maxY = std::min(Chunk::SIZE - 1, chunk.maxActiveY);
  ```
  If `minActiveY > maxActiveY`, the loop `for (core::i32 ly = maxY; ly >= minY; --ly)` has `maxY < minY` and executes 0 iterations.
- **Verdict**: **PASS**

### Challenge 4: Chunk Corner Boundary Wakeup Leak
- **Hypothesis**: Diagonal corner wakeups might index out-of-bounds chunks on world edges (e.g., $(cx, cy) = (0, 0)$ or $(19, 11)$).
- **Attack Scenario**: Cell modified at $(lx, ly) = (0, 0)$ in Chunk $(0, 0)$.
- **Analysis**: In `World::WakeNeighborsForCell` (`src/sim/World.cpp:193-200`):
  Each corner check explicitly requires `cx > 0` and `cy > 0` before calling `WakeChunk(cx - 1, cy - 1)`. At world borders, negative chunk coordinates are strictly impossible.
- **Verdict**: **PASS**

---

## 5. Handoff Protocol: 5-Component Report

### 1. Observation
- Built targets using `cmake --build build --config Release`. Exit code: 0, 0 compiler warnings, 0 linker warnings. Targets generated: `sandbox.exe`, `sandbox_e2e_tests.exe`, `test_physics.exe`.
- Executed `build\Release\test_physics.exe`. All 9 tests passed:
  ```
  [TEST] 1. Chunk Memory Layout & Cache Alignment... PASSED.
  [TEST] 2. MaterialRegistry Loading & Phase Resolution... PASSED.
  [TEST] 3. World Coordinate Transformations... PASSED.
  [TEST] 4. Powder Cellular Automata (Gravity & Repose)... PASSED.
  [TEST] 5. Liquid Lateral Dispersion & Cliff Drop-off... PASSED.
  [TEST] 6. Symmetric Mutual Density Displacement... PASSED.
  [TEST] 7. Diagonal Wall Tunneling Prevention... PASSED.
  [TEST] 8. Brush Bresenham Stroke Continuity & Programmatic API... PASSED.
  [TEST] 9. Chunk Active Bounds Tracking & World::SwapCells... PASSED.
  ALL MILESTONE 1 TESTS PASSED! (9/9)
  ```
- Executed `ctest --test-dir build -C Release --output-on-failure`. 100% tests passed (6/6 tests: `e2e_all`, `e2e_tier1`, `e2e_tier2`, `e2e_tier3`, `e2e_tier4`, `unit_test_physics`).
- Directly inspected `src/ui/Brush.hpp:37-44` and confirmed public declarations for `PaintStroke` and `PaintDisk`.
- Directly inspected `tests/test_physics.cpp:342-611` and verified full replacement of Test 8 with real API calls and addition of Test 9.
- Directly inspected `src/sim/Chunk.hpp:36-37, 64-80` and `src/sim/World.cpp:138-201` and verified consistent `minActiveY`/`maxActiveY` bounds maintenance and neighbor waking.

### 2. Logic Chain
1. In Iteration 1, the review rejected Milestone 1 due to Finding 1 (facade test in Test 8) and Finding 3 (`SwapCells` omitting active bounds updates).
2. Worker M1 Iteration 2 refactored `Brush` to make `PaintStroke` and `PaintDisk` public with comprehensive parameter and coordinate overloads.
3. Worker M1 Iteration 2 rewrote Test 8 to exercise all 5 trajectories, 8-way continuous rasterization, radius 3 disk shape, and eraser strokes directly via `Brush`.
4. Observation of `tests/test_physics.cpp` confirms that no facade or manual loops bypass `Brush`.
5. Observation of `src/sim/Chunk.*` and `src/sim/World.*` confirms that active vertical bounds are consistently represented, maintained across swaps and placements, and verified by Test 9.
6. Execution of compiler and test runners confirms zero warnings, zero errors, and 100% pass rate.
7. Therefore, all requirements for Milestone 1 are satisfied without reservation.

### 3. Caveats
- No caveats. The remediation was strictly focused on the identified defects without introducing regressions to the simulation core or rendering pipeline.

### 4. Conclusion
Milestone 1 Iteration 2 is fully compliant with specifications, architectural requirements, and teamwork integrity standards. The code is production-ready for Milestone 1 and ready to advance to Milestone 2 (Thermodynamics, Reactions, Gases & Explosions).

### 5. Verification Method
To independently verify:
```powershell
cmake --build build --config Release
.\build\Release\test_physics.exe
ctest --test-dir build -C Release --output-on-failure
```
Invalidation conditions:
- Any test failure in `test_physics.exe` or `ctest`.
- Any reappearance of manual bypassing loops in `TestBrushBresenham`.
- Any particle freeze in `Physics::StepChunk` when entering a chunk at row 0.
