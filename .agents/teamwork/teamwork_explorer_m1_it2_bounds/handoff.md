# Milestone 1 Iteration 2 — Active Boundary & Neighbor Cascade Architecture Report

**Author**: Explorer 3 (Investigation & Synthesis)  
**Date**: 2026-11-11  
**Milestone**: Milestone 1 Iteration 2 (Active Bounds & Wake Cascade)  
**State Folder**: `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\teamwork_explorer_m1_it2_bounds`  
**Target Recipient**: Worker & Parent Orchestrator  

---

## Executive Summary

Reviewer 1 flagged a **Major Architectural Defect** in `World::SwapCells` (`src/sim/World.cpp:121-170`), where cell properties and `activeCellCount` are swapped across chunk boundaries, but `minActiveY` and `maxActiveY` in the source and destination chunks are never updated.

Our deep-dive investigation across `src/sim/Chunk.hpp`, `src/sim/Chunk.cpp`, `src/sim/World.hpp`, `src/sim/World.cpp`, and `src/sim/Physics.cpp` reveals that the bug extends beyond `SwapCells`:
1. **Inconsistent Empty Chunk Representation**: `Chunk::Clear` initializes `minActiveY = 0` and `maxActiveY = 63` even when `activeCellCount == 0`, whereas `Chunk::UpdateActiveBounds` sets `minActiveY = SIZE - 1` (63) and `maxActiveY = 0` for empty chunks. Because empty chunks start at `[0, 63]`, `Chunk::SetCell` never contracts bounds, rendering active-row bounding boxes completely inactive unless manually recomputed.
2. **Freezing Particles in `Physics::StepChunk`**: If bounds are tightened by `UpdateActiveBounds()` or future sleeping mechanisms, any particle falling into a chunk at $ly = 0$ or falling down to $ly + 1$ is outside `[minActiveY, maxActiveY]`. In `Physics::StepChunk:40`, the simulation loop `for (core::i32 ly = maxY; ly >= minY; --ly)` skips this row, freezing the particle indefinitely.
3. **Missing Neighbor Wake Cascade in `World::SwapCells`**: While `World::SetCell:70-74` wakes adjacent chunks when drawing at borders, `World::SwapCells` only wakes `c1` and `c2`. When a particle falls away from $ly = 0$ or lands at $ly = 63$, the chunk above $(cx, cy - 1)$ or below $(cx, cy + 1)$ remains asleep. Supported particles in sleeping chunks above the vacated hole never fall, causing unphysical floating structures. Furthermore, diagonal corner movements (e.g., powder sliding off edge corners) do not wake diagonal neighbor chunks.

This document presents a unified, $O(1)$, zero-allocation fix strategy for the Worker.

---

## 1. Observation

### 1.1 `minActiveY` / `maxActiveY` Initialization and Update Inconsistency
- In `src/sim/Chunk.hpp:36-37`:
  ```cpp
  core::i32 minActiveY{0};
  core::i32 maxActiveY{SIZE - 1};
  ```
- In `src/sim/Chunk.cpp:25-26` (`Chunk::Clear`):
  ```cpp
  minActiveY = 0;
  maxActiveY = SIZE - 1;
  ```
  Even when `fillMat == EMPTY_MATERIAL_ID` and `activeCellCount = 0`, bounds are set to `[0, 63]`.
- In `src/sim/Chunk.cpp:51-52` (`Chunk::SetCell`):
  ```cpp
  if (ly < minActiveY) minActiveY = ly;
  if (ly > maxActiveY) maxActiveY = ly;
  ```
  Since `minActiveY` starts at 0 and `maxActiveY` starts at 63, for any $ly \in [0, 63]$, $ly < 0$ and $ly > 63$ evaluate to `false`. Bounds never tighten to the actual rows where particles exist.
- In `src/sim/Chunk.cpp:76-80` (`Chunk::UpdateActiveBounds`):
  ```cpp
  if (activeCellCount == 0) {
      minActiveY = SIZE - 1; // 63
      maxActiveY = 0;        // 0
      return;
  }
  ```
  Here an empty chunk is represented by an inverted range (`minActiveY > maxActiveY`), which conflicts with `Clear()`.

### 1.2 `World::SwapCells` Omits Active Bounds Updates
- In `src/sim/World.cpp:121-170` (`World::SwapCells`):
  ```cpp
  // Track active cell count changes if swapping between different chunks
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
  }

  // Direct memory swap of cell properties
  std::swap(c1->material[idx1], c2->material[idx2]);
  std::swap(c1->temperature[idx1], c2->temperature[idx2]);
  std::swap(c1->lifetime[idx1], c2->lifetime[idx2]);

  // Stamp current tick parity to prevent double-updates
  c1->flags[idx1] = (c1->flags[idx1] & ~FLAG_PARITY) | m_currentParity;
  c2->flags[idx2] = (c2->flags[idx2] & ~FLAG_PARITY) | m_currentParity;

  c1->isDirty = true;
  c1->WakeUp();

  if (c1 != c2) {
      c2->isDirty = true;
      c2->WakeUp();
  }
  ```
  `World::SwapCells` modifies `material`, `temperature`, `lifetime`, and `activeCellCount`, but **never touches `minActiveY` or `maxActiveY`** for either `c1` or `c2`.

### 1.3 `Physics::StepChunk` Relies Strictly on `[minActiveY, maxActiveY]`
- In `src/sim/Physics.cpp:30-40`:
  ```cpp
  void Physics::StepChunk(World& world, Chunk& chunk) noexcept {
      if (chunk.isSleeping || chunk.activeCellCount == 0) return;

      core::u8 currentParity = world.GetCurrentParity();
      core::i32 startX = chunk.chunkX * Chunk::SIZE;
      core::i32 startY = chunk.chunkY * Chunk::SIZE;

      core::i32 minY = std::max(0, chunk.minActiveY);
      core::i32 maxY = std::min(Chunk::SIZE - 1, chunk.maxActiveY);

      for (core::i32 ly = maxY; ly >= minY; --ly) {
  ```
  If Chunk $B$ has active cells at rows $20..30$ (`minActiveY = 20, maxActiveY = 30`), and a particle from Chunk $A$ above falls into Chunk $B$ at row $ly = 0$, `minActiveY` remains 20. When `StepChunk` runs, `ly` starts at 30 and stops at 20. Row $ly = 0$ is skipped, freezing the particle.

### 1.4 Missing Neighbor Wake Cascade in `World::SwapCells`
- In `src/sim/World.cpp:70-74` (`World::SetCell`):
  ```cpp
  // Wake neighbors if cell is placed at the chunk border
  if (lx == 0 && cx > 0) WakeChunk(cx - 1, cy);
  if (lx == Chunk::SIZE - 1 && cx + 1 < m_chunksX) WakeChunk(cx + 1, cy);
  if (ly == 0 && cy > 0) WakeChunk(cx, cy - 1);
  if (ly == Chunk::SIZE - 1 && cy + 1 < m_chunksY) WakeChunk(cx, cy + 1);
  ```
- In `src/sim/World.cpp:163-170` (`World::SwapCells`):
  `SwapCells` only executes `c1->WakeUp()` and `c2->WakeUp()`. It contains **no neighbor waking logic**.
  - If a particle within chunk $(cx, cy)$ falls from $ly = 62$ to $ly = 63$ (the bottom boundary), chunk $(cx, cy + 1)$ below is NOT woken.
  - If a particle within chunk $(cx, cy)$ falls from $ly = 0$ to $ly = 1$, vacating $ly = 0$, chunk $(cx, cy - 1)$ above is NOT woken. If $(cx, cy - 1)$ was sleeping, its resting particles will float.
  - If a particle at $(lx = 0, ly = 63)$ slides diagonally down-left to $(lx = 63, ly = 0)$ in chunk $(cx - 1, cy + 1)$, diagonal chunk neighbors are not systematically awakened.

---

## 2. Logic Chain

1. **Observation 1.1** establishes that empty chunks currently initialize with `minActiveY = 0` and `maxActiveY = 63`. Because $ly \in [0, 63]$, bounds can never expand beyond this range, so without calling `UpdateActiveBounds()`, every chunk acts as if all 64 rows are active.
2. When bounds are contracted via `UpdateActiveBounds()` (or when sleeping chunk bounds contraction is implemented for Milestone 3), `minActiveY` and `maxActiveY` accurately frame active cells (e.g. $[20, 30]$).
3. **Observation 1.2** establishes that when particles move via `World::SwapCells`, `minActiveY` and `maxActiveY` are not updated.
4. **Observation 1.3** establishes that `Physics::StepChunk` iterates only over $ly \in [\max(0, \text{chunk.minActiveY}), \min(63, \text{chunk.maxActiveY})]$.
5. If a particle enters a chunk at row $ly < \text{minActiveY}$ (e.g. $ly = 0$) or falls within a chunk to $ly > \text{maxActiveY}$ (e.g. $ly = 31$), and `SwapCells` fails to expand bounds, that row is omitted by the loop in `StepChunk`. The particle receives no simulation ticks and permanently freezes.
6. Furthermore, **Observation 1.4** demonstrates that `World::SwapCells` lacks boundary wake cascading. When a cell on a chunk border ($ly = 0$, $ly = 63$, $lx = 0$, $lx = 63$) is swapped:
   - If a particle moves onto $ly = 63$, the chunk below ($cy + 1$) must be awake so it can receive the falling particle on subsequent ticks.
   - If a particle vacates $ly = 0$, the chunk above ($cy - 1$) must be awake so any particles previously supported by the vacated cell can resume falling.
   - If a particle moves across diagonal corners, the diagonal neighbor must be awake.
7. Therefore, to ensure mathematical correctness, high simulation throughput (avoiding scanning 64 rows per empty/sparse chunk), and seamless chunk transitions:
   - Chunk active bounds representation must be standardized across `Chunk::Clear`, `Chunk::SetCell`, `Chunk::UpdateActiveBounds`, and `World::SwapCells`.
   - `World::SwapCells` must expand active bounds for any chunk receiving or moving a non-empty particle.
   - `World::SwapCells` and `World::SetCell` must invoke a unified boundary neighbor wake cascade.

---

## 3. Caveats

- **Full scan vs $O(1)$ expansion**: On particle movement, active bounds can be expanded in $O(1)$ time via `std::min`/`std::max`. Contracting bounds when a particle vacates an edge row requires scanning the vacated row across 64 columns, which is unnecessary and costly to do per swap ($O(64)$). As is standard in falling sand physics engines, bounds should **expand eagerly on swaps** and **contract lazily** (during `UpdateActiveBounds()` or when chunks go to sleep).
- **Single-threaded `Physics::Step` vs `Physics::StepChunk`**: Milestone 1's main loop currently uses `Physics::Step(world)` (full-grid row scanner from $y = \text{height} - 1$ down to 0). The bounds bug directly manifests when `Physics::StepChunk(world, chunk)` is called. Milestone 3's 4-phase checkerboard parallel runner will execute via `StepChunk`, making this fix a hard prerequisite for M3.
- **Empty material handling**: When swapping two non-empty materials (e.g., Sand displacing Water), both destination coordinates receive active material and both chunks must expand their bounds. When swapping with Air, only the coordinate receiving the non-empty material should expand its bounds. Expanding bounds for Air would degrade simulation culling.

---

## 4. Conclusion & Recommended Architecture

### 4.1 Chunk Bounds State Representation
Represent empty bounds consistently using an inverted interval:
- Empty chunk (`activeCellCount == 0`): `minActiveY = SIZE - 1` (63), `maxActiveY = 0`.
- Inverted condition (`minActiveY > maxActiveY`) signals an empty bounding box.
- Populated chunk: $0 \le \text{minActiveY} \le \text{maxActiveY} \le \text{SIZE} - 1$.

### 4.2 New Helper Methods on `Chunk`
Add two inline methods to `Chunk`:
```cpp
void ExpandActiveBounds(core::i32 ly) noexcept {
    if (minActiveY > maxActiveY) {
        minActiveY = ly;
        maxActiveY = ly;
    } else {
        if (ly < minActiveY) minActiveY = ly;
        if (ly > maxActiveY) maxActiveY = ly;
    }
}

void ResetActiveBounds() noexcept {
    minActiveY = SIZE - 1;
    maxActiveY = 0;
}
```

### 4.3 Update `Chunk::Clear` and `Chunk::SetCell`
1. In `Chunk::Clear`:
   - If `fillMat == EMPTY_MATERIAL_ID`: set `activeCellCount = 0`, `minActiveY = SIZE - 1`, `maxActiveY = 0`.
   - If `fillMat != EMPTY_MATERIAL_ID`: set `activeCellCount = AREA`, `minActiveY = 0`, `maxActiveY = SIZE - 1`.
2. In `Chunk::SetCell`:
   - If `mat != EMPTY_MATERIAL_ID`: call `ExpandActiveBounds(ly);`
   - If `mat == EMPTY_MATERIAL_ID && activeCellCount == 0`: call `ResetActiveBounds();`

### 4.4 Update `World::SwapCells`
After swapping cell properties:
1. If `c1 != c2`:
   - If `c1->activeCellCount == 0`: `c1->ResetActiveBounds();`
   - If `c2->activeCellCount == 0`: `c2->ResetActiveBounds();`
2. Expand bounds for non-empty cells:
   - If `c1->material[idx1] != EMPTY_MATERIAL_ID`: `c1->ExpandActiveBounds(ly1);`
   - If `c2->material[idx2] != EMPTY_MATERIAL_ID`: `c2->ExpandActiveBounds(ly2);`
3. Cascade boundary neighbor wakeups:
   - Call `WakeNeighborsForCell` for `(cx1, cy1, lx1, ly1)`.
   - Call `WakeNeighborsForCell` for `(cx2, cy2, lx2, ly2)`.

### 4.5 Centralize `World::WakeNeighborsForCell`
Add private/public helper `WakeNeighborsForCell` in `World`:
```cpp
void World::WakeNeighborsForCell(core::i32 cx, core::i32 cy, core::i32 lx, core::i32 ly) noexcept {
    // 1. Orthogonal neighbors across edges
    if (lx == 0 && cx > 0) WakeChunk(cx - 1, cy);
    if (lx == Chunk::SIZE - 1 && cx + 1 < m_chunksX) WakeChunk(cx + 1, cy);
    if (ly == 0 && cy > 0) WakeChunk(cx, cy - 1);
    if (ly == Chunk::SIZE - 1 && cy + 1 < m_chunksY) WakeChunk(cx, cy + 1);

    // 2. Diagonal corner neighbors
    if (lx == 0 && ly == 0 && cx > 0 && cy > 0)
        WakeChunk(cx - 1, cy - 1);
    if (lx == Chunk::SIZE - 1 && ly == 0 && cx + 1 < m_chunksX && cy > 0)
        WakeChunk(cx + 1, cy - 1);
    if (lx == 0 && ly == Chunk::SIZE - 1 && cx > 0 && cy + 1 < m_chunksY)
        WakeChunk(cx - 1, cy + 1);
    if (lx == Chunk::SIZE - 1 && ly == Chunk::SIZE - 1 && cx + 1 < m_chunksX && cy + 1 < m_chunksY)
        WakeChunk(cx + 1, cy + 1);
}
```
Use this helper in both `World::SetCell` and `World::SwapCells`.

---

## 5. Detailed Implementation Blueprint for Worker

The Worker should apply the following modifications:

### 5.1 File: `src/sim/Chunk.hpp`
Add declarations:
```cpp
    // Active bounds management
    void ExpandActiveBounds(core::i32 ly) noexcept;
    void ResetActiveBounds() noexcept;
```
Ensure default initializers match empty bounds:
```cpp
    core::i32 minActiveY{SIZE - 1};
    core::i32 maxActiveY{0};
```

### 5.2 File: `src/sim/Chunk.cpp`
1. Update `Chunk::Clear`:
   ```cpp
   void Chunk::Clear(MaterialId fillMat, core::f32 defaultTemp) noexcept {
       if (fillMat == EMPTY_MATERIAL_ID) {
           std::memset(material, 0, sizeof(material));
           activeCellCount = 0;
           minActiveY = SIZE - 1;
           maxActiveY = 0;
       } else {
           std::fill_n(material, AREA, fillMat);
           activeCellCount = AREA;
           minActiveY = 0;
           maxActiveY = SIZE - 1;
       }
       std::fill_n(temperature, AREA, defaultTemp);
       std::memset(flags, 0, sizeof(flags));
       std::memset(lifetime, 0, sizeof(lifetime));

       isDirty = true;
       isSleeping = false;
       sleepCounter = SLEEP_HYSTERESIS;
   }
   ```
2. Update `Chunk::SetCell`:
   ```cpp
   void Chunk::SetCell(core::i32 lx, core::i32 ly, MaterialId mat, core::f32 temp,
                       core::u8 flag, core::u8 life) noexcept {
       core::i32 idx = Index(lx, ly);
       MaterialId oldMat = material[idx];

       if (oldMat == EMPTY_MATERIAL_ID && mat != EMPTY_MATERIAL_ID) {
           ++activeCellCount;
       } else if (oldMat != EMPTY_MATERIAL_ID && mat == EMPTY_MATERIAL_ID) {
           if (activeCellCount > 0) --activeCellCount;
       }

       material[idx] = mat;
       temperature[idx] = temp;
       flags[idx] = flag;
       lifetime[idx] = life;

       isDirty = true;
       WakeUp();

       if (mat != EMPTY_MATERIAL_ID) {
           ExpandActiveBounds(ly);
       } else if (activeCellCount == 0) {
           ResetActiveBounds();
       }
   }
   ```
3. Implement `ExpandActiveBounds` and `ResetActiveBounds`:
   ```cpp
   void Chunk::ExpandActiveBounds(core::i32 ly) noexcept {
       if (minActiveY > maxActiveY) {
           minActiveY = ly;
           maxActiveY = ly;
       } else {
           if (ly < minActiveY) minActiveY = ly;
           if (ly > maxActiveY) maxActiveY = ly;
       }
   }

   void Chunk::ResetActiveBounds() noexcept {
       minActiveY = SIZE - 1;
       maxActiveY = 0;
   }
   ```

### 5.3 File: `src/sim/World.hpp`
Add method to `World`:
```cpp
    void WakeNeighborsForCell(core::i32 cx, core::i32 cy, core::i32 lx, core::i32 ly) noexcept;
```

### 5.4 File: `src/sim/World.cpp`
1. Update `World::SetCell` to use `WakeNeighborsForCell`:
   ```cpp
   bool World::SetCell(core::i32 x, core::i32 y, MaterialId mat, core::f32 temp,
                       core::u8 flags, core::u8 lifetime) noexcept {
       if (!IsInBounds(x, y)) return false;

       core::i32 cx = WorldToChunkX(x);
       core::i32 cy = WorldToChunkY(y);
       core::i32 lx = WorldToLocalX(x);
       core::i32 ly = WorldToLocalY(y);

       Chunk* chunk = GetChunk(cx, cy);
       if (!chunk) return false;

       core::u8 finalFlags = (flags & ~FLAG_PARITY) | (m_currentParity ^ FLAG_PARITY);
       chunk->SetCell(lx, ly, mat, temp, finalFlags, lifetime);

       WakeNeighborsForCell(cx, cy, lx, ly);
       return true;
   }
   ```
2. Update `World::SwapCells`:
   ```cpp
   void World::SwapCells(core::i32 x1, core::i32 y1, core::i32 x2, core::i32 y2) noexcept {
       if (!IsInBounds(x1, y1) || !IsInBounds(x2, y2)) return;

       core::i32 cx1 = WorldToChunkX(x1);
       core::i32 cy1 = WorldToChunkY(y1);
       core::i32 lx1 = WorldToLocalX(x1);
       core::i32 ly1 = WorldToLocalY(y1);
       core::i32 idx1 = Chunk::Index(lx1, ly1);

       core::i32 cx2 = WorldToChunkX(x2);
       core::i32 cy2 = WorldToChunkY(y2);
       core::i32 lx2 = WorldToLocalX(x2);
       core::i32 ly2 = WorldToLocalY(y2);
       core::i32 idx2 = Chunk::Index(lx2, ly2);

       Chunk* c1 = GetChunk(cx1, cy1);
       Chunk* c2 = (cx1 == cx2 && cy1 == cy2) ? c1 : GetChunk(cx2, cy2);

       if (!c1 || !c2) return;

       // Track active cell count changes if swapping between different chunks
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
           if (c1->activeCellCount == 0) c1->ResetActiveBounds();
           if (c2->activeCellCount == 0) c2->ResetActiveBounds();
       }

       // Direct memory swap of cell properties
       std::swap(c1->material[idx1], c2->material[idx2]);
       std::swap(c1->temperature[idx1], c2->temperature[idx2]);
       std::swap(c1->lifetime[idx1], c2->lifetime[idx2]);

       // Stamp current tick parity to prevent double-updates
       c1->flags[idx1] = (c1->flags[idx1] & ~FLAG_PARITY) | m_currentParity;
       c2->flags[idx2] = (c2->flags[idx2] & ~FLAG_PARITY) | m_currentParity;

       c1->isDirty = true;
       c1->WakeUp();

       // Expand active vertical bounds for chunks holding non-empty particles
       if (c1->material[idx1] != EMPTY_MATERIAL_ID) {
           c1->ExpandActiveBounds(ly1);
       }
       if (c2->material[idx2] != EMPTY_MATERIAL_ID) {
           c2->ExpandActiveBounds(ly2);
       }

       if (c1 != c2) {
           c2->isDirty = true;
           c2->WakeUp();
       }

       // Wake neighbor chunks if cells touch chunk borders
       WakeNeighborsForCell(cx1, cy1, lx1, ly1);
       if (c1 != c2 || lx1 != lx2 || ly1 != ly2) {
           WakeNeighborsForCell(cx2, cy2, lx2, ly2);
       }
   }
   ```
3. Implement `WakeNeighborsForCell`:
   ```cpp
   void World::WakeNeighborsForCell(core::i32 cx, core::i32 cy, core::i32 lx, core::i32 ly) noexcept {
       if (lx == 0 && cx > 0) WakeChunk(cx - 1, cy);
       if (lx == Chunk::SIZE - 1 && cx + 1 < m_chunksX) WakeChunk(cx + 1, cy);
       if (ly == 0 && cy > 0) WakeChunk(cx, cy - 1);
       if (ly == Chunk::SIZE - 1 && cy + 1 < m_chunksY) WakeChunk(cx, cy + 1);

       if (lx == 0 && ly == 0 && cx > 0 && cy > 0)
           WakeChunk(cx - 1, cy - 1);
       if (lx == Chunk::SIZE - 1 && ly == 0 && cx + 1 < m_chunksX && cy > 0)
           WakeChunk(cx + 1, cy - 1);
       if (lx == 0 && ly == Chunk::SIZE - 1 && cx > 0 && cy + 1 < m_chunksY)
           WakeChunk(cx - 1, cy + 1);
       if (lx == Chunk::SIZE - 1 && ly == Chunk::SIZE - 1 && cx + 1 < m_chunksX && cy + 1 < m_chunksY)
           WakeChunk(cx + 1, cy + 1);
   }
   ```

---

## 6. Verification Method

### 6.1 Unit Test Specification (`tests/test_physics.cpp`)
Add a new comprehensive test `TestActiveBoundsAndNeighborWaking()`:
```cpp
void TestActiveBoundsAndNeighborWaking() {
    std::cout << "[TEST] 9. Chunk Active Bounds Tracking & Boundary Wake Cascades..." << std::endl;

    World world(1280, 768);
    Physics physics;
    auto& reg = MaterialRegistry::Instance();
    MaterialId sand = reg.FindByName("sand");

    // 1. Empty chunk initial state has inverted bounds
    Chunk* c00 = world.GetChunk(0, 0);
    TEST_ASSERT(c00->activeCellCount == 0);
    TEST_ASSERT(c00->minActiveY > c00->maxActiveY && "Empty chunk must have inverted bounds");

    // 2. SetCell expands bounds to exact row
    world.SetCell(10, 25, sand);
    TEST_ASSERT(c00->activeCellCount == 1);
    TEST_ASSERT(c00->minActiveY == 25 && c00->maxActiveY == 25);

    // 3. Falling within chunk expands maxActiveY
    physics.Step(world);
    world.AdvanceTick();
    TEST_ASSERT(c00->minActiveY <= 25 && c00->maxActiveY >= 26);
    TEST_ASSERT(world.GetCellMaterial(10, 26) == sand);

    // 4. Inter-chunk swap into empty neighbor at ly = 0
    world.Clear();
    Chunk* c01 = world.GetChunk(0, 1);
    // Put sand at bottom-most row of Chunk (0, 0)
    world.SetCell(10, 63, sand);
    TEST_ASSERT(c00->minActiveY == 63 && c00->maxActiveY == 63);
    TEST_ASSERT(c01->minActiveY > c01->maxActiveY);

    // Step physics: sand falls from (10, 63) to (10, 64) -> Chunk(0, 1) local ly = 0
    physics.Step(world);
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(10, 64) == sand);
    TEST_ASSERT(c01->activeCellCount == 1);
    TEST_ASSERT(c01->minActiveY == 0 && c01->maxActiveY == 0 && "Destination chunk bounds must include row 0");

    // Crucial: StepChunk on Chunk(0, 1) must now step row 0 down to row 1
    physics.StepChunk(world, *c01);
    world.AdvanceTick();
    TEST_ASSERT(world.GetCellMaterial(10, 65) == sand && "StepChunk must not skip row 0");
    TEST_ASSERT(c01->maxActiveY >= 1);

    // 5. Boundary Wake Cascade: Put Chunk(0, 1) to sleep, move particle to border ly = 63
    world.Clear();
    c01->isSleeping = true;
    world.SetCell(10, 62, sand);
    physics.Step(world); // Falls to 63
    world.AdvanceTick();

    TEST_ASSERT(world.GetCellMaterial(10, 63) == sand);
    TEST_ASSERT(!c01->isSleeping && "Chunk below must be woken when active particle reaches ly = 63");

    std::cout << "  -> Active bounds tracking & boundary wake cascades verified. PASSED." << std::endl;
}
```

### 6.2 Compilation & Execution Commands
```powershell
# 1. Compile project
cmake --build build --config Release

# 2. Execute physics unit tests
.\build\Release\test_physics.exe

# 3. Execute e2e test suite
.\build\Release\sandbox_e2e_tests.exe --all
```

### 6.3 Invalidation Conditions
The fix shall be deemed invalid if:
- Any falling or liquid particle freezes at a chunk boundary ($y = 63 \to y = 64$) when simulated via `Physics::StepChunk`.
- `Chunk` memory size deviates from 32,832 bytes or alignment deviates from 64 bytes (`alignof(Chunk) == 64`).
- An empty chunk retains historical bounds from deleted cells instead of resetting to inverted bounds.
- A sleeping chunk directly adjacent to a boundary movement fails to awaken.
