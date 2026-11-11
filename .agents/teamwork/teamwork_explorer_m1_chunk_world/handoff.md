# Architectural Investigation & Design Report: 64x64 SoA Chunk & World Grid

**Author**: Teamwork Explorer (Chunk & Grid Architecture Explorer for Milestone 1)  
**Date**: 2026-11-10  
**Target Milestone**: Milestone 1 (ШАГ 2 — World, Chunks, MaterialRegistry & Basic Physics/Rendering)  
**Target Components**: `src/sim/Chunk.hpp`, `src/sim/Chunk.cpp`, `src/sim/World.hpp`, `src/sim/World.cpp`

---

## 1. Observation

### 1.1 Existing Codebase & Directory Structure
1. **Existing Core Types (`src/core/Types.hpp`)**:
   - `CACHE_LINE_SIZE = 64` (line 24): Processor cache line alignment constant.
   - `CHUNK_SIZE = 64` (line 27): Edge length of one chunk in cells.
   - `CHUNK_AREA = CHUNK_SIZE * CHUNK_SIZE` (line 28): Exactly 4096 cells per chunk.
   - Fixed-width numeric types: `i8, i16, i32, i64, u8, u16, u32, u64, f32, f64` (lines 10–21).
   - Geometry and color types: `Point2i`, `Rect2i`, `Color4u` (lines 31–99).
   - *Observation*: `MaterialId` and `CellFlags` are not yet defined in `src/core/Types.hpp`.
2. **Existing Configuration & Dimensions (`src/core/Config.hpp`, `assets/config.json`)**:
   - `worldWidth{1280}` (Config.hpp line 17, `assets/config.json` line 9): World width in cells.
   - `worldHeight{768}` (Config.hpp line 18, `assets/config.json` line 10): World height in cells.
   - Grid chunk geometry: $1280 / 64 = 20$ chunks horizontally, $768 / 64 = 12$ chunks vertically, totaling **240 chunks** ($983,040$ cells).
   - Target simulation rate: `targetTPS{60}` ($\Delta t = 16.666$ ms).
3. **Simulation Source Directory (`src/sim/`)**:
   - Directory `src/sim/` exists in the filesystem but contains **zero files**.
4. **Build System (`CMakeLists.txt`)**:
   - `ALL_SOURCES` currently includes only `src/main.cpp`, `src/core/Config.cpp`, and `src/core/ThreadPool.cpp` (lines 60–68).
   - To link `Chunk` and `World`, CMakeLists.txt must be updated to include `src/sim/Chunk.cpp` and `src/sim/World.cpp` (or a dedicated static library `sandbox_sim`).

---

## 2. Logic Chain

### 2.1 64x64 SoA Chunk Data Layout & L1 Cache Residency
1. *Hardware Cache Constraints*: Modern x86-64 processors (Intel Alder Lake/Raptor Lake, AMD Zen 3/Zen 4) possess 32 KB or 48 KB of L1 Data Cache per physical core, with 64-byte cache lines.
2. *SoA Data Sizing Calculation*:
   - A chunk is $64 \times 64 = 4096$ cells.
   - `material[4096]` (`core::u16` = 2 bytes): $4096 \times 2 = 8,192$ bytes (8 KB = 128 cache lines).
   - `temperature[4096]` (`core::f32` = 4 bytes): $4096 \times 4 = 16,384$ bytes (16 KB = 256 cache lines).
   - `flags[4096]` (`core::u8` = 1 byte): $4096 \times 1 = 4,096$ bytes (4 KB = 64 cache lines).
   - `lifetime[4096]` (`core::u8` = 1 byte): $4096 \times 1 = 4,096$ bytes (4 KB = 64 cache lines).
   - Total contiguous arrays size: $8192 + 16384 + 4096 + 4096 = 32,768$ bytes = **exactly 32.0 KB**.
3. *AoS vs SoA Performance Derivation*:
   - Under AoS (`struct Cell { u16 mat; f32 temp; u8 flags; u8 life; };` = 8 bytes per cell), a chunk is 32,768 bytes, but scanning a row of 64 cells for material IDs pulls all 8 bytes per cell ($64 \times 8 = 512$ bytes = 8 cache lines).
   - Under SoA, scanning a row of 64 material IDs accesses only $64 \times 2 = 128$ bytes = **exactly 2 cache lines**.
   - This delivers a **4x reduction** in cache bandwidth during physics sweeps.
   - Furthermore, in Milestone 2, temperature diffusion over `f32 temperature[4096]` achieves 100% contiguous vectorization via AVX2 (8 floats per 256-bit register) without scatter/gather overhead.
4. *Alignment Requirements*:
   - Adding `alignas(core::CACHE_LINE_SIZE)` (64 bytes) to each array and to the `Chunk` class itself guarantees that every row boundary is aligned to an exact cache line, eliminating split-cache-line access stalls.

### 2.2 Indexing Arithmetic
1. The chunk dimensions are $64 \times 64$.
2. Since $64 = 2^6$, row multiplication and coordinate extraction map directly to bitwise operations:
   - Forward mapping:
     $$\text{Index}(lx, ly) = (ly \ll 6) \mid lx$$
   - Reverse mapping:
     $$lx = \text{index} \ \& \ 63$$
     $$ly = \text{index} \gg 6$$
3. Bitwise shifts execute in a single CPU cycle ($< 0.5$ ns) and avoid integer division (`idiv`/`imul`).

### 2.3 Parity Flags & Double-Update Prevention
1. *Hazard*: In cellular automata, when a powder or liquid particle at $(x, y)$ moves down to $(x, y+1)$, if the update loop traverses top-to-bottom, the particle at $(x, y+1)$ would be visited again in the next row, causing it to cascade down multiple rows in a single frame.
2. *Resolution Mechanism*:
   - Traversal order is bottom-to-top ($y = 63 \dots 0$).
   - To prevent double updates when particles move sideways or across chunk boundaries, each tick tracks a global frame parity bit:
     $$\text{currentParity} = \text{tick} \ \& \ 1$$
   - Flag bit 0 is reserved as `FLAG_PARITY = 0x01`.
   - When a cell moves to destination $(tx, ty)$, its parity bit is set to `currentParity`:
     $$\text{flags}[dst] = (\text{flags}[dst] \ \& \ \sim\text{FLAG\_PARITY}) \mid \text{currentParity}$$
   - During the simulation step, cells with `(flags[idx] & FLAG_PARITY) == currentParity` are skipped.
   - This prevents double updates with zero heap allocation and zero double-buffer memory cost.

### 2.4 World Grid Architecture & Coordinate Conversion
1. *Grid Sizing*:
   - Default dimensions: $W = 1280$, $H = 768$.
   - Chunk counts: $C_x = W / 64 = 20$, $C_y = H / 64 = 12$. Total chunks = 240.
2. *World Coordinate Mapping*:
   - For any valid world coordinate $(x, y) \in [0, W-1] \times [0, H-1]$:
     - Chunk coordinates:
       $$cx = x \gg 6$$
       $$cy = y \gg 6$$
     - Local chunk coordinates:
       $$lx = x \ \& \ 63$$
       $$ly = y \ \& \ 63$$
     - Chunk array index:
       $$\text{chunkIndex} = cy \cdot C_x + cx$$
     - Local cell index:
       $$\text{localIndex} = (ly \ll 6) \mid lx$$
   - Inverse mapping from $(cx, cy, lx, ly)$ to $(x, y)$:
     $$x = (cx \ll 6) \mid lx$$
     $$y = (cy \ll 6) \mid ly$$
3. *Chunk Memory Allocation Model*:
   - Storing chunks as `std::vector<std::unique_ptr<Chunk>> m_chunks` of size $C_x \times C_y$:
     - Each chunk is allocated on heap with 64-byte alignment (`std::make_unique<Chunk>()` or aligned `new`).
     - Stable pointers: raw pointers `Chunk*` remain valid across world lifetime and can be safely passed to worker threads in multithreaded checkerboard phases (Milestone 3).
     - Flat indexing: $O(1)$ access via `m_chunks[cy * m_chunksX + cx]`.

### 2.5 Atomic Cell Swap (`SwapCells`) & Boundary Safety
1. *Intra-Chunk vs Inter-Chunk Swapping*:
   - When $(x_1, y_1)$ and $(x_2, y_2)$ lie within the same chunk ($cx_1 == cx_2 \land cy_1 == cy_2$):
     - Both indices are dereferenced from the same `Chunk*`.
     - Direct `std::swap` of `material`, `temperature`, `lifetime`.
     - Update parity bit on both cells.
     - Set chunk `isDirty = true` and `isSleeping = false`.
   - When $(x_1, y_1)$ and $(x_2, y_2)$ cross chunk boundaries ($cx_1 \ne cx_2 \lor cy_1 \ne cy_2$):
     - Lookup `c1 = GetChunk(cx_1, cy_1)` and `c2 = GetChunk(cx_2, cy_2)`.
     - If either is out of bounds, swap is rejected.
     - Swap `material`, `temperature`, `lifetime` between `c1` at `idx1` and `c2` at `idx2`.
     - If empty material ($0$) is swapped with non-empty material:
       `c1->activeCellCount--; c2->activeCellCount++;`
     - Mark both chunks `isDirty = true`, `isSleeping = false`.
     - Call `WakeNeighbors` if swap touches the boundary cell of either chunk.
2. *Out-of-Bounds Handling*:
   - `IsInBounds(x, y)`: $0 \le x < W \land 0 \le y < H$.
   - `GetCellMaterial(x, y)`: returns `EMPTY_MATERIAL_ID` (0) if out-of-bounds.
   - `GetCellTemperature(x, y)`: returns ambient $20.0^\circ\text{C}$ if out-of-bounds.
   - `SetCell(x, y, ...)`: returns `false` and performs no memory access if out-of-bounds.

---

## 3. Caveats

1. **Over-Aligned Allocation on MSVC**:
   - `Chunk` has `alignas(64)`. In C++20 with MSVC (Visual Studio 2022), `operator new` and `std::make_unique` automatically support over-aligned types (`alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__`). However, if placed directly in a `std::vector<Chunk>`, MSVC standard allocator respects alignment, but `std::unique_ptr<Chunk>` is even safer and ensures zero pointer invalidation during world initialization.
2. **Signed Shift Arithmetic on Negative Coordinates**:
   - In C++20, right shift `>>` on signed integers is arithmetic shift (retaining sign). For negative world coordinates (e.g. `x = -1`), `(-1 >> 6) == -1`, but `lx = (-1 & 63) == 63`.
   - Bounds check `IsInBounds(x, y)` MUST precede coordinate conversion to prevent indexing negative chunk coordinates.
3. **Neighbor Interaction in 4-Phase Checkerboard (Future M3 Compatibility)**:
   - When swapping across chunk boundaries, Chebyshev distance between chunks in the same phase is $\ge 2$. Thus, Phase $p$ chunks only touch inactive chunks of other phases. However, in Milestone 1 (single-threaded), all chunks are accessible. The API design must remain 100% compatible with both M1 single-threaded and M3 multi-threaded dispatch.

---

## 4. Conclusion & Concrete Architectural Design

### 4.1 Chunk Architecture Specification (`src/sim/Chunk.hpp` and `.cpp`)

#### Header Design (`src/sim/Chunk.hpp`):
```cpp
#pragma once

#include "core/Types.hpp"
#include <algorithm>
#include <memory>

namespace sandbox::sim {

using MaterialId = core::u16;
inline constexpr MaterialId EMPTY_MATERIAL_ID = 0;

// Cell bit flags (u8)
enum CellFlags : core::u8 {
    FLAG_NONE     = 0x00,
    FLAG_PARITY   = 0x01, // Double-update prevention bit (matches current tick parity)
    FLAG_SLEEPING = 0x02, // Settled particle at rest
    FLAG_BURNING  = 0x04, // Particle actively burning
    FLAG_FALLING  = 0x08  // Dynamic falling particle
};

// 64x64 Structure-of-Arrays Chunk (alignas(64))
class alignas(core::CACHE_LINE_SIZE) Chunk {
public:
    static constexpr core::i32 SIZE = 64;
    static constexpr core::i32 AREA = SIZE * SIZE; // 4096 cells
    static constexpr core::u8  SLEEP_HYSTERESIS = 4; // Ticks of inactivity before sleep

    // --- Core SoA Arrays (32,768 bytes total = 32 KB L1 Data Cache resident) ---
    alignas(core::CACHE_LINE_SIZE) MaterialId material[AREA]{0};     // 8,192 bytes (128 cache lines)
    alignas(core::CACHE_LINE_SIZE) core::f32  temperature[AREA]{};  // 16,384 bytes (256 cache lines)
    alignas(core::CACHE_LINE_SIZE) core::u8   flags[AREA]{0};        // 4,096 bytes (64 cache lines)
    alignas(core::CACHE_LINE_SIZE) core::u8   lifetime[AREA]{0};     // 4,096 bytes (64 cache lines)

    // --- Chunk Metadata (Aligned to 64 bytes) ---
    core::i32 chunkX{0};
    core::i32 chunkY{0};
    core::u32 activeCellCount{0};
    core::i32 minActiveY{0};
    core::i32 maxActiveY{SIZE - 1};
    core::u8  sleepCounter{SLEEP_HYSTERESIS};
    bool      isDirty{true};
    bool      isSleeping{false};

    // Constructors
    Chunk() noexcept;
    Chunk(core::i32 cx, core::i32 cy) noexcept;

    // Fast 1D index from local coordinates: (ly << 6) | lx
    [[nodiscard]] static constexpr core::i32 Index(core::i32 lx, core::i32 ly) noexcept {
        return (ly << 6) | lx;
    }

    // Inverse: get local coordinates from 1D index
    static constexpr void FromIndex(core::i32 index, core::i32& lx, core::i32& ly) noexcept {
        lx = index & 63;
        ly = index >> 6;
    }

    // Checkerboard phase index: (cx & 1) | ((cy & 1) << 1) in [0..3]
    [[nodiscard]] constexpr core::u32 GetPhase() const noexcept {
        return (static_cast<core::u32>(chunkX) & 1u) |
               ((static_cast<core::u32>(chunkY) & 1u) << 1u);
    }

    // Cell accessors
    void SetCell(core::i32 lx, core::i32 ly, MaterialId mat, core::f32 temp = 20.0f,
                 core::u8 flag = FLAG_NONE, core::u8 life = 0) noexcept;

    // State management
    void Clear(MaterialId fillMat = EMPTY_MATERIAL_ID, core::f32 defaultTemp = 20.0f) noexcept;
    void WakeUp() noexcept;
    void StepSleep() noexcept;
    void UpdateActiveBounds() noexcept;
};

// Static assertions ensuring exact memory layout and alignment contracts
static_assert(sizeof(Chunk::material) == 8192, "Material array must be 8192 bytes");
static_assert(sizeof(Chunk::temperature) == 16384, "Temperature array must be 16384 bytes");
static_assert(sizeof(Chunk::flags) == 4096, "Flags array must be 4096 bytes");
static_assert(sizeof(Chunk::lifetime) == 4096, "Lifetime array must be 4096 bytes");
static_assert(alignof(Chunk) == 64, "Chunk class must be 64-byte cache line aligned");

} // namespace sandbox::sim
```

#### Implementation Design (`src/sim/Chunk.cpp`):
```cpp
#include "sim/Chunk.hpp"
#include <cstring>

namespace sandbox::sim {

Chunk::Chunk() noexcept : Chunk(0, 0) {}

Chunk::Chunk(core::i32 cx, core::i32 cy) noexcept
    : chunkX(cx), chunkY(cy) {
    Clear(EMPTY_MATERIAL_ID, 20.0f);
}

void Chunk::Clear(MaterialId fillMat, core::f32 defaultTemp) noexcept {
    if (fillMat == EMPTY_MATERIAL_ID) {
        std::memset(material, 0, sizeof(material));
        activeCellCount = 0;
    } else {
        std::fill_n(material, AREA, fillMat);
        activeCellCount = AREA;
    }
    std::fill_n(temperature, AREA, defaultTemp);
    std::memset(flags, 0, sizeof(flags));
    std::memset(lifetime, 0, sizeof(lifetime));

    minActiveY = 0;
    maxActiveY = SIZE - 1;
    isDirty = true;
    isSleeping = false;
    sleepCounter = SLEEP_HYSTERESIS;
}

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

    if (ly < minActiveY) minActiveY = ly;
    if (ly > maxActiveY) maxActiveY = ly;
}

void Chunk::WakeUp() noexcept {
    isSleeping = false;
    sleepCounter = SLEEP_HYSTERESIS;
}

void Chunk::StepSleep() noexcept {
    if (isSleeping) return;

    if (activeCellCount == 0) {
        isSleeping = true;
        return;
    }

    if (sleepCounter > 0) {
        --sleepCounter;
    } else {
        isSleeping = true;
    }
}

void Chunk::UpdateActiveBounds() noexcept {
    if (activeCellCount == 0) {
        minActiveY = SIZE - 1;
        maxActiveY = 0;
        return;
    }

    core::i32 newMin = -1;
    core::i32 newMax = -1;

    for (core::i32 ly = 0; ly < SIZE; ++ly) {
        core::i32 rowOffset = ly << 6;
        for (core::i32 lx = 0; lx < SIZE; ++lx) {
            if (material[rowOffset | lx] != EMPTY_MATERIAL_ID) {
                if (newMin == -1) newMin = ly;
                newMax = ly;
                break;
            }
        }
    }

    minActiveY = (newMin != -1) ? newMin : 0;
    maxActiveY = (newMax != -1) ? newMax : (SIZE - 1);
}

} // namespace sandbox::sim
```

---

### 4.2 World Grid Architecture Specification (`src/sim/World.hpp` and `.cpp`)

#### Header Design (`src/sim/World.hpp`):
```cpp
#pragma once

#include "sim/Chunk.hpp"
#include "core/Types.hpp"
#include <vector>
#include <memory>

namespace sandbox::sim {

class World {
public:
    // Default world dimensions: 1280x768 (20x12 chunks = 240 chunks)
    static constexpr core::i32 DEFAULT_WIDTH = 1280;
    static constexpr core::i32 DEFAULT_HEIGHT = 768;

private:
    core::i32 m_width{DEFAULT_WIDTH};
    core::i32 m_height{DEFAULT_HEIGHT};
    core::i32 m_chunksX{DEFAULT_WIDTH / Chunk::SIZE};   // 20
    core::i32 m_chunksY{DEFAULT_HEIGHT / Chunk::SIZE};  // 12
    core::u8  m_currentParity{0};                       // 0 or 1, toggled each tick

    // Array of 64-byte aligned unique chunk pointers
    std::vector<std::unique_ptr<Chunk>> m_chunks;

public:
    explicit World(core::i32 width = DEFAULT_WIDTH, core::i32 height = DEFAULT_HEIGHT);
    ~World() = default;

    // Non-copyable, movable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

    // Dimension accessors
    [[nodiscard]] constexpr core::i32 GetWidth() const noexcept { return m_width; }
    [[nodiscard]] constexpr core::i32 GetHeight() const noexcept { return m_height; }
    [[nodiscard]] constexpr core::i32 GetChunksX() const noexcept { return m_chunksX; }
    [[nodiscard]] constexpr core::i32 GetChunksY() const noexcept { return m_chunksY; }
    [[nodiscard]] size_t GetChunkCount() const noexcept { return m_chunks.size(); }
    [[nodiscard]] constexpr core::u8 GetCurrentParity() const noexcept { return m_currentParity; }

    // Bounds checking
    [[nodiscard]] constexpr bool IsInBounds(core::i32 x, core::i32 y) const noexcept {
        return x >= 0 && x < m_width && y >= 0 && y < m_height;
    }

    [[nodiscard]] constexpr bool IsChunkInBounds(core::i32 cx, core::i32 cy) const noexcept {
        return cx >= 0 && cx < m_chunksX && cy >= 0 && cy < m_chunksY;
    }

    // Fast coordinate transformations
    [[nodiscard]] static constexpr core::i32 WorldToChunkX(core::i32 x) noexcept { return x >> 6; }
    [[nodiscard]] static constexpr core::i32 WorldToChunkY(core::i32 y) noexcept { return y >> 6; }
    [[nodiscard]] static constexpr core::i32 WorldToLocalX(core::i32 x) noexcept { return x & 63; }
    [[nodiscard]] static constexpr core::i32 WorldToLocalY(core::i32 y) noexcept { return y & 63; }
    [[nodiscard]] static constexpr core::i32 LocalToWorldX(core::i32 cx, core::i32 lx) noexcept { return (cx << 6) | lx; }
    [[nodiscard]] static constexpr core::i32 LocalToWorldY(core::i32 cy, core::i32 ly) noexcept { return (cy << 6) | ly; }

    // Chunk flat index: cy * chunksX + cx
    [[nodiscard]] constexpr size_t ChunkIndex(core::i32 cx, core::i32 cy) const noexcept {
        return static_cast<size_t>(cy) * static_cast<size_t>(m_chunksX) + static_cast<size_t>(cx);
    }

    // Chunk retrieval
    [[nodiscard]] Chunk* GetChunk(core::i32 cx, core::i32 cy) noexcept;
    [[nodiscard]] const Chunk* GetChunk(core::i32 cx, core::i32 cy) const noexcept;
    [[nodiscard]] Chunk* GetChunkByIndex(size_t index) noexcept;
    [[nodiscard]] const Chunk* GetChunkByIndex(size_t index) const noexcept;
    [[nodiscard]] Chunk* GetChunkAtWorld(core::i32 x, core::i32 y) noexcept;
    [[nodiscard]] const Chunk* GetChunkAtWorld(core::i32 x, core::i32 y) const noexcept;

    // Cell manipulation
    bool SetCell(core::i32 x, core::i32 y, MaterialId mat, core::f32 temp = 20.0f,
                 core::u8 flags = FLAG_NONE, core::u8 lifetime = 0) noexcept;
    [[nodiscard]] MaterialId GetCellMaterial(core::i32 x, core::i32 y) const noexcept;
    [[nodiscard]] core::f32  GetCellTemperature(core::i32 x, core::i32 y) const noexcept;
    [[nodiscard]] core::u8   GetCellFlags(core::i32 x, core::i32 y) const noexcept;
    void SetCellFlags(core::i32 x, core::i32 y, core::u8 flags) noexcept;
    [[nodiscard]] core::u8   GetCellLifetime(core::i32 x, core::i32 y) const noexcept;
    void SetCellLifetime(core::i32 x, core::i32 y, core::u8 lifetime) noexcept;

    // Cell swap (handles intra-chunk and inter-chunk)
    void SwapCells(core::i32 x1, core::i32 y1, core::i32 x2, core::i32 y2) noexcept;

    // Sleeping & wake-up cascade
    void WakeChunk(core::i32 cx, core::i32 cy) noexcept;
    void WakeNeighbors(core::i32 cx, core::i32 cy) noexcept;
    void WakeChunkAtWorld(core::i32 x, core::i32 y) noexcept;

    // Simulation lifecycle
    void AdvanceTick() noexcept;
    void Clear(MaterialId fillMat = EMPTY_MATERIAL_ID, core::f32 temp = 20.0f) noexcept;

    // Telemetry
    [[nodiscard]] core::u32 GetTotalActiveCells() const noexcept;
    [[nodiscard]] core::u32 GetAwakeChunkCount() const noexcept;
};

} // namespace sandbox::sim
```

#### Implementation Design (`src/sim/World.cpp`):
```cpp
#include "sim/World.hpp"
#include <utility>

namespace sandbox::sim {

World::World(core::i32 width, core::i32 height) {
    // Clamp dimensions to multiples of CHUNK_SIZE (64)
    m_chunksX = std::max(1, (width + Chunk::SIZE - 1) / Chunk::SIZE);
    m_chunksY = std::max(1, (height + Chunk::SIZE - 1) / Chunk::SIZE);
    m_width = m_chunksX * Chunk::SIZE;
    m_height = m_chunksY * Chunk::SIZE;

    size_t totalChunks = static_cast<size_t>(m_chunksX) * static_cast<size_t>(m_chunksY);
    m_chunks.reserve(totalChunks);

    for (core::i32 cy = 0; cy < m_chunksY; ++cy) {
        for (core::i32 cx = 0; cx < m_chunksX; ++cx) {
            m_chunks.push_back(std::make_unique<Chunk>(cx, cy));
        }
    }
}

Chunk* World::GetChunk(core::i32 cx, core::i32 cy) noexcept {
    if (!IsChunkInBounds(cx, cy)) return nullptr;
    return m_chunks[ChunkIndex(cx, cy)].get();
}

const Chunk* World::GetChunk(core::i32 cx, core::i32 cy) const noexcept {
    if (!IsChunkInBounds(cx, cy)) return nullptr;
    return m_chunks[ChunkIndex(cx, cy)].get();
}

Chunk* World::GetChunkByIndex(size_t index) noexcept {
    if (index >= m_chunks.size()) return nullptr;
    return m_chunks[index].get();
}

const Chunk* World::GetChunkByIndex(size_t index) const noexcept {
    if (index >= m_chunks.size()) return nullptr;
    return m_chunks[index].get();
}

Chunk* World::GetChunkAtWorld(core::i32 x, core::i32 y) noexcept {
    if (!IsInBounds(x, y)) return nullptr;
    return GetChunk(WorldToChunkX(x), WorldToChunkY(y));
}

const Chunk* World::GetChunkAtWorld(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return nullptr;
    return GetChunk(WorldToChunkX(x), WorldToChunkY(y));
}

bool World::SetCell(core::i32 x, core::i32 y, MaterialId mat, core::f32 temp,
                    core::u8 flags, core::u8 lifetime) noexcept {
    if (!IsInBounds(x, y)) return false;

    core::i32 cx = WorldToChunkX(x);
    core::i32 cy = WorldToChunkY(y);
    core::i32 lx = WorldToLocalX(x);
    core::i32 ly = WorldToLocalY(y);

    Chunk* chunk = GetChunk(cx, cy);
    if (!chunk) return false;

    chunk->SetCell(lx, ly, mat, temp, flags, lifetime);

    // Wake neighbors if cell is placed at the chunk border
    if (lx == 0 && cx > 0) WakeChunk(cx - 1, cy);
    if (lx == Chunk::SIZE - 1 && cx + 1 < m_chunksX) WakeChunk(cx + 1, cy);
    if (ly == 0 && cy > 0) WakeChunk(cx, cy - 1);
    if (ly == Chunk::SIZE - 1 && cy + 1 < m_chunksY) WakeChunk(cx, cy + 1);

    return true;
}

MaterialId World::GetCellMaterial(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return EMPTY_MATERIAL_ID;
    const Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    return chunk ? chunk->material[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] : EMPTY_MATERIAL_ID;
}

core::f32 World::GetCellTemperature(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return 20.0f;
    const Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    return chunk ? chunk->temperature[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] : 20.0f;
}

core::u8 World::GetCellFlags(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return FLAG_NONE;
    const Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    return chunk ? chunk->flags[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] : FLAG_NONE;
}

void World::SetCellFlags(core::i32 x, core::i32 y, core::u8 flags) noexcept {
    if (!IsInBounds(x, y)) return;
    Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    if (chunk) {
        chunk->flags[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] = flags;
        chunk->isDirty = true;
    }
}

core::u8 World::GetCellLifetime(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return 0;
    const Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    return chunk ? chunk->lifetime[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] : 0;
}

void World::SetCellLifetime(core::i32 x, core::i32 y, core::u8 lifetime) noexcept {
    if (!IsInBounds(x, y)) return;
    Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    if (chunk) {
        chunk->lifetime[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] = lifetime;
        chunk->isDirty = true;
    }
}

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
}

void World::WakeChunk(core::i32 cx, core::i32 cy) noexcept {
    Chunk* chunk = GetChunk(cx, cy);
    if (chunk) {
        chunk->WakeUp();
    }
}

void World::WakeNeighbors(core::i32 cx, core::i32 cy) noexcept {
    for (core::i32 dy = -1; dy <= 1; ++dy) {
        for (core::i32 dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            WakeChunk(cx + dx, cy + dy);
        }
    }
}

void World::WakeChunkAtWorld(core::i32 x, core::i32 y) noexcept {
    if (IsInBounds(x, y)) {
        WakeChunk(WorldToChunkX(x), WorldToChunkY(y));
    }
}

void World::AdvanceTick() noexcept {
    m_currentParity ^= 1;
}

void World::Clear(MaterialId fillMat, core::f32 temp) noexcept {
    for (auto& chunk : m_chunks) {
        if (chunk) {
            chunk->Clear(fillMat, temp);
        }
    }
}

core::u32 World::GetTotalActiveCells() const noexcept {
    core::u32 total = 0;
    for (const auto& chunk : m_chunks) {
        if (chunk) {
            total += chunk->activeCellCount;
        }
    }
    return total;
}

core::u32 World::GetAwakeChunkCount() const noexcept {
    core::u32 awake = 0;
    for (const auto& chunk : m_chunks) {
        if (chunk && !chunk->isSleeping) {
            ++awake;
        }
    }
    return awake;
}

} // namespace sandbox::sim
```

---

## 5. Verification Method

To independently verify this architectural design upon implementation:

### 5.1 Static Layout & Alignment Verification
Compile and run unit tests asserting memory structures:
```cpp
// Verify 64-byte alignment
static_assert(alignof(sandbox::sim::Chunk) == 64, "Chunk must be 64-byte aligned");

// Verify SoA array sizes fit exact cache lines
static_assert(sizeof(sandbox::sim::Chunk::material) == 8192);    // 128 cache lines
static_assert(sizeof(sandbox::sim::Chunk::temperature) == 16384); // 256 cache lines
static_assert(sizeof(sandbox::sim::Chunk::flags) == 4096);        // 64 cache lines
static_assert(sizeof(sandbox::sim::Chunk::lifetime) == 4096);     // 64 cache lines

// Verify total chunk payload is ~32 KB (L1 Cache fit)
static_assert(sizeof(sandbox::sim::Chunk) >= 32768 && sizeof(sandbox::sim::Chunk) <= 33000);
```

### 5.2 Coordinate Transformation Round-Trip Test
For every cell in a $1280 \times 768$ world ($983,040$ coordinate pairs):
```cpp
for (int y = 0; y < 768; ++y) {
    for (int x = 0; x < 1280; ++x) {
        int cx = World::WorldToChunkX(x);
        int cy = World::WorldToChunkY(y);
        int lx = World::WorldToLocalX(x);
        int ly = World::WorldToLocalY(y);

        assert(cx >= 0 && cx < 20);
        assert(cy >= 0 && cy < 12);
        assert(lx >= 0 && lx < 64);
        assert(ly >= 0 && ly < 64);

        int reconstructedX = World::LocalToWorldX(cx, lx);
        int reconstructedY = World::LocalToWorldY(cy, ly);
        assert(reconstructedX == x);
        assert(reconstructedY == y);

        int localIdx = Chunk::Index(lx, ly);
        int decLx, decLy;
        Chunk::FromIndex(localIdx, decLx, decLy);
        assert(decLx == lx && decLy == ly);
    }
}
```

### 5.3 Cell Swapping & Parity Test
```cpp
World world(1280, 768);
const MaterialId SAND = 3;
const MaterialId WATER = 4;

// 1. Intra-chunk swap:
world.SetCell(10, 10, SAND, 25.0f);
world.SetCell(10, 11, WATER, 20.0f);
world.SwapCells(10, 10, 10, 11);
assert(world.GetCellMaterial(10, 10) == WATER);
assert(world.GetCellMaterial(10, 11) == SAND);
assert((world.GetCellFlags(10, 10) & FLAG_PARITY) == world.GetCurrentParity());

// 2. Inter-chunk swap across border (x=63 to x=64):
world.SetCell(63, 10, SAND, 30.0f);
world.SetCell(64, 10, WATER, 15.0f);
world.SwapCells(63, 10, 64, 10);
assert(world.GetCellMaterial(63, 10) == WATER);
assert(world.GetCellMaterial(64, 10) == SAND);

// 3. Out-of-bounds safety:
assert(world.GetCellMaterial(-1, 0) == EMPTY_MATERIAL_ID);
assert(world.GetCellMaterial(1280, 500) == EMPTY_MATERIAL_ID);
assert(!world.SetCell(-5, 100, SAND));
```

### 5.4 Build & Test Invalidation Conditions
This architecture remains verified and valid unless:
1. `CHUNK_SIZE` is altered from 64.
2. The compiler target is changed from C++20 with MSVC / Clang / GCC supporting over-aligned types.
3. The cache line width of the deployment target diverges from 64 bytes.
