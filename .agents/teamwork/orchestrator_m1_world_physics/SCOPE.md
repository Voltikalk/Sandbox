# Scope: Milestone 1 (World, Chunks, MaterialRegistry & Basic Physics/Rendering - ШАГ 2)

## Architecture & Responsibilities
Milestone 1 delivers the fundamental building blocks of the pixel sandbox:
1. **Simulation Grid & Data Structure**:
   - 64x64 SoA Chunk layout (`material`, `temperature`, `flags`, `lifetime` 4096-element arrays), aligned to 64-byte cache lines (`alignas(64)`).
   - `World` class managing 2D grid of chunks (default 20x12 = 240 chunks = 1280x768 cells), world-to-chunk coordinate translation, and cell accessors.
2. **Material System**:
   - `Material` definition struct with states (`Solid`, `Powder`, `Liquid`, `Gas`), thermal properties, physics parameters (`density`, `dispersion`), color & variation.
   - `MaterialRegistry` singleton parsing 18 core material JSON files in `assets/materials/`.
3. **Cellular Automata Physics**:
   - `FLAG_PARITY = 0x01` double-update prevention bit and bottom-to-top traversal order.
   - Powder movement: downward fall, diagonal angle-of-repose sliding.
   - Liquid movement: downward fall, diagonal sliding, lateral dispersion up to `dispersion` cells with cliff drop-off priority.
   - Symmetric mutual density displacement: heavier particles displace lighter particles ($A \leftrightarrow B$ swap) for natural sinking/buoyancy.
4. **Presentation & Interaction**:
   - `WorldRenderer`: streaming texture upload (`SDL_UpdateTexture`/`SDL_LockTexture`) with `GL_NEAREST` filtering.
   - `Brush`: mouse tool supporting LMB painting, RMB erasing (Air), mouse wheel radius, and continuous Bresenham stroke interpolation.
   - Integration into `src/main.cpp` interactive loop with clean compilation in CMake.

## Feature Inventory (Milestone 1)
| # | Feature | Description | Status |
|---|---------|-------------|--------|
| 1 | 64x64 SoA Chunk Layout | Structure-of-Arrays cache-line aligned (alignas(64)) 32KB payload fitting in L1 cache | DONE |
| 2 | MaterialRegistry & Schema | Material struct & JSON loading from assets/materials/*.json with 18 core materials | DONE |
| 3 | Core Materials Assets | JSON assets for air, sand, water, stone, dirt, wood, acid, lava, ice, steam, oil, etc. | DONE |
| 4 | Powder Cellular Automata | Gravity fall, angle-of-repose sliding, resting pile formation | DONE |
| 5 | Liquid Cellular Automata | Gravity fall, lateral dispersion with cliff drop-off priority | DONE |
| 6 | Density Mutual Displacement | Symmetric particle swapping based on density (sand sinks in water, water sinks in oil) | DONE |
| 7 | Streaming Texture Rendering | Efficient pixel buffer upload and SDL_Texture rendering with GL_NEAREST filtering | DONE |
| 8 | Interactive Mouse Brush | LMB draw, RMB erase, mouse wheel radius adjustment, continuous stroke interpolation | DONE |

## Interface Contracts
- `src/core/Types.hpp`: Primitive types (`u8`, `u16`, `u32`, `i32`, `f32`, `Color4u`, `CACHE_LINE_SIZE = 64`).
- `src/sim/Chunk.hpp`: `Chunk` class with `material[4096]`, `temperature[4096]`, `flags[4096]`, `lifetime[4096]`.
- `src/sim/Material.hpp`: `Material` struct, `MaterialState` enum, `MaterialId`.
- `src/sim/MaterialRegistry.hpp`: `MaterialRegistry::Instance()`, `LoadFromDirectory()`, `RegisterMaterial()`, `Get()`, `FindByName()`.
- `src/sim/World.hpp`: `World` chunk grid, `SetCell`, `GetCellMaterial`, `GetCellTemperature`, `SwapCells`, coordinate mapping.
- `src/sim/Physics.hpp`: Physics update step for Powder and Liquid, parity toggle, density swap logic.
- `src/ui/Renderer.hpp`: `WorldRenderer` creating SDL streaming texture, rendering active chunk pixels, `GL_NEAREST`.
- `src/ui/Brush.hpp`: `Brush` tool handling mouse clicks, radius, material placement.

## Code Layout (Owned Files for M1 Worker)
- `assets/materials/*.json` (all 18 material files)
- `src/core/Types.hpp`
- `src/sim/Material.hpp`
- `src/sim/Chunk.hpp` / `src/sim/Chunk.cpp`
- `src/sim/MaterialRegistry.hpp` / `src/sim/MaterialRegistry.cpp`
- `src/sim/World.hpp` / `src/sim/World.cpp`
- `src/sim/Physics.hpp` / `src/sim/Physics.cpp`
- `src/ui/Renderer.hpp` / `src/ui/Renderer.cpp`
- `src/ui/Brush.hpp` / `src/ui/Brush.cpp`
- `src/main.cpp`
- `CMakeLists.txt`
