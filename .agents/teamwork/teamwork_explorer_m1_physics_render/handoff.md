# Architectural Investigation & Design Report: Cellular Automata Physics, WorldRenderer & Interactive Brush

**Author**: Teamwork Explorer (Physics & Rendering Explorer for Milestone 1)  
**Date**: 2026-11-10  
**Target Milestone**: Milestone 1 (ШАГ 2 — World, Chunks, MaterialRegistry & Basic Physics/Rendering)  
**Target Components**:  
- `src/sim/Physics.hpp` / `src/sim/Physics.cpp`  
- `src/ui/Renderer.hpp` / `src/ui/Renderer.cpp`  
- `src/ui/Brush.hpp` / `src/ui/Brush.cpp`  

---

## 1. Observation

### 1.1 Existing Codebase & Environment Diagnostics
1. **Core Utilities & Types (`src/core/Types.hpp`, `src/core/Math.hpp`)**:
   - `Types.hpp` defines fixed-width primitives (`u8, u16, u32, i32, f32`), `CHUNK_SIZE = 64`, `CHUNK_AREA = 4096`, `Point2i`, `Rect2i`, and `Color4u` (lines 62–99).
   - `Color4u::ToU32()` packs channels as Little-Endian RGBA (byte 0 = R, byte 1 = G, byte 2 = B, byte 3 = A) which maps directly to SDL2's `SDL_PIXELFORMAT_RGBA32` (or `SDL_PIXELFORMAT_ABGR8888`).
   - `Math.hpp` defines `FastRNG` (Xorshift32 PRNG), `CoordinateHash(x, y, seed)`, and `CellNoiseVariation(x, y, maxVariation)` (lines 52–57), providing deterministic, zero-allocation spatial color dithering.
2. **Chunk & World Grid Contracts (`.agents/teamwork/teamwork_explorer_m1_chunk_world/handoff.md`)**:
   - `MaterialId` is `core::u16`, with `EMPTY_MATERIAL_ID = 0`.
   - `FLAG_PARITY = 0x01` is bit 0 of `flags[4096]`.
   - `World::AdvanceTick()` advances the simulation tick and toggles parity: `m_currentParity = (m_currentParity == 0) ? FLAG_PARITY : 0;`.
   - `World::SwapCells(x1, y1, x2, y2)` atomically exchanges `material`, `temperature`, and `lifetime` across intra- or inter-chunk boundaries, and automatically stamps `m_currentParity` into `flags` for both cells (lines 596–597 of peer handoff).
3. **SDL2 Hardware Renderer & Filtering Capabilities**:
   - Inspecting `external/sdl2/include/SDL_render.h`:
     * Line 103: `SDL_ScaleModeNearest` is present in enum `SDL_ScaleMode`.
     * Line 524: `SDL_SetTextureScaleMode(SDL_Texture* texture, SDL_ScaleMode scaleMode)` is available for setting point sampling (`GL_NEAREST`).
     * `SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height)` is standard across Windows Direct3D/OpenGL SDL2 backends.
4. **Build System & Toolchain Status**:
   - CMake 4.3.2 and MSVC 19.44 (Visual Studio 2022 BuildTools x64) configured SDL2 (static) and Dear ImGui in 0.7s and compiled `build/Release/sandbox.exe` in under 2 seconds.
   - `src/sim/` and `src/ui/` directories exist but currently have no implementation files for Physics, Renderer, or Brush.

---

## 2. Logic Chain

### 2.1 Cellular Automata Movement & Bias Elimination
1. **Vertical Traversal Direction**:
   - *Observation*: Under standard downward gravity, if row traversal proceeds top-to-bottom ($y = 0 \to H-1$), a particle at $(x, y)$ moving to $(x, y+1)$ is re-evaluated when the outer loop reaches $y+1$, falling indefinitely until hitting an obstacle in a single tick.
   - *Logic*: Traversing bottom-to-top ($y = H-1 \to 0$) guarantees that row $y+1$ has already completed its evaluation for the tick. Any cell moving into $y+1$ remains at rest until subsequent ticks.
2. **Horizontal Scan Bias Elimination**:
   - *Observation*: Scanning each row monotonically left-to-right causes particles to favor rightward slides, resulting in asymmetric pyramids with greater repose slope on the left.
   - *Logic*: Scan direction must alternate per row and per tick:
     $$\text{leftToRight} = ((y + \text{tickCount}) \ \& \ 1) \ne 0$$
   - In addition, the evaluation order of diagonal checks $(x-1, y+1)$ vs $(x+1, y+1)$ is randomized per cell using `FastRNG::Chance(0.5f)` or `(rng.NextU32() & 1)`.
3. **Parity Bit Tracking (`FLAG_PARITY = 0x01`)**:
   - *Observation*: When a liquid particle disperses horizontally into $(x + \Delta x, y)$ in the direction of the row sweep, it could be visited again during the same row iteration. Similarly, upward displacement of lighter liquids or gases would be visited again during bottom-to-top sweeps.
   - *Logic*:
     - Parity bit toggles between `0x00` and `0x01` on every tick.
     - When evaluating cell $(x, y)$: if `(flags[idx] & FLAG_PARITY) == currentParity`, the cell was already updated in this tick and is immediately skipped.
     - When `World::SwapCells` executes, it stamps `currentParity` into `flags` of both exchanged cells.
     - This delivers $O(1)$ zero-cost double-update protection without needing full-grid memory clears between frames.

### 2.2 Symmetric Mutual Density Displacement
1. *Governing Principle*: Natural buoyancy states that heavier matter sinks under gravity, displacing lighter matter upward.
2. *Mathematical Displacement Condition*:
   A source cell $A$ at $(x_1, y_1)$ can displace a destination cell $B$ at $(x_2, y_2)$ if and only if:
   - Target $B$ is not rigid (`targetMat.state != MaterialState::Solid`).
   - If $B$ is `EMPTY_MATERIAL_ID` (Air, $\rho \approx 1.2\text{ kg/m}^3$), any powder or liquid ($A$) displaces $B$.
   - If $A$ is Powder:
     - Displaces Liquid $B$ if $\rho(A) > \rho(B)$ (e.g. Sand $1600\text{ kg/m}^3 > \text{Water } 1000\text{ kg/m}^3$).
     - Does NOT displace other Powders (allowing distinct sand layers to rest on one another).
   - If $A$ is Liquid:
     - Displaces Liquid $B$ if $\rho(A) > \rho(B)$ (e.g. Water $1000\text{ kg/m}^3 > \text{Oil } 800\text{ kg/m}^3$).
     - Does NOT displace Powders or Solids (liquids rest on sand and stone).
3. *Atomic Exchange*:
   Displacement invokes `World::SwapCells(x1, y1, x2, y2)`. This swaps material ID, temperature, and lifetime, marks both chunks dirty, and updates active cell counts across chunk boundaries.

### 2.3 Liquid Lateral Dispersion & Cliff Drop-off Priority
1. **Dispersion Search**:
   - Liquid attempts direct downward fall first, then diagonal slides.
   - If blocked below and diagonally, it disperses laterally up to $D = \text{mat.dispersion}$ cells.
   - Primary search direction ($\pm 1$) is randomized 50/50.
2. **Cliff Drop-off Priority**:
   - As the liquid steps outward $d = 1 \dots D$:
     - If an obstacle is encountered at $(x + d \cdot \text{dir}, y)$, lateral raycasting stops.
     - For each open candidate cell $(x + d \cdot \text{dir}, y)$, check the support cell directly below: $(x + d \cdot \text{dir}, y + 1)$.
     - If the support cell is displaceable (Air or lighter fluid), a **cliff / waterfall ledge** is detected!
     - *Priority Rule*: The liquid halts horizontal expansion at this candidate coordinate and swaps immediately to $(x + d \cdot \text{dir}, y)$, allowing downward gravity to carry it over the cliff on the subsequent tick, rather than hovering horizontally across the chasm.

### 2.4 WorldRenderer Streaming Pipeline
1. **Pixel Buffer Architecture**:
   - A CPU-side buffer `std::vector<core::u32> m_pixels` sized $W \times H$ ($1280 \times 768 = 983,040$ pixels, ~3.93 MB).
   - For every dirty chunk, cells are mapped:
     $$\text{color} = \text{Clamp}(\text{mat.color} + \text{CellNoiseVariation}(wx, wy, \text{mat.colorVariation}))$$
   - Empty cells are mapped to clear / background color `{18, 18, 22, 255}`.
2. **Hardware Upload & Filtering**:
   - `SDL_UpdateTexture(m_texture, nullptr, m_pixels.data(), m_worldWidth * sizeof(u32))` uploads the full buffer in $\approx 0.2$ ms via DMA.
   - `SDL_SetTextureScaleMode(m_texture, SDL_ScaleModeNearest)` forces GPU nearest-neighbor point sampling (`GL_NEAREST`), maintaining sharp, pixel-perfect rendering without bilinear blur.
   - `SDL_RenderCopy(renderer, m_texture, nullptr, &dstRect)` scales the texture to window dimensions.

### 2.5 Interactive Brush & Continuous Stroke Interpolation
1. **Fast-Motion Discontinuity Hazard**:
   - High-DPI mice polled at 60 FPS can traverse 100+ pixels between consecutive frames. Sampling only instantaneous cursor coordinates produces disconnected dots.
2. **Bresenham Stroke Interpolation**:
   - When the mouse button is held down, line rasterization (Bresenham algorithm) connects `prevPos` to `currPos`.
   - At each rasterized point $(x_k, y_k)$, a filled disk of radius $R$ is painted onto the `World`:
     $$(x - x_k)^2 + (y - y_k)^2 \le R^2$$
   - LMB paints `selectedMaterialId`; RMB paints `EMPTY_MATERIAL_ID` (Air / eraser).
   - Mouse wheel adjusts radius $R \in [1, 50]$.
   - If `ImGui::GetIO().WantCaptureMouse` is true, brush painting is suppressed to prevent painting through UI windows.

---

## 3. Caveats

1. **Diagonal Wall Tunneling Prevention**:
   - In cellular automata, if cell $(x, y)$ moves diagonally to $(x+1, y+1)$ while $(x+1, y)$ and $(x, y+1)$ are solid walls, particles could pass through diagonal corners ("corner leaks").
   - *Mitigation*: In `Physics::UpdatePowder` and `UpdateLiquid`, diagonal movement is permitted only if either $(x \pm 1, y)$ or $(x, y+1)$ is non-solid.
2. **Texture Upload Overhead**:
   - Updating 983,040 pixels each frame transfers 3.93 MB per frame (235 MB/s at 60 FPS). PCIe bandwidth easily accommodates this. For even lower overhead, dirty chunk sub-rect updates (`SDL_UpdateTexture` with `SDL_Rect`) can be employed if profiling reveals CPU/GPU driver stalls.
3. **ImGui Mouse Capture**:
   - Brush input handling must query `ImGui::GetIO().WantCaptureMouse` before dispatching click or scroll events.

---

## 4. Conclusion & Concrete Architectural Specifications

### 4.1 Physics Specification (`src/sim/Physics.hpp` and `src/sim/Physics.cpp`)

#### `src/sim/Physics.hpp`:
```cpp
#pragma once

#include "sim/World.hpp"
#include "sim/MaterialRegistry.hpp"
#include "core/Math.hpp"
#include "core/Types.hpp"

namespace sandbox::sim {

class Physics {
private:
    core::FastRNG m_rng{1337u};
    core::u64     m_tickCount{0};

public:
    Physics() noexcept = default;
    explicit Physics(core::u32 seed) noexcept : m_rng(seed) {}

    // Main simulation step for all cellular automata
    void Step(World& world) noexcept;

    // Single chunk step (for multithreaded checkerboard phases in M3)
    void StepChunk(World& world, Chunk& chunk) noexcept;

    [[nodiscard]] core::u64 GetTickCount() const noexcept { return m_tickCount; }
    void Reset() noexcept { m_tickCount = 0; }

private:
    void UpdateCell(World& world, core::i32 x, core::i32 y, core::u8 currentParity) noexcept;
    void UpdatePowder(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept;
    void UpdateLiquid(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept;

    // Symmetric mutual density displacement rule
    [[nodiscard]] bool CanDisplace(const Material& sourceMat, MaterialId targetMatId) const noexcept;
};

} // namespace sandbox::sim
```

#### `src/sim/Physics.cpp`:
```cpp
#include "sim/Physics.hpp"
#include <algorithm>

namespace sandbox::sim {

void Physics::Step(World& world) noexcept {
    core::u8 currentParity = world.GetCurrentParity();
    core::i32 width = world.GetWidth();
    core::i32 height = world.GetHeight();

    // Bottom-to-top traversal prevents multi-cell cascading in a single tick
    for (core::i32 y = height - 1; y >= 0; --y) {
        // Alternating horizontal scan direction eliminates directional bias
        bool leftToRight = ((y + static_cast<core::i32>(m_tickCount)) & 1) != 0;

        if (leftToRight) {
            for (core::i32 x = 0; x < width; ++x) {
                UpdateCell(world, x, y, currentParity);
            }
        } else {
            for (core::i32 x = width - 1; x >= 0; --x) {
                UpdateCell(world, x, y, currentParity);
            }
        }
    }

    ++m_tickCount;
}

void Physics::StepChunk(World& world, Chunk& chunk) noexcept {
    if (chunk.isSleeping || chunk.activeCellCount == 0) return;

    core::u8 currentParity = world.GetCurrentParity();
    core::i32 startX = chunk.chunkX * Chunk::SIZE;
    core::i32 startY = chunk.chunkY * Chunk::SIZE;

    core::i32 minY = std::max(0, chunk.minActiveY);
    core::i32 maxY = std::min(Chunk::SIZE - 1, chunk.maxActiveY);

    for (core::i32 ly = maxY; ly >= minY; --ly) {
        core::i32 wy = startY + ly;
        bool leftToRight = ((wy + static_cast<core::i32>(m_tickCount)) & 1) != 0;

        if (leftToRight) {
            for (core::i32 lx = 0; lx < Chunk::SIZE; ++lx) {
                UpdateCell(world, startX + lx, wy, currentParity);
            }
        } else {
            for (core::i32 lx = Chunk::SIZE - 1; lx >= 0; --lx) {
                UpdateCell(world, startX + lx, wy, currentParity);
            }
        }
    }
}

void Physics::UpdateCell(World& world, core::i32 x, core::i32 y, core::u8 currentParity) noexcept {
    MaterialId matId = world.GetCellMaterial(x, y);
    if (matId == EMPTY_MATERIAL_ID) return;

    // Check parity bit: skip if already updated during this tick
    core::u8 flags = world.GetCellFlags(x, y);
    if ((flags & FLAG_PARITY) == currentParity) return;

    const Material& mat = MaterialRegistry::Instance().Get(matId);

    if (mat.state == MaterialState::Powder) {
        UpdatePowder(world, x, y, mat);
    } else if (mat.state == MaterialState::Liquid) {
        UpdateLiquid(world, x, y, mat);
    }
}

bool Physics::CanDisplace(const Material& sourceMat, MaterialId targetMatId) const noexcept {
    // Air (empty) is always displaceable by powder or liquid
    if (targetMatId == EMPTY_MATERIAL_ID) return true;

    const Material& targetMat = MaterialRegistry::Instance().Get(targetMatId);

    // Solids are rigid and cannot be displaced
    if (targetMat.state == MaterialState::Solid) return false;

    // Powders displace liquids with strictly lower density
    if (sourceMat.state == MaterialState::Powder) {
        if (targetMat.state == MaterialState::Liquid) {
            return sourceMat.density > targetMat.density;
        }
        if (targetMat.state == MaterialState::Gas) return true;
        return false; // Powders do not displace other powders (ensures stable slope)
    }

    // Liquids displace lighter liquids
    if (sourceMat.state == MaterialState::Liquid) {
        if (targetMat.state == MaterialState::Liquid) {
            return sourceMat.density > targetMat.density;
        }
        if (targetMat.state == MaterialState::Gas) return true;
        return false; // Liquids do not displace powders or solids
    }

    return false;
}

void Physics::UpdatePowder(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept {
    core::i32 downY = y + 1;
    if (downY >= world.GetHeight()) return;

    // 1. Direct downward gravity
    MaterialId downMat = world.GetCellMaterial(x, downY);
    if (CanDisplace(mat, downMat)) {
        world.SwapCells(x, y, x, downY);
        return;
    }

    // 2. Diagonal sliding with randomized direction to prevent slope skewing
    core::i32 dir = (m_rng.NextU32() & 1u) ? 1 : -1;
    core::i32 dirs[2] = { dir, -dir };

    for (core::i32 i = 0; i < 2; ++i) {
        core::i32 diagX = x + dirs[i];
        if (!world.IsInBounds(diagX, downY)) continue;

        // Diagonal corner leak guard: do not squeeze if both adjacent orthos are solid
        MaterialId sideMat = world.GetCellMaterial(diagX, y);
        const auto& sideMatRef = MaterialRegistry::Instance().Get(sideMat);
        const auto& downMatRef = MaterialRegistry::Instance().Get(downMat);
        if (sideMatRef.state == MaterialState::Solid && downMatRef.state == MaterialState::Solid) {
            continue;
        }

        MaterialId diagMat = world.GetCellMaterial(diagX, downY);
        if (CanDisplace(mat, diagMat)) {
            world.SwapCells(x, y, diagX, downY);
            return;
        }
    }
}

void Physics::UpdateLiquid(World& world, core::i32 x, core::i32 y, const Material& mat) noexcept {
    core::i32 downY = y + 1;

    // 1. Direct downward gravity
    if (downY < world.GetHeight()) {
        MaterialId downMat = world.GetCellMaterial(x, downY);
        if (CanDisplace(mat, downMat)) {
            world.SwapCells(x, y, x, downY);
            return;
        }

        // 2. Diagonal downward sliding
        core::i32 dir = (m_rng.NextU32() & 1u) ? 1 : -1;
        core::i32 dirs[2] = { dir, -dir };

        for (core::i32 i = 0; i < 2; ++i) {
            core::i32 diagX = x + dirs[i];
            if (!world.IsInBounds(diagX, downY)) continue;

            MaterialId sideMat = world.GetCellMaterial(diagX, y);
            const auto& sideMatRef = MaterialRegistry::Instance().Get(sideMat);
            const auto& downMatRef = MaterialRegistry::Instance().Get(downMat);
            if (sideMatRef.state == MaterialState::Solid && downMatRef.state == MaterialState::Solid) {
                continue;
            }

            MaterialId diagMat = world.GetCellMaterial(diagX, downY);
            if (CanDisplace(mat, diagMat)) {
                world.SwapCells(x, y, diagX, downY);
                return;
            }
        }
    }

    // 3. Lateral Dispersion with Cliff Drop-off Priority
    core::i32 dispersion = std::clamp(mat.dispersion, 1, 10);
    core::i32 primaryDir = (m_rng.NextU32() & 1u) ? 1 : -1;
    core::i32 dirs[2] = { primaryDir, -primaryDir };

    for (core::i32 dIdx = 0; dIdx < 2; ++dIdx) {
        core::i32 currentDir = dirs[dIdx];
        core::i32 bestX = x;

        for (core::i32 step = 1; step <= dispersion; ++step) {
            core::i32 targetX = x + currentDir * step;
            if (!world.IsInBounds(targetX, y)) break;

            MaterialId targetMat = world.GetCellMaterial(targetX, y);
            if (!CanDisplace(mat, targetMat)) break; // Raycast obstructed

            bestX = targetX;

            // CLIFF DROP-OFF CHECK:
            // Check support cell directly beneath the candidate
            core::i32 belowTargetY = y + 1;
            if (belowTargetY < world.GetHeight()) {
                MaterialId belowMat = world.GetCellMaterial(targetX, belowTargetY);
                if (CanDisplace(mat, belowMat)) {
                    // Cliff / hole discovered! Halt lateral expansion immediately
                    break;
                }
            }
        }

        if (bestX != x) {
            world.SwapCells(x, y, bestX, y);
            return;
        }
    }
}

} // namespace sandbox::sim
```

---

### 4.2 WorldRenderer Specification (`src/ui/Renderer.hpp` and `src/ui/Renderer.cpp`)

#### `src/ui/Renderer.hpp`:
```cpp
#pragma once

#include "sim/World.hpp"
#include "sim/MaterialRegistry.hpp"
#include "core/Types.hpp"
#include <SDL.h>
#include <vector>

namespace sandbox::ui {

class WorldRenderer {
private:
    SDL_Renderer*        m_sdlRenderer{nullptr};
    SDL_Texture*         m_texture{nullptr};
    core::i32            m_worldWidth{0};
    core::i32            m_worldHeight{0};
    std::vector<core::u32> m_pixelBuffer;
    core::u32            m_backgroundColor{0xFF161212}; // Little-Endian RGBA(18, 18, 22, 255)

public:
    WorldRenderer(SDL_Renderer* renderer, core::i32 worldWidth, core::i32 worldHeight);
    ~WorldRenderer();

    WorldRenderer(const WorldRenderer&) = delete;
    WorldRenderer& operator=(const WorldRenderer&) = delete;

    // Update pixel buffer from dirty chunks and stream to GPU
    void Render(sim::World& world, SDL_Renderer* renderer, const SDL_Rect* viewport = nullptr);

    // Coordinate conversion: converts screen pixels (sx, sy) to world cells (wx, wy)
    [[nodiscard]] bool ScreenToWorld(core::i32 screenX, core::i32 screenY,
                                     const SDL_Rect& viewport,
                                     core::i32& worldX, core::i32& worldY) const noexcept;

    void MarkAllDirty() noexcept;

private:
    void UpdateChunkPixels(const sim::Chunk& chunk);
};

} // namespace sandbox::ui
```

#### `src/ui/Renderer.cpp`:
```cpp
#include "ui/Renderer.hpp"
#include "core/Math.hpp"
#include "core/Log.hpp"
#include <algorithm>

namespace sandbox::ui {

WorldRenderer::WorldRenderer(SDL_Renderer* renderer, core::i32 worldWidth, core::i32 worldHeight)
    : m_sdlRenderer(renderer), m_worldWidth(worldWidth), m_worldHeight(worldHeight) {
    
    m_pixelBuffer.resize(static_cast<size_t>(m_worldWidth) * static_cast<size_t>(m_worldHeight), m_backgroundColor);

    // Create streaming texture with RGBA32 pixel format
    m_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        m_worldWidth,
        m_worldHeight
    );

    if (!m_texture) {
        LOG_ERROR("Renderer", "Failed to create SDL streaming texture: ", SDL_GetError());
    } else {
        // Enforce GL_NEAREST point filtering for crisp pixel art
        SDL_SetTextureScaleMode(m_texture, SDL_ScaleModeNearest);
    }
}

WorldRenderer::~WorldRenderer() {
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
}

void WorldRenderer::Render(sim::World& world, SDL_Renderer* renderer, const SDL_Rect* viewport) {
    if (!m_texture) return;

    size_t chunkCount = world.GetChunkCount();
    for (size_t i = 0; i < chunkCount; ++i) {
        sim::Chunk* chunk = world.GetChunkByIndex(i);
        if (chunk && chunk->isDirty) {
            UpdateChunkPixels(*chunk);
            chunk->isDirty = false;
        }
    }

    // Stream pixels to texture
    SDL_UpdateTexture(m_texture, nullptr, m_pixelBuffer.data(), m_worldWidth * static_cast<int>(sizeof(core::u32)));

    // Render scaled texture onto window
    SDL_RenderCopy(renderer, m_texture, nullptr, viewport);
}

void WorldRenderer::UpdateChunkPixels(const sim::Chunk& chunk) {
    core::i32 startWorldX = chunk.chunkX * sim::Chunk::SIZE;
    core::i32 startWorldY = chunk.chunkY * sim::Chunk::SIZE;

    for (core::i32 ly = 0; ly < sim::Chunk::SIZE; ++ly) {
        core::i32 wy = startWorldY + ly;
        size_t rowOffset = static_cast<size_t>(wy) * static_cast<size_t>(m_worldWidth);

        for (core::i32 lx = 0; lx < sim::Chunk::SIZE; ++lx) {
            core::i32 wx = startWorldX + lx;
            core::i32 localIdx = sim::Chunk::Index(lx, ly);
            sim::MaterialId matId = chunk.material[localIdx];

            if (matId == sim::EMPTY_MATERIAL_ID) {
                m_pixelBuffer[rowOffset + wx] = m_backgroundColor;
            } else {
                const auto& mat = sim::MaterialRegistry::Instance().Get(matId);
                core::i32 noise = core::CellNoiseVariation(wx, wy, mat.colorVariation);

                core::u8 r = static_cast<core::u8>(std::clamp<core::i32>(mat.color.r + noise, 0, 255));
                core::u8 g = static_cast<core::u8>(std::clamp<core::i32>(mat.color.g + noise, 0, 255));
                core::u8 b = static_cast<core::u8>(std::clamp<core::i32>(mat.color.b + noise, 0, 255));

                m_pixelBuffer[rowOffset + wx] = core::Color4u(r, g, b, mat.color.a).ToU32();
            }
        }
    }
}

bool WorldRenderer::ScreenToWorld(core::i32 screenX, core::i32 screenY,
                                  const SDL_Rect& viewport,
                                  core::i32& worldX, core::i32& worldY) const noexcept {
    if (viewport.w <= 0 || viewport.h <= 0) return false;
    if (screenX < viewport.x || screenX >= viewport.x + viewport.w ||
        screenY < viewport.y || screenY >= viewport.y + viewport.h) {
        return false;
    }

    worldX = static_cast<core::i32>((static_cast<core::i64>(screenX - viewport.x) * m_worldWidth) / viewport.w);
    worldY = static_cast<core::i32>((static_cast<core::i64>(screenY - viewport.y) * m_worldHeight) / viewport.h);
    return (worldX >= 0 && worldX < m_worldWidth && worldY >= 0 && worldY < m_worldHeight);
}

void WorldRenderer::MarkAllDirty() noexcept {
    // Fill background
    std::fill(m_pixelBuffer.begin(), m_pixelBuffer.end(), m_backgroundColor);
}

} // namespace sandbox::ui
```

---

### 4.3 Interactive Brush Specification (`src/ui/Brush.hpp` and `src/ui/Brush.cpp`)

#### `src/ui/Brush.hpp`:
```cpp
#pragma once

#include "sim/World.hpp"
#include "ui/Renderer.hpp"
#include "core/Types.hpp"
#include <SDL.h>

namespace sandbox::ui {

class Brush {
public:
    static constexpr core::i32 MIN_RADIUS = 1;
    static constexpr core::i32 MAX_RADIUS = 50;

private:
    sim::MaterialId m_selectedMaterial{1}; // Default to first registered material (e.g. Sand)
    core::i32       m_radius{5};
    bool            m_isPainting{false};
    bool            m_isErasing{false};
    core::Point2i   m_lastMousePos{0, 0};

public:
    Brush() noexcept = default;

    // Properties
    void SetMaterial(sim::MaterialId id) noexcept { m_selectedMaterial = id; }
    [[nodiscard]] sim::MaterialId GetMaterial() const noexcept { return m_selectedMaterial; }

    void SetRadius(core::i32 r) noexcept { m_radius = std::clamp(r, MIN_RADIUS, MAX_RADIUS); }
    [[nodiscard]] core::i32 GetRadius() const noexcept { return m_radius; }

    // Event handling
    void HandleEvent(const SDL_Event& event, const WorldRenderer& renderer, const SDL_Rect& viewport, sim::World& world) noexcept;

    // Per-frame update (for smooth continuous strokes while button is held)
    void Update(const WorldRenderer& renderer, const SDL_Rect& viewport, sim::World& world) noexcept;

private:
    void PaintStroke(sim::World& world, core::Point2i from, core::Point2i to) noexcept;
    void PaintDisk(sim::World& world, core::i32 cx, core::i32 cy, sim::MaterialId mat, core::i32 radius) noexcept;
};

} // namespace sandbox::ui
```

#### `src/ui/Brush.cpp`:
```cpp
#include "ui/Brush.hpp"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace sandbox::ui {

void Brush::HandleEvent(const SDL_Event& event, const WorldRenderer& renderer, const SDL_Rect& viewport, sim::World& world) noexcept {
    // If Dear ImGui wants mouse input, brush must ignore it
    if (ImGui::GetIO().WantCaptureMouse) {
        m_isPainting = false;
        m_isErasing = false;
        return;
    }

    if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y > 0) {
            SetRadius(m_radius + 1);
        } else if (event.wheel.y < 0) {
            SetRadius(m_radius - 1);
        }
    } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        core::i32 wx = 0, wy = 0;
        if (renderer.ScreenToWorld(event.button.x, event.button.y, viewport, wx, wy)) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                m_isPainting = true;
                m_isErasing = false;
                m_lastMousePos = { wx, wy };
                PaintDisk(world, wx, wy, m_selectedMaterial, m_radius);
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                m_isPainting = true;
                m_isErasing = true;
                m_lastMousePos = { wx, wy };
                PaintDisk(world, wx, wy, sim::EMPTY_MATERIAL_ID, m_radius);
            }
        }
    } else if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
            m_isPainting = false;
            m_isErasing = false;
        }
    }
}

void Brush::Update(const WorldRenderer& renderer, const SDL_Rect& viewport, sim::World& world) noexcept {
    if (!m_isPainting) return;

    if (ImGui::GetIO().WantCaptureMouse) {
        m_isPainting = false;
        m_isErasing = false;
        return;
    }

    int mx = 0, my = 0;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);

    bool lmb = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    bool rmb = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;

    if (!lmb && !rmb) {
        m_isPainting = false;
        return;
    }

    core::i32 wx = 0, wy = 0;
    if (renderer.ScreenToWorld(mx, my, viewport, wx, wy)) {
        core::Point2i currPos{ wx, wy };
        PaintStroke(world, m_lastMousePos, currPos);
        m_lastMousePos = currPos;
    }
}

void Brush::PaintStroke(sim::World& world, core::Point2i from, core::Point2i to) noexcept {
    core::i32 x0 = from.x;
    core::i32 y0 = from.y;
    core::i32 x1 = to.x;
    core::i32 y1 = to.y;

    sim::MaterialId mat = m_isErasing ? sim::EMPTY_MATERIAL_ID : m_selectedMaterial;

    // Bresenham's line algorithm for continuous stroke interpolation
    core::i32 dx = std::abs(x1 - x0);
    core::i32 dy = -std::abs(y1 - y0);
    core::i32 sx = (x0 < x1) ? 1 : -1;
    core::i32 sy = (y0 < y1) ? 1 : -1;
    core::i32 err = dx + dy;

    while (true) {
        PaintDisk(world, x0, y0, mat, m_radius);
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
}

void Brush::PaintDisk(sim::World& world, core::i32 cx, core::i32 cy, sim::MaterialId mat, core::i32 radius) noexcept {
    if (radius <= 1) {
        world.SetCell(cx, cy, mat);
        return;
    }

    core::i32 r2 = radius * radius;
    for (core::i32 dy = -radius; dy <= radius; ++dy) {
        core::i32 wy = cy + dy;
        if (wy < 0 || wy >= world.GetHeight()) continue;

        for (core::i32 dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                core::i32 wx = cx + dx;
                if (wx >= 0 && wx < world.GetWidth()) {
                    world.SetCell(wx, wy, mat);
                }
            }
        }
    }
}

} // namespace sandbox::ui
```

---

## 5. Verification Method

### 5.1 Verification Commands
1. **Compilation Check**:
   ```powershell
   cmake --build build --config Release
   ```
2. **Execution Test**:
   ```powershell
   .\build\Release\sandbox.exe
   ```

### 5.2 Deterministic Unit Test Matrix (`tests/test_physics.cpp`)
The following unit tests directly validate the physics logic chain:
1. **Test `PowderGravityFall`**:
   - Initialize empty world. Place Sand at $(10, 10)$. Step physics 1 tick.
   - Assert `GetCellMaterial(10, 11) == SandId` and `GetCellMaterial(10, 10) == EMPTY_MATERIAL_ID`.
2. **Test `PowderDiagonalRepose`**:
   - Place Stone at $(10, 11)$, Sand at $(10, 10)$, empty at $(9, 11)$ and $(11, 11)$.
   - Step physics 1 tick.
   - Assert Sand is at either $(9, 11)$ or $(11, 11)$, and $(10, 10)$ is now empty.
3. **Test `MutualDisplacementSandWater`**:
   - Place Water at $(20, 20)$ and Sand at $(20, 19)$.
   - Step physics 1 tick.
   - Assert Sand is at $(20, 20)$ and Water is displaced up to $(20, 19)$.
4. **Test `MutualDisplacementWaterOil`**:
   - Place Oil at $(30, 30)$ and Water at $(30, 29)$.
   - Step physics 1 tick.
   - Assert Water is at $(30, 30)$ and Oil is displaced up to $(30, 29)$.
5. **Test `LiquidLateralDispersionAndCliff`**:
   - Build a 3-block ledge: Stone at $(10, 20), (11, 20), (12, 20)$. Stone wall at $(9, 19)$. Air at $(13, 20)$ (cliff).
   - Place Water at $(10, 19)$ with dispersion 4.
   - Step physics 1 tick.
   - Assert Water moved to $(13, 19)$ (cliff edge) without skipping into $(14, 19)$.
6. **Test `BrushBresenhamContinuity`**:
   - Fast stroke between $(10, 10)$ and $(30, 30)$ with radius 1.
   - Count cells painted: must equal exactly 21 connected 8-neighbor cells.
