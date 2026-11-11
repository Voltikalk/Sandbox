# Survey Phase 1: Codebase & Build Architecture Survey

## 1. Observation

### 1.1 Workspace Directory Structure & Layout
Inspection of workspace `c:\Users\Drilla\Desktop\Sandbox` (excluding build artifacts and external vendor trees) identified the following files and directories:

```
c:\Users\Drilla\Desktop\Sandbox/
├── CMakeLists.txt                     # Main CMake configuration (2,616 bytes)
├── CMakePresets.json                  # CMake presets for MSVC x64 and Ninja (1,394 bytes)
├── imgui.ini                          # Cached Dear ImGui window layout (124 bytes)
├── ORIGINAL_REQUEST.md                # Full user specification & requirements (9,191 bytes)
├── vcpkg.json                         # Vcpkg manifest (207 bytes)
├── assets/                            # Runtime game assets copied next to .exe
│   ├── config.json                    # Simulation, window, and render parameters (469 bytes)
│   └── materials/
│       └── air.json                   # Base air material definition (458 bytes)
├── external/                          # Third-party vendored dependencies
│   ├── imgui/                         # Dear ImGui v1.90.5 source, headers, backends
│   ├── nlohmann/                      # nlohmann/json v3.11.3 (single-header json.hpp)
│   └── sdl2/                          # SDL v2.30.2 full source tree
├── src/                               # C++20 source code
│   ├── main.cpp                       # Application entry point & game loop (8,749 bytes)
│   └── core/                          # Core engine utilities
│       ├── Config.hpp / Config.cpp    # JSON-backed configuration structs (959 / 3,686 bytes)
│       ├── Log.hpp                    # Thread-safe ANSI colored logger (2,575 bytes)
│       ├── Math.hpp                   # Fast PRNG, spatial hashing, math helpers (2,596 bytes)
│       ├── ThreadPool.hpp / .cpp      # Multi-worker thread pool (2,134 / 3,533 bytes)
│       ├── Timer.hpp                  # HighResClock, FrameTimer, ScopeTimer (3,327 bytes)
│       └── Types.hpp                  # Fixed-width types, Point2i, Rect2i, Color4u (3,050 bytes)
└── tests/                             # Existing empty directory for unit tests
```

### 1.2 Dependencies & Integration Mechanism

#### SDL2 (v2.30.2)
- **Source location**: `external/sdl2` (verified via `external/sdl2/include/SDL_version.h`: `SDL_MAJOR_VERSION = 2`, `SDL_MINOR_VERSION = 30`, `SDL_PATCHLEVEL = 2`).
- **CMake Integration**:
  ```cmake
  # CMakeLists.txt:20-26
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/sdl2/CMakeLists.txt")
      set(SDL_SHARED OFF CACHE BOOL "" FORCE)
      set(SDL_STATIC ON CACHE BOOL "" FORCE)
      set(SDL_TEST OFF CACHE BOOL "" FORCE)
      add_subdirectory(external/sdl2 EXCLUDE_FROM_ALL)
      set(SDL2_TARGET "SDL2-static")
  ```
  SDL2 is compiled in-tree as a static library (`SDL2-static.lib`).
- **WinMain / Entry point**:
  In `src/main.cpp:1`:
  ```cpp
  #define SDL_MAIN_HANDLED
  #include <SDL.h>
  ```
  Standard `int main(int argc, char* argv[])` is used directly without SDL WinMain redirection.
- **Linked Windows system libraries**:
  ```cmake
  # CMakeLists.txt:83-85
  if(WIN32)
      target_link_libraries(sandbox PRIVATE winmm imm32 version setupapi user32 gdi32 shell32)
  endif()
  ```

#### Dear ImGui (v1.90.5)
- **Source location**: `external/imgui` (verified via `external/imgui/imgui.h`: `#define IMGUI_VERSION "1.90.5"`).
- **CMake Integration**:
  ```cmake
  # CMakeLists.txt:36-57
  add_library(imgui STATIC
      external/imgui/imgui.cpp
      external/imgui/imgui_demo.cpp
      external/imgui/imgui_draw.cpp
      external/imgui/imgui_tables.cpp
      external/imgui/imgui_widgets.cpp
      external/imgui/backends/imgui_impl_sdl2.cpp
      external/imgui/backends/imgui_impl_sdlrenderer2.cpp
  )
  target_include_directories(imgui PUBLIC external/imgui external/imgui/backends)
  target_link_libraries(imgui PUBLIC ${SDL2_TARGET})
  ```
  ImGui is compiled statically with MSVC `/W0` to suppress third-party compiler warnings.
- **Backends**: `imgui_impl_sdl2` (events & window) and `imgui_impl_sdlrenderer2` (rendering via `SDL_Renderer`).
- **Fonts & Localization**: `src/main.cpp:92-118` contains Cyrillic font loading logic scanning `C:\Windows\Fonts\segoeui.ttf`, `arial.ttf`, `tahoma.ttf` with `io.Fonts->GetGlyphRangesCyrillic()`.

#### nlohmann/json (v3.11.3)
- **Source location**: `external/nlohmann/json.hpp` (single header, 919 KB).
- **CMake Integration**: Included through `target_include_directories(sandbox PRIVATE external)`. Used via `#include <nlohmann/json.hpp>`.

#### vcpkg & Presets
- `vcpkg.json` is present with `sdl2`, `imgui`, and `nlohmann-json`, but the project is currently fully self-contained through `external/` without requiring external vcpkg installation.
- `CMakePresets.json` defines presets: `default` (Visual Studio 17 2022 x64), `release`, `debug`, and `ninja-release`.

### 1.3 ШАГ 1 Skeleton Implementation Details

#### Entry point & Lifecycle (`src/main.cpp`)
1. **Console initialization**:
   `SetConsoleOutputCP(CP_UTF8)`, `SetConsoleCP(CP_UTF8)`, and `ENABLE_VIRTUAL_TERMINAL_PROCESSING` enabled on Windows for ANSI color escape sequences (`main.cpp:27-39`).
2. **Config loading**:
   `AppConfig config; config.LoadFromFile("assets/config.json");` (`main.cpp:44-45`).
3. **SDL2 Window & Renderer creation**:
   - `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS)` (`main.cpp:48`).
   - Window created with `SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE` (`main.cpp:53-65`).
   - Renderer created with `SDL_RENDERER_ACCELERATED` and optional `SDL_RENDERER_PRESENTVSYNC` (`main.cpp:73-84`).
4. **ImGui context & style**:
   - `ImGui::CreateContext()`, Cyrillic font loading, `ImGui::StyleColorsDark()` with rounded corners (`main.cpp:87-130`).
5. **ThreadPool initialization**:
   - `ThreadPool threadPool(config.sim.threadCount);` (`main.cpp:133`).
6. **Main game loop**:
   - `FrameTimer frameTimer;` tracks delta time and smoothed 60-frame FPS (`main.cpp:137, 149`).
   - Fixed time step accumulator:
     `fixedTimeStep = 1.0 / static_cast<f64>(config.sim.targetTPS);` (default 60 TPS = 16.666 ms).
     Accumulates `dt * config.sim.timeScale` and executes substeps up to `maxSubsteps` (5).
     Includes death spiral guard: `if (simAccumulator > fixedTimeStep * maxSubsteps) simAccumulator = 0.0;` (`main.cpp:173-192`).
   - Event processing: `SDL_PollEvent`, forwarding to `ImGui_ImplSDL2_ProcessEvent`, F3 toggles debug overlay, ESC quits (`main.cpp:153-171`).
   - Debug overlay window (F3): displays FPS, frame time (ms), target TPS, sim tick duration (ms), tick counter, active worker threads, window size, world dimensions, pause checkbox, and time scale slider (`main.cpp:199-218`).
   - Clear screen (`SDL_RenderClear`) and ImGui render (`ImGui_ImplSDLRenderer2_RenderDrawData`) (`main.cpp:220-228`).
7. **Clean shutdown**:
   ImGui shutdown, SDL_DestroyRenderer, SDL_DestroyWindow, SDL_Quit (`main.cpp:234-241`).

#### Core Subsystems (`src/core/`)
- `Types.hpp`:
  - Fixed-size integers: `i8, i16, i32, i64, u8, u16, u32, u64, f32, f64`.
  - Constants: `CACHE_LINE_SIZE = 64`, `CHUNK_SIZE = 64`, `CHUNK_AREA = 4096`.
  - Math structs: `Point2i`, `Rect2i`.
  - Color type: `Color4u` (`alignas(4)`), RGBA packing to `u32`, unpacking from `u32`, and `Color4u::Lerp`.
- `Math.hpp`:
  - `FastRNG`: Deterministic Xorshift32 PRNG (`NextU32`, `NextInt`, `NextFloat`, `Chance`).
  - Spatial noise: `CoordinateHash(x, y, seed)` and `CellNoiseVariation(x, y, maxVariation)`.
  - Utilities: `DistanceSquared(x1, y1, x2, y2)`, `Sign(val)`.
- `Log.hpp`:
  - `Logger` singleton with mutex lock, log levels (`Debug`, `Info`, `Warning`, `Error`), millisecond timestamp formatting, ANSI color coding.
  - Helper macros: `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`.
- `Timer.hpp`:
  - `HighResClock`: Wrapper around `std::chrono::steady_clock` (seconds, ms, microseconds).
  - `FrameTimer`: Computes current delta time and rolling 60-frame average FPS/frametime.
  - `ScopeTimer`: RAII timer measuring execution duration into an output pointer.
- `Config.hpp` & `Config.cpp`:
  - `WindowConfig`, `SimConfig`, `RenderConfig`, and `AppConfig`.
  - Full JSON serialization and deserialization via nlohmann/json.
- `ThreadPool.hpp` & `ThreadPool.cpp`:
  - Multi-worker pool using `std::thread`, `std::mutex`, `std::condition_variable`.
  - Methods: `Enqueue<F, Args...>`, `WaitAll()`, and batch-partitioned `ParallelFor(count, loopBody)`.

### 1.4 Verification of Current Build
Executed `cmake --build build --config Release` directly in the environment:
```
Версия MSBuild 17.14.40+3e7442088 для .NET Framework
  SDL2-static.vcxproj -> C:\Users\Drilla\Desktop\Sandbox\build\external\sdl2\Release\SDL2-static.lib
  imgui.vcxproj -> C:\Users\Drilla\Desktop\Sandbox\build\Release\imgui.lib
  sandbox.vcxproj -> C:\Users\Drilla\Desktop\Sandbox\build\Release\sandbox.exe
```
Exit code: `0`.
Output artifact: `build/Release/sandbox.exe` (2,546,688 bytes).
Assets automatically copied to `build/Release/assets/` via POST_BUILD rule.

### 1.5 Missing Components Inventory (ШАГ 2 — ШАГ 5)
Direct comparison between `ORIGINAL_REQUEST.md` and current ШАГ 1 implementation:

| Module | Requirement | Status in ШАГ 1 | Required in Remaining Steps |
|---|---|---|---|
| **Simulation Grid** | 64x64 Chunks, SoA layout, cache line aligned | Only `CHUNK_SIZE = 64` defined in `Types.hpp` | **ШАГ 2**: Implement `Chunk` (SoA arrays for material, temp, flags, lifetime) and `World` |
| **Material System** | JSON loading, MaterialRegistry, physical properties | Only `air.json` exists in `assets/materials/` | **ШАГ 2**: Implement `MaterialRegistry`, full JSON parsing, material asset files (sand, water, stone, wood, oil, acid, etc.) |
| **Basic Physics** | POWDER (sand) & LIQUID (water) cellular automata | None | **ШАГ 2**: Falling, sliding, dispersion, density-based displacement |
| **World Renderer** | SDL streaming texture (`GL_NEAREST`) | None (only `SDL_RenderClear` solid color) | **ШАГ 2**: `SDL_CreateTexture` streaming RGBA8888, texture locking, nearest neighbor blit |
| **Brush Interaction** | LMB draw, RMB erase, mouse wheel radius | None | **ШАГ 2**: Screen-to-world coordinate transform, circular brush applicator |
| **Thermodynamics** | Heat diffusion, phase transitions | None | **ШАГ 3**: Fourier conduction between neighbors, latent heat, melt/freeze/boil triggers |
| **Reactions** | `reactions.json` parser, contact reactions | None | **ШАГ 3**: Fast $O(1)$ 2D matrix reaction table, acid etching, burning |
| **Atmosphere / Fire** | GAS & FIRE physics, buoyant rise, lifetime decay | None | **ШАГ 3**: Smoke, steam, fire particle simulation |
| **Explosions** | `ExplosionSystem`, blast wave, destruction | None | **ШАГ 3**: Raycast-shadowed shockwave, thermal flash, crater formation |
| **Parallel Simulation** | 4-phase checkerboard chunk decomposition | Single `ThreadPool` exists, but no chunk partitioning | **ШАГ 4**: Divide chunks into 4 non-adjacent groups, run each group in parallel via `ThreadPool::ParallelFor` |
| **Chunk Sleep / Wake** | Sleeping chunks, cascade boundary wake-up | None | **ШАГ 4**: Activity detection, skip sleeping chunks, wake neighbors when crossing edges |
| **Performance Benchmark**| 300,000+ active cells at $\ge 60$ TPS | None | **ШАГ 4 / ШАГ 5**: Optimize inner loops to maintain step time $\le 16.6$ ms |
| **User Interface** | Category palette, brush controls, gravity slider | Only basic F3 overlay exists | **ШАГ 5**: Full Dear ImGui docking/panels, material selection, simulation controls |
| **World Serialization** | Binary RLE save / load | None | **ШАГ 5**: Run-Length Encoding binary format for fast snapshot save & restore |
| **Automated Tests** | CTest unit test suite | `tests/` folder is empty, no `enable_testing()` in CMake | **ШАГ 5**: Create unit tests for physics, reactions, serialization, checkerboard |
| **Console Benchmark** | CLI headless mode (`--benchmark`) | CLI arguments are currently ignored (`(void)argc;`) | **ШАГ 5**: Headless test harness running 300k+ cells without SDL window |
| **Documentation** | `README.md` & `ARCHITECTURE.md` | Neither file exists | **ШАГ 5**: Write comprehensive engine documentation |

---

## 2. Logic Chain

### 2.1 Architectural Decoupling: Simulation vs Presentation
1. *Observation 1.3*: `main.cpp` currently manages SDL window creation, ImGui setup, event pump, and rendering inside a monolithic function.
2. *Observation 1.5*: Requirements demand both interactive GUI execution and headless console benchmarking (`--benchmark`), as well as CTest unit testing.
3. *Logical Deduction*: The simulation engine (`World`, `Chunk`, `MaterialRegistry`, `Simulator`, `Physics`, `Thermodynamics`, `Reactions`, `Explosions`, `Serialization`) must NOT depend on SDL2 or Dear ImGui.
4. *Conclusion*: The simulation core should be compiled as a standalone static library `sandbox_sim`.
   - The interactive GUI app `sandbox` links `sandbox_sim` + `imgui` + `SDL2-static`.
   - The test executable `sandbox_tests` links `sandbox_sim` without SDL or ImGui.
   - The benchmark runner can be executed either headless inside `sandbox --benchmark` or via a dedicated test/bench target.

### 2.2 Memory Alignment & SoA Cache Locality for 300,000+ Cells
1. *Observation 1.1 & 1.3*: `src/core/Types.hpp` defines `CHUNK_SIZE = 64`, `CHUNK_AREA = 4096`, and `CACHE_LINE_SIZE = 64`.
2. *Observation 1.5*: The world dimensions in `config.json` are $1280 \times 768$ (20 chunks horizontally by 12 chunks vertically = 240 chunks total, 983,040 cells). Requirement R3 requires simulating 300,000+ active cells at 60 TPS ($\le 16.6$ ms per tick).
3. *Hardware Analysis*: At 60 TPS, the budget per tick is 16.6 ms. Simulating 300,000 cells in 16 ms means each cell update must take $\le 53$ nanoseconds.
4. *Data Layout Reasoning*:
   - An AoS (Array of Structures) layout (`struct Cell { u16 id; f32 temp; u8 flags; u8 lifetime; }`) would disperse related data across memory, resulting in CPU cache misses during physics passes that only read material IDs.
   - A Structure of Arrays (SoA) layout aligned to 64 bytes (`alignas(64) MaterialId material[4096]`, `alignas(64) f32 temperature[4096]`, etc.) fits an entire chunk's primary state into 32 KB.
   - 32 KB matches the exact L1 Data Cache size of modern x86-64 CPUs.
   - When a thread processes a chunk, the entire chunk stays resident in L1 cache, eliminating main memory stalls.

### 2.3 Concurrency Architecture: 4-Phase Checkerboard
1. *Observation 1.3*: `src/core/ThreadPool.hpp` already provides an efficient `ParallelFor(count, loopBody)` that splits work across threads without per-task dynamic heap allocation.
2. *Problem*: If two adjacent chunks are simulated simultaneously on different threads, particles moving across chunk borders would produce data races on cell writes without locks. Locking individual cells or chunks would destroy 60 TPS performance.
3. *Mathematical Solution*: 4-Phase 2D Checkerboard decomposition:
   - Phase 0: Chunks with $(cx \pmod 2 == 0 \land cy \pmod 2 == 0)$
   - Phase 1: Chunks with $(cx \pmod 2 == 1 \land cy \pmod 2 == 0)$
   - Phase 2: Chunks with $(cx \pmod 2 == 0 \land cy \pmod 2 == 1)$
   - Phase 3: Chunks with $(cx \pmod 2 == 1 \land cy \pmod 2 == 1)$
4. *Invariance Proof*: No two chunks in the same phase share an edge or corner (minimum distance between any two chunks in the same phase is 1 full chunk = 64 cells).
5. *Execution*: Each phase can safely update its set of chunks concurrently via `threadPool.ParallelFor(...)` with a sync barrier (`WaitAll()`) between phases. This guarantees 100% determinism and 0 data races without any cell-level mutexes.

### 2.4 Streaming Texture Rendering Efficiency
1. *Observation 1.2*: SDL2 renderer is initialized with `SDL_RENDERER_ACCELERATED`.
2. *Observation 1.5*: R1 specifies rendering the pixel world through a streaming texture with `GL_NEAREST` filtering.
3. *Rendering Pipeline*:
   - Allocate `SDL_Texture* m_worldTexture` with `SDL_TEXTUREACCESS_STREAMING`, format `SDL_PIXELFORMAT_RGBA32`, size $1280 \times 768$.
   - Call `SDL_SetTextureScaleMode(m_worldTexture, SDL_ScaleModeNearest)` to achieve `GL_NEAREST` pixel-art scaling without blur.
   - In each frame, lock the texture (`SDL_LockTexture`), copy/convert active chunk colors into the mapped pixel pointer, unlock (`SDL_UnlockTexture`), and draw with `SDL_RenderCopy`.
   - Sleeping or unchanged chunks do not need color recomputation, only copying cached RGB colors.

---

## 3. Caveats

1. **No Existing Unit Tests**: The `tests/` directory is currently empty and `CMakeLists.txt` does not include `enable_testing()`. Testing infrastructure must be integrated in CMake before unit tests can be executed via `ctest`.
2. **Environment Toolchain Specifics**: `ninja` and `cl.exe` are not present in the default system `PATH` of this shell. However, `cmake` (v4.3.2) and `MSBuild` (v17.14.40+) function properly and compile the project using `cmake --build build --config Release`.
3. **vcpkg Manifest vs Vendored Sources**: Although `vcpkg.json` exists in the repository root, CMakeLists.txt is configured to consume dependencies directly from `external/`. Building with vcpkg is not required and would introduce redundant builds; the vendored libraries (`external/sdl2`, `external/imgui`, `external/nlohmann`) must remain the primary source of truth.
4. **SDL WinMain Handling**: Because `#define SDL_MAIN_HANDLED` is used, the executable must maintain a standard `int main(int argc, char* argv[])` signature.

---

## 4. Conclusion & Modular Architecture Design

### 4.1 Target Source Tree Layout
To facilitate clean separation of concerns and parallel agent implementation across ШАГ 2 - ШАГ 5, the codebase should be organized into the following modules:

```
src/
├── core/                              # [EXISTING ШАГ 1]
│   ├── Types.hpp
│   ├── Math.hpp
│   ├── Log.hpp
│   ├── Timer.hpp
│   ├── Config.hpp / Config.cpp
│   └── ThreadPool.hpp / ThreadPool.cpp
├── sim/                               # [SIMULATION ENGINE - ШАГ 2, 3, 4, 5]
│   ├── Cell.hpp                       # Cell bit flags, CellView lightweight accessor
│   ├── Chunk.hpp / Chunk.cpp          # 64x64 SoA aligned chunk container & lifecycle
│   ├── World.hpp / World.cpp          # 2D chunk grid, spatial indexing, step orchestration
│   ├── Material.hpp                   # MaterialDef, MaterialState enum (STATIC, SOLID, POWDER, LIQUID, GAS)
│   ├── MaterialRegistry.hpp / .cpp    # JSON loader for assets/materials/*.json, name/ID lookup
│   ├── Physics.hpp / Physics.cpp      # POWDER falling/sliding, LIQUID dispersion & density displacement
│   ├── Thermodynamics.hpp / .cpp      # Discrete cellular heat diffusion, phase transitions
│   ├── Reactions.hpp / Reactions.cpp  # reactions.json parser, O(1) reaction lookup table
│   ├── Explosions.hpp / Explosions.cpp# Radial raycast shockwave, destruction, thermal blast
│   ├── Simulator.hpp / Simulator.cpp  # 4-phase checkerboard parallel runner & sleeping chunk manager
│   └── Serialization.hpp / .cpp       # Binary RLE world save/load format
├── gfx/                               # [RENDERING & CAMERA - ШАГ 2]
│   ├── Renderer.hpp / Renderer.cpp    # SDL streaming texture, pixel buffer blit, GL_NEAREST scaling
│   └── Camera.hpp / Camera.cpp        # Viewport zoom, pan, screen-to-world coordinate transformations
├── ui/                                # [USER INTERFACE & TOOLS - ШАГ 2, 5]
│   ├── Brush.hpp / Brush.cpp          # Tool system: LMB draw, RMB erase, wheel radius, shapes
│   └── UI.hpp / UI.cpp                # ImGui palette, category tabs, sim sliders, F3 overlay
└── app/                               # [APPLICATION LIFECYCLE - ШАГ 1, 5]
    ├── App.hpp / App.cpp              # Application coordinator, game loop, event dispatcher
    └── Benchmark.hpp / Benchmark.cpp  # Headless CLI benchmark runner (300k+ cells @ 60 TPS)
```

### 4.2 Proposed CMakeLists.txt Architecture
To support unit testing with CTest and headless benchmarking without code duplication, `CMakeLists.txt` should be refactored into modular targets:

```cmake
# --- Target 1: sandbox_sim (Static library, zero GUI dependencies) ---
add_library(sandbox_sim STATIC
    src/core/Config.cpp
    src/core/ThreadPool.cpp
    src/sim/Chunk.cpp
    src/sim/World.cpp
    src/sim/MaterialRegistry.cpp
    src/sim/Physics.cpp
    src/sim/Thermodynamics.cpp
    src/sim/Reactions.cpp
    src/sim/Explosions.cpp
    src/sim/Simulator.cpp
    src/sim/Serialization.cpp
)
target_include_directories(sandbox_sim PUBLIC src external)

# --- Target 2: sandbox (Main interactive game executable) ---
add_executable(sandbox
    src/main.cpp
    src/gfx/Renderer.cpp
    src/gfx/Camera.cpp
    src/ui/Brush.cpp
    src/ui/UI.cpp
    src/app/App.cpp
    src/app/Benchmark.cpp
)
target_link_libraries(sandbox PRIVATE sandbox_sim imgui ${SDL2_TARGET})
if(WIN32)
    target_link_libraries(sandbox PRIVATE winmm imm32 version setupapi user32 gdi32 shell32)
endif()

# --- Target 3: sandbox_tests (CTest automated test suite) ---
enable_testing()
add_executable(sandbox_tests
    tests/test_main.cpp
    tests/test_materials.cpp
    tests/test_physics.cpp
    tests/test_thermodynamics.cpp
    tests/test_reactions.cpp
    tests/test_serialization.cpp
    tests/test_checkerboard.cpp
)
target_link_libraries(sandbox_tests PRIVATE sandbox_sim)
add_test(NAME AllUnitTests COMMAND sandbox_tests)
```

### 4.3 Recommended Step-by-Step Implementation Roadmap

#### ШАГ 2 (World, Chunks, Physics, Streaming Texture, Brush)
1. **Core Data Structures**:
   - Implement `Chunk` (SoA layout, 4096 cells per array, `alignas(64)`).
   - Implement `World` (vector of chunks $20 \times 12$, chunk indexing, coordinate boundary clamping).
2. **Material System**:
   - Implement `Material` struct matching `air.json` schema.
   - Implement `MaterialRegistry` scanning and parsing `assets/materials/*.json`.
   - Populate essential material assets: `sand.json`, `water.json`, `stone.json`, `wood.json`, `oil.json`, `acid.json`, `glass.json`, `iron.json`.
3. **Cellular Automata Physics**:
   - `POWDER`: Check straight down $(x, y+1)$; if blocked, check $(x \pm 1, y+1)$ with randomized horizontal preference.
   - `LIQUID`: Check down $(x, y+1)$; if blocked, disperse horizontally $(x \pm 1, y)$ up to dispersion limit.
   - Density displacement: If target cell contains lighter material ($\rho_{\text{dest}} < \rho_{\text{src}}$), swap cells instead of blocking.
4. **Rendering & Brush**:
   - Implement `Renderer` with streaming texture `SDL_PIXELFORMAT_RGBA32` and `SDL_ScaleModeNearest`.
   - Implement `Brush` (circle rasterization with radius $r$, LMB write material, RMB erase to air, wheel change radius).

#### ШАГ 3 (Thermodynamics, Reactions, Gases, Explosions)
1. **Thermodynamics**:
   - 4-neighbor cellular heat conduction with harmonic mean thermal conductivity.
   - Phase change check: if $T \ge \text{melt\_point}$, convert to liquid; if $T \ge \text{boil\_point}$, convert to gas; if $T \le \text{freeze\_point}$, convert to solid.
2. **Reactions**:
   - Create `assets/reactions.json`.
   - Compile reactions into dense 2D lookup array `ReactionRule* lookup[MAX_MAT][MAX_MAT]`.
   - Execute binary contact reactions during simulation tick (e.g., Water + Fire $\to$ Steam, Acid + Metal $\to$ Corrosion).
3. **Atmosphere & Fire**:
   - `GAS`: Upward movement $(x, y-1)$ with Brownian drift, lifetime decay to air.
   - `FIRE`: High temperature, ignites adjacent flammables, decays to smoke.
4. **Explosion System**:
   - Radial Bresenham raycast shockwave penetrating materials based on strength.
   - Thermal radiation pulse heating surrounding cells and triggering secondary blasts.

#### ШАГ 4 (Multithreading & Performance Optimization)
1. **4-Phase Checkerboard Decomposition**:
   - Divide active chunks into 4 phase vectors.
   - Run each phase via `ThreadPool::ParallelFor`.
2. **Sleeping Chunks Optimization**:
   - Track `isDirty` and active particle count per chunk.
   - If no cell moved or reacted for $K$ ticks, put chunk to sleep.
   - Awake neighbor chunks whenever a particle moves within 1 cell of the chunk boundary.
3. **Performance Profiling**:
   - Ensure tick time for 300,000 active cells remains $\le 16.6$ ms (60 TPS).

#### ШАГ 5 (UI, Serialization, CTest, Benchmark, Documentation)
1. **Dear ImGui Interface**:
   - Material palette grouped by categories (Solids, Powders, Liquids, Gases, Explosives).
   - Sliders for simulation speed, gravity vector, brush radius, and brush shape.
   - Enhanced F3 debug overlay with live TPS graph, active cells counter, and sleeping chunks count.
2. **Binary RLE Serialization**:
   - Binary format: Magic header (`SB2D`), world dimensions, chunk count, followed by per-chunk RLE-compressed arrays.
   - Save / load buttons in ImGui and hotkeys (F5 quicksave, F9 quickload).
3. **CTest Unit Tests**:
   - Test physics rules (powder angle of repose, liquid leveling, density sorting).
   - Test thermal conduction and phase transitions.
   - Test reaction rules.
   - Test RLE round-trip integrity.
4. **CLI Benchmark Mode**:
   - Add argument parsing for `--benchmark`.
   - Spawn a world with 300,000 active sand/water cells, run 300 ticks headless, measure min/max/avg tick times, assert $\ge 60$ TPS.
5. **Documentation**:
   - Create comprehensive `README.md` and `ARCHITECTURE.md`.

---

## 5. Verification Method

To independently verify the observations and conclusions of this survey:

1. **Verify Build of ШАГ 1**:
   Run the following command from the workspace root:
   ```powershell
   cmake --build build --config Release
   ```
   **Expected result**: MSBuild compiles `SDL2-static.lib`, `imgui.lib`, and `sandbox.exe` with exit code `0`.
2. **Verify Output Binary & Asset Placement**:
   Inspect the output directory:
   ```powershell
   Test-Path build\Release\sandbox.exe
   Test-Path build\Release\assets\config.json
   Test-Path build\Release\assets\materials\air.json
   ```
   **Expected result**: All paths return `True`.
3. **Verify Header & Types Consistency**:
   Inspect `src/core/Types.hpp` lines 24–28:
   - `CACHE_LINE_SIZE` is `64`.
   - `CHUNK_SIZE` is `64`.
   - `CHUNK_AREA` is `4096`.
4. **Verify ImGui Cyrillic Font Support**:
   Inspect `src/main.cpp` lines 96–115 to confirm fallback loading of `segoeui.ttf`, `arial.ttf`, or `tahoma.ttf`.
5. **Future Invalidation Conditions**:
   This survey report's conclusions remain valid unless:
   - Third-party dependencies are upgraded or relocated from `external/`.
   - The CMake generator or target compiler standard is changed from C++20.
   - The chunk dimension is modified from $64 \times 64$.
