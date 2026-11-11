# Project: High-Performance 2D Modular Sandbox Game Engine

## Architecture

The engine is engineered in C++20 for Windows x64 with SDL2, Dear ImGui, and CMake. To guarantee both high-performance interactive simulation and headless deterministic testing/benchmarking, the system is strictly decoupled into modular layers:

```
+-------------------------------------------------------------------------------+
|                       Presentation & Application Layer                         |
|   - GUI (Dear ImGui): Material Palette, Brush Options, Sim & Gravity Controls|
|   - Windowing & Rendering (SDL2 / Streaming Texture / GL_NEAREST)             |
|   - F3 Debug Overlay: FPS, TPS, Active Chunks & Cells Metrics                 |
|   - CLI Entry Point: Interactive GUI mode vs Headless --benchmark mode        |
+-------------------------------------------------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
|                             Simulation Core (SoA)                             |
|   - World: Grid of 64x64 Chunks (e.g. 20x12 = 240 Chunks = 983,040 cells)    |
|   - Chunk: Cache-line aligned (alignas(64)), 32 KB SoA arrays (L1 resident)   |
|   - MaterialRegistry: Fast ID lookups, JSON asset loading (assets/materials/) |
|   - Physics: POWDER, LIQUID, GAS, FIRE, Mutual Density Displacement           |
|   - Thermodynamics: Cellular heat diffusion, phase transitions, latent heat   |
|   - ReactionSystem: O(1) reaction table compiled from reactions.json          |
|   - ExplosionSystem: Raycast shockwave, destruction, thermal blast            |
|   - Multithreading: 4-Phase Checkerboard parallel simulation, sleep cascade    |
+-------------------------------------------------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
|                           Storage & Infrastructure                            |
|   - RLE Serialization: Fast binary save/load format for full world state      |
|   - ThreadPool & Task Dispatcher: Zero-allocation concurrent batching         |
|   - Headless Benchmark Harness & CTest Suite                                  |
+-------------------------------------------------------------------------------+
```

### Data Flow
1. **Input / UI**: Player uses mouse brush (LMB, RMB, wheel) or ImGui controls to modify cells in the `World`.
2. **Simulation Step**:
   - Simulation tick executes at fixed 60 TPS ($\Delta t = 16.666$ ms).
   - 4-Phase Checkerboard schedules active chunks across worker threads in spatial phases `(cx & 1) | ((cy & 1) << 1)`.
   - Physics (gravity, density swaps, dispersion) and chemistry (reactions, fire, thermodynamics) update cell arrays.
   - Sleeping chunks with zero activity are skipped; activity on chunk boundaries cascades wake-up signals.
3. **Render Step**:
   - `WorldRenderer` maps active chunk SoA material arrays into a 32-bit RGBA streaming texture (`SDL_UpdateTexture` / `SDL_LockTexture`).
   - SDL renderer draws the texture scaled with nearest-neighbor interpolation (`GL_NEAREST`).
   - ImGui draws the user interface and F3 debug statistics on top.

---

## Code Layout

```
c:\Users\Drilla\Desktop\Sandbox/
├── CMakeLists.txt                      # Root build configuration and targets
├── assets/
│   ├── config.json                     # Default window & simulation configuration
│   ├── reactions.json                  # Declarative chemical & physical reaction rules
│   └── materials/                      # JSON material definitions (18+ materials)
│       ├── air.json
│       ├── sand.json
│       ├── water.json
│       ├── stone.json
│       ├── ...
├── src/
│   ├── main.cpp                        # Entry point (GUI and --benchmark CLI dispatcher)
│   ├── core/                           # ThreadPool, Types, Math, Config, Log, Timer
│   │   ├── Types.hpp
│   │   ├── Math.hpp
│   │   ├── Config.hpp / Config.cpp
│   │   ├── ThreadPool.hpp / ThreadPool.cpp
│   │   ├── Timer.hpp
│   │   └── Log.hpp
│   ├── sim/                            # Core simulation engine (no SDL/ImGui dependency)
│   │   ├── Chunk.hpp / Chunk.cpp       # 64x64 SoA Chunk layout, cache alignment
│   │   ├── Material.hpp                # Material properties struct & state enums
│   │   ├── MaterialRegistry.hpp / .cpp # JSON loader & material registry
│   │   ├── World.hpp / World.cpp       # World grid of chunks, coordinate queries
│   │   ├── Physics.hpp / Physics.cpp   # Powder, liquid, gas, fire movement & density swap
│   │   ├── Thermodynamics.hpp / .cpp   # Heat diffusion & phase transitions
│   │   ├── ReactionSystem.hpp / .cpp   # O(1) reaction table & reactions.json
│   │   ├── ExplosionSystem.hpp / .cpp  # Raycast shockwave, destruction, thermal blast
│   │   ├── Simulator.hpp / Simulator.cpp# 4-phase checkerboard & sleep manager
│   │   └── Serialization.hpp / .cpp    # Binary RLE world save/load
│   └── ui/                             # Presentation, rendering & input handling
│       ├── Renderer.hpp / Renderer.cpp # Streaming texture renderer (GL_NEAREST)
│       ├── Brush.hpp / Brush.cpp       # Interactive mouse brush tool
│       └── UI.hpp / UI.cpp             # Dear ImGui palette, controls, F3 overlay
└── tests/                              # CTest unit tests & benchmark harnesses
    ├── CMakeLists.txt                  # Test targets
    ├── test_physics.cpp                # Powder, liquid, displacement tests
    ├── test_thermo_reactions.cpp       # Heat diffusion, phase changes, reactions
    ├── test_rle_serialization.cpp      # Save/load roundtrip & compression tests
    ├── test_multithreading.cpp         # 4-phase checkerboard concurrency & sleep tests
    └── benchmark_runner.cpp            # Headless 300,000+ active cells @ 60 TPS runner
```

---

## Feature Inventory

Every feature identified in the Survey phase is mapped to a concrete milestone below:

| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | 64x64 SoA Chunk Layout | Structure-of-Arrays cache-line aligned (alignas(64)) 32KB payload fitting in L1 cache | M1 | R1, Survey |
| 2 | MaterialRegistry & Schema | Material struct & JSON loading from assets/materials/*.json with 18 core materials | M1 | R1, Survey |
| 3 | Core Materials Assets | JSON assets for air, sand, water, stone, dirt, wood, acid, lava, ice, steam, oil, etc. | M1 | R1, Survey |
| 4 | Powder Cellular Automata | Gravity fall, angle-of-repose sliding, resting pile formation | M1 | R1, Survey |
| 5 | Liquid Cellular Automata | Gravity fall, lateral dispersion with cliff drop-off priority | M1 | R1, Survey |
| 6 | Density Mutual Displacement | Symmetric particle swapping based on density (sand sinks in water, water sinks in oil) | M1 | R1, Survey |
| 7 | Streaming Texture Rendering | Efficient pixel buffer upload and SDL_Texture rendering with GL_NEAREST filtering | M1 | R1, Survey |
| 8 | Interactive Mouse Brush | LMB draw, RMB erase, mouse wheel radius adjustment, continuous stroke interpolation | M1 | R1, Survey |
| 9 | Cellular Heat Diffusion | Discrete 2D heat conduction between neighbors with harmonic mean contact conductivity | M2 | R2, Survey |
| 10 | Material Phase Transitions | Melting, boiling, condensation and freezing when thresholds crossed, with latent heat | M2 | R2, Survey |
| 11 | ReactionSystem & Table | Declarative reactions.json parsed into O(1) dense 2D lookup table | M2 | R2, Survey |
| 12 | Gas Cellular Automata | Upward buoyancy, Brownian lateral dispersion, lifetime decay | M2 | R2, Survey |
| 13 | Fire & Combustion | High-temperature energetic state, flammability ignition, smoke and ash emission | M2 | R2, Survey |
| 14 | ExplosionSystem | Raycast-shadowed shockwave, strength-based destruction, thermal blast, chain reactions | M2 | R2, Survey |
| 15 | 4-Phase Checkerboard Concurrency | Thread-safe parallel chunk simulation without per-cell locking or data races | M3 | R3, Survey |
| 16 | Sleeping Chunks & Wakeup Cascade | Skip inactive chunks, cascade wake-up on boundary particle, heat, or shockwave | M3 | R3, Survey |
| 17 | 300,000+ Cells @ 60 TPS Performance | Zero-allocation batching, active-row bounding boxes, tick budget <= 16.666 ms | M3 | R3, Survey |
| 18 | Dear ImGui Category Palette | Material selection grouped by category (Solid, Powder, Liquid, Gas, Energy, Tools) | M4 | R4, Survey |
| 19 | UI Simulation Controls | Pause, step, simulation speed slider, 2D gravity vector direction and magnitude | M4 | R4, Survey |
| 20 | F3 Debug Telemetry Overlay | Real-time counters for FPS, TPS, tick duration, active chunk count, active cell count | M4 | R4, Survey |
| 21 | Binary RLE Save/Load | Lossless run-length encoded binary world serialization and deserialization | M4 | R4, Survey |
| 22 | Headless CLI Benchmark | `--benchmark` CLI flag executing 300k+ active cells simulation at 60 TPS without GUI | M4 | R4, Survey |
| 23 | CTest Modular Test Suite | Unit tests for physics, thermodynamics, reactions, RLE, and multithreading | M4 | R4, Survey |
| 24 | Documentation (README/ARCH) | Comprehensive build instructions, architecture guide, and module documentation | M4 | R4, Survey |
| 25 | E2E Test Suite Validation | 100% pass rate across Tier 1 (Feature), Tier 2 (Boundary), Tier 3 (Pairwise), Tier 4 (Workload) | M5 | R1-R4, Dual Track |
| 26 | Adversarial Hardening (Tier 5) | White-box adversarial testing, edge case stress-testing, coverage hardening | M5 | Dual Track |

---

## Milestones

| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | World, Chunks, MaterialRegistry & Basic Physics/Rendering (ШАГ 2) | SoA Chunk 64x64, MaterialRegistry, 18 JSON materials, Powder & Liquid physics, density displacement, streaming texture renderer, brush tool | none | DONE |
| M2 | Thermodynamics, Reactions, Gases & Explosions (ШАГ 3) | Heat diffusion, phase transitions, reactions.json & ReactionSystem O(1) table, Gas & Fire physics, ExplosionSystem shockwave | M1 | IN_PROGRESS |
| M3 | Multithreaded Simulation & Performance Optimization (ШАГ 4) | 4-Phase Checkerboard parallel updater, sleeping chunks & cascade wake-up, optimization for 300k+ active cells @ 60 TPS | M2 | PLANNED |
| M4 | UI, RLE Save/Load, Benchmark, CTest & Docs (ШАГ 5) | Dear ImGui palette/controls/F3 overlay, binary RLE save/load, headless `--benchmark`, CTest unit test suite, README & ARCHITECTURE | M3 | PLANNED |
| M5 | Final Milestone: E2E Test Suite & Adversarial Hardening | Phase 1: 100% E2E test pass (Tiers 1-4); Phase 2: Tier 5 adversarial coverage hardening | M4, TEST_READY.md | PLANNED |

---

## Interface Contracts

### Chunk & World ↔ Physics & Simulation
```cpp
namespace sandbox::sim {

using MaterialId = core::u16;
inline constexpr MaterialId EMPTY_MATERIAL_ID = 0;

enum CellFlags : core::u8 {
    FLAG_NONE     = 0x00,
    FLAG_PARITY   = 0x01,
    FLAG_SLEEPING = 0x02,
    FLAG_BURNING  = 0x04,
    FLAG_FALLING  = 0x08
};

class alignas(core::CACHE_LINE_SIZE) Chunk {
public:
    static constexpr core::i32 SIZE = 64;
    static constexpr core::i32 AREA = SIZE * SIZE;

    alignas(core::CACHE_LINE_SIZE) MaterialId material[AREA]{0};
    alignas(core::CACHE_LINE_SIZE) core::f32  temperature[AREA]{20.0f};
    alignas(core::CACHE_LINE_SIZE) core::u8   flags[AREA]{0};
    alignas(core::CACHE_LINE_SIZE) core::u8   lifetime[AREA]{0};

    core::i32 chunkX{0};
    core::i32 chunkY{0};
    core::u32 activeCellCount{0};
    bool isDirty{true};
    bool isSleeping{false};

    inline static constexpr core::i32 Index(core::i32 lx, core::i32 ly) noexcept {
        return (ly << 6) | lx;
    }
};

class World {
public:
    World(core::i32 width, core::i32 height);
    Chunk* GetChunk(core::i32 cx, core::i32 cy);
    const Chunk* GetChunk(core::i32 cx, core::i32 cy) const;
    bool SetCell(core::i32 x, core::i32 y, MaterialId mat, core::f32 temp = 20.0f);
    MaterialId GetCellMaterial(core::i32 x, core::i32 y) const;
    core::f32 GetCellTemperature(core::i32 x, core::i32 y) const;
    void SwapCells(core::i32 x1, core::i32 y1, core::i32 x2, core::i32 y2);
    void WakeNeighbors(core::i32 cx, core::i32 cy);
};

}
```

### MaterialRegistry
```cpp
namespace sandbox::sim {

enum class MaterialState : core::u8 {
    Solid, Powder, Liquid, Gas
};

struct Material {
    MaterialId id{0};
    std::string name;
    std::string category;
    core::Color4u color{255, 255, 255, 255};
    core::u8 colorVariation{0};
    MaterialState state{MaterialState::Solid};
    core::f32 density{1000.0f};
    core::i32 dispersion{1};
    core::f32 flammability{0.0f};
    core::f32 ignitionTemp{300.0f};
    core::f32 meltPoint{9999.0f};
    core::f32 boilPoint{9999.0f};
    core::f32 freezePoint{-9999.0f};
    core::f32 specificHeat{1.0f};
    core::f32 thermalConductivity{0.5f};
    core::f32 strength{10.0f};
    core::u8 lifetime{0};
    MaterialId meltMaterialId{0};
    MaterialId boilMaterialId{0};
    MaterialId freezeMaterialId{0};
};

class MaterialRegistry {
public:
    static MaterialRegistry& Instance();
    bool LoadFromDirectory(const std::string& directoryPath);
    MaterialId RegisterMaterial(Material mat);
    const Material& Get(MaterialId id) const;
    MaterialId FindByName(const std::string& name) const;
    size_t Count() const;
};

}
```

### ReactionSystem
```cpp
namespace sandbox::sim {

struct ReactionResult {
    MaterialId productA{0};
    MaterialId productB{0};
    core::f32 heatDelta{0.0f};
    core::f32 probability{1.0f};
    bool explode{false};
    core::f32 explosionPower{0.0f};
    core::i32 explosionRadius{0};
};

class ReactionSystem {
public:
    static ReactionSystem& Instance();
    bool LoadFromFile(const std::string& jsonPath, const MaterialRegistry& registry);
    const ReactionResult* GetReaction(MaterialId a, MaterialId b) const;
};

}
```

### Serialization
```cpp
namespace sandbox::sim {

struct SaveHeader {
    char magic[4] = {'P', 'I', 'X', 'S'};
    core::u16 version = 1;
    core::i32 worldWidth;
    core::i32 worldHeight;
    core::u32 chunkCount;
    core::u32 totalActiveCells;
};

bool SaveWorldToFile(const World& world, const std::string& filePath);
bool LoadWorldFromFile(World& world, const std::string& filePath);

}
```
