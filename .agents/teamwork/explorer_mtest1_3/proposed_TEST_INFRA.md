# TEST_INFRA.md — End-to-End Test Infrastructure & Specification

## 1. Executive Summary & Test Philosophy

This document defines the specification, architecture, methodologies, and quality gates for the automated end-to-end (E2E) testing suite of the **High-Performance 2D Modular Sandbox Game Engine**.

```
+---------------------------------------------------------------------------------------+
|                                    TEST PHILOSOPHY                                    |
|                                                                                       |
|   1. Opaque-Box & Requirement-Driven                                                  |
|      - Tests validate macroscopic behavior against requirements in ORIGINAL_REQUEST   |
|        and PROJECT.md, querying only public contracts (World, Chunk, Registry).       |
|      - Zero coupling to internal private implementations or data layouts.             |
|                                                                                       |
|   2. 100% Headless Execution                                                          |
|      - Independent of SDL2 windowing, OpenGL/DirectX GPU context, and Dear ImGui.     |
|      - Executes in pure CPU memory space; fully deterministic, fast, CI-ready.        |
|                                                                                       |
|   3. Multi-Tiered Stratification                                                      |
|      - Tier 1: Functional Equivalence Partitioning (>= 5 tests / feature)             |
|      - Tier 2: Boundary Value & Extreme Stress Testing (>= 5 tests / feature)         |
|      - Tier 3: Pairwise Combinatorial Interactions (All-Pairs feature matrices)       |
|      - Tier 4: Real-World Workload Scenarios (Full physical-chemical macro dynamics)  |
|      - Tier 5: Adversarial Hardening (Fault injection, fuzzing, corruption recovery)  |
+---------------------------------------------------------------------------------------+
```

### Core Principles
- **Opaque-Box Testing**: All test assertions execute strictly against public API surfaces (`sandbox::sim::World`, `sandbox::sim::MaterialRegistry`, `sandbox::sim::ReactionSystem`, `sandbox::sim::SaveWorldToFile`, `sandbox::sim::LoadWorldFromFile`). Tests never inspect private class members or assume undocumented internal ordering.
- **Requirement-Driven Traceability**: Every test case explicitly maps to one or more requirements from `ORIGINAL_REQUEST.md` (R1 to R4) and features from `PROJECT.md` (F1 to F26).
- **Headless Determinism**: The entire test suite runs without opening an OS window or creating a graphics/audio context. Simulation steps are ticked deterministically with fixed $\Delta t = 16.666$ ms (60 TPS). For any given seed and initial cell configuration, simulation outcomes must be bit-for-bit identical across runs.
- **No Reliance on Visuals**: Physical correctness is evaluated via quantitative invariants: particle mass conservation, center-of-mass trajectory, angle of repose bounds, temperature diffusion Fourier bounds, and chemical stoichiometry counts.

---

## 2. Testing Methodology & Formal Techniques

The testing suite employs four complementary formal verification techniques:

```
+-------------------------------------------------------------------------------+
| Category-Partition (Tier 1)                                                   |
| Divides inputs/states into equivalence classes; samples each partition.       |
+---------------------------------------┬---------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
| Boundary Value Analysis (Tier 2)                                              |
| Tests extremes: grid edges, chunk borders, max temps, zero lifetime, buffers. |
+---------------------------------------┬---------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
| Pairwise Combinatorial (Tier 3)                                               |
| Evaluates interactions between pairs of systems (liquid+powder, heat+concur). |
+---------------------------------------┬---------------------------------------+
                                        │
                                        ▼
+-------------------------------------------------------------------------------+
| Real-World Workloads (Tier 4)                                                 |
| High-stress macroscopic simulations (dam break, volcano, chemical cascade).   |
+-------------------------------------------------------------------------------+
```

### 2.1 Category-Partition Method
Each feature domain is partitioned into mutually exclusive equivalence classes:
- **Material States**: Solid (immobile), Powder (gravity fall + angle-of-repose slide), Liquid (gravity fall + lateral dispersion), Gas (upward buoyancy + Brownian dispersion).
- **Thermal Regimes**: Cryogenic ($T < 0^\circ\text{C}$), Ambient ($0^\circ\text{C} \le T \le 100^\circ\text{C}$), Pyrogenic ($100^\circ\text{C} < T < 1000^\circ\text{C}$), Plasma/Extreme ($T \ge 1000^\circ\text{C}$).
- **Concurrency Phases**: Phase 0 $(0,0)$, Phase 1 $(1,0)$, Phase 2 $(0,1)$, Phase 3 $(1,1)$ in the 4-phase checkerboard layout.
- **Sleep States**: Active (awake), Border-triggered (cascade wake), Static (sleeping).

### 2.2 Boundary Value Analysis (BVA)
Tests probe exact edge values where off-by-one bugs and overflow conditions cluster:
- **Spatial Coordinates**: $x = 0$, $x = \text{Width} - 1$, $y = 0$, $y = \text{Height} - 1$.
- **Chunk Boundaries**: $x = c_x \cdot 64 - 1$, $x = c_x \cdot 64$, $y = c_y \cdot 64 - 1$, $y = c_y \cdot 64$.
- **Temperature Extremes**: Absolute zero ($-273.15^\circ\text{C}$), Phase boundary $-\epsilon$, Phase boundary $+\epsilon$, Maximum engine temperature ($10,000^\circ\text{C}$).
- **Cell Counts & Densities**: 0 active cells (empty chunk), 1 cell, 4095 cells, 4096 cells (completely saturated chunk).
- **Lifetime Values**: 0 (immediate expiration), 1, 254, 255 (maximum `u8` lifetime).

### 2.3 Pairwise Combinatorial Testing
Rather than an exhaustive $O(S^k)$ state space, Pairwise Combinatorial Testing verifies all 2-way interactions between orthogonal feature dimensions:
- $\text{Material}_A \times \text{Material}_B$ mutual displacement (e.g., Sand vs Water, Water vs Oil, Lava vs Water).
- $\text{Thermal State} \times \text{Chemical Reaction}$ (e.g., Gunpowder + Heat, Acid + Iron at cryogenic vs ambient temp).
- $\text{Concurrency Phase} \times \text{Chunk Boundary Transfer}$ (e.g., particle falling across horizontal and vertical chunk boundaries while neighboring chunks execute in different threads).

### 2.4 Real-World Workload Testing
Evaluates macro-scale emergent behaviors involving hundreds of thousands of interacting particles over hundreds of continuous simulation frames. Invariants verify that energy, mass, and conservation laws hold at engine scale.

---

## 3. Complete Feature Inventory & Tier Mapping (F1 to F26)

All 26 features identified in `PROJECT.md § Feature Inventory` are cataloged below with their assigned verification tiers and target test counts:

| Feature ID | Feature Name | Primary Milestone | Assigned Tiers | Target Test Count | Description & Verification Focus |
|:---|:---|:---:|:---:|:---:|:---|
| **F1** | 64x64 SoA Chunk Layout | M1 | Tier 1, Tier 2 | $\ge 10$ | 32KB SoA arrays, cache alignment (`alignas(64)`), indexing `Index(lx, ly)`, coordinate translation, bounds safety. |
| **F2** | MaterialRegistry & Schema | M1 | Tier 1, Tier 2 | $\ge 10$ | JSON loading from `assets/materials/*.json`, ID resolution, default fallback, property query consistency. |
| **F3** | Core Materials Assets | M1 | Tier 1, Tier 2 | $\ge 10$ | Schema integrity for all 18+ JSON files (air, sand, water, stone, dirt, wood, acid, lava, ice, steam, oil, etc.). |
| **F4** | Powder Cellular Automata | M1 | Tier 1, 2, 3, 4 | $\ge 12$ | Vertical gravity fall, diagonal sliding, angle-of-repose pile stability, pyramid formation. |
| **F5** | Liquid Cellular Automata | M1 | Tier 1, 2, 3, 4 | $\ge 12$ | Vertical fall, lateral dispersion ($v_x = \pm \text{dispersion}$), cliff drop-off priority, horizontal leveling. |
| **F6** | Density Mutual Displacement | M1 | Tier 1, 2, 3, 4 | $\ge 12$ | Heavy powder sinks in light liquid (sand in water); light liquid floats on heavy liquid (oil on water); symmetric swap. |
| **F7** | Streaming Texture Rendering | M1 | Tier 1 | $\ge 5$ | Headless buffer generation: correct 32-bit RGBA pixel extraction from chunk SoA material IDs and color variations. |
| **F8** | Interactive Mouse Brush | M1 | Tier 1, Tier 2 | $\ge 10$ | Continuous stroke interpolation (Bresenham line), circular radius fill, eraser brush (air replacement), clamp to world. |
| **F9** | Cellular Heat Diffusion | M2 | Tier 1, 2, 3, 4 | $\ge 12$ | Discrete 2D heat conduction, harmonic mean conductivity, specific heat capacity, temperature conservation. |
| **F10** | Material Phase Transitions | M2 | Tier 1, 2, 3, 4 | $\ge 12$ | Solid melting (ice $\to$ water, stone $\to$ lava), boiling (water $\to$ steam), freezing, condensation, latent heat. |
| **F11** | ReactionSystem & Table | M2 | Tier 1, 2, 3, 4 | $\ge 12$ | Declarative `reactions.json` parsed into $O(1)$ dense 2D lookup table, reaction products, probabilities, heat release. |
| **F12** | Gas Cellular Automata | M2 | Tier 1, 2, 3, 4 | $\ge 10$ | Upward buoyancy fall vector, Brownian lateral diffusion, lifetime decay decrement, dissipation into air. |
| **F13** | Fire & Combustion | M2 | Tier 1, 2, 3, 4 | $\ge 12$ | Ignition temperature triggers, flammability propagation, fuel consumption, smoke and ash emission, water quench. |
| **F14** | ExplosionSystem | M2 | Tier 1, 2, 3, 4 | $\ge 12$ | Raycast shockwave propagation, material strength resistance, destruction radius, thermal blast, chain detonations. |
| **F15** | 4-Phase Checkerboard Concurrency | M3 | Tier 1, 2, 3, 4 | $\ge 12$ | Parallel chunk simulation across worker threads without data races, deterministic matching against single-thread run. |
| **F16** | Sleeping Chunks & Cascade Wake | M3 | Tier 1, 2, 3, 4 | $\ge 12$ | Inactive chunks sleep (`activeCellCount == 0`), cascade wake-up triggered by neighbor boundary activity, heat, shockwave. |
| **F17** | 300k+ Cells @ 60 TPS Perf | M3 | Tier 1, Tier 4 | $\ge 6$ | Zero-allocation stepping, active row bounding boxes, tick execution time $\le 16.666$ ms under 300,000+ active cells. |
| **F18** | Dear ImGui Category Palette | M4 | Tier 1 | $\ge 5$ | Headless category catalog validation: Solid, Powder, Liquid, Gas, Energy, Tools grouping and registry alignment. |
| **F19** | UI Simulation Controls | M4 | Tier 1, Tier 2 | $\ge 10$ | Simulation pause, step-by-step advance, simulation speed scaling, 2D gravity vector redirection (inverted, horizontal). |
| **F20** | F3 Debug Telemetry Overlay | M4 | Tier 1 | $\ge 5$ | Headless metric computation verification: active chunk count, active cell count, TPS, FPS, and tick timing metrics. |
| **F21** | Binary RLE Save/Load | M4 | Tier 1, Tier 2 | $\ge 10$ | Lossless run-length encoding binary format, magic bytes `PIXS`, versioning, roundtrip equality of all cell arrays. |
| **F22** | Headless CLI Benchmark | M4 | Tier 1, Tier 4 | $\ge 5$ | CLI argument `--benchmark` execution without GUI, verification of output metrics and exit code 0. |
| **F23** | CTest Modular Test Suite | M4 | Tier 1, Tier 5 | $\ge 5$ | Integration with `ctest`, test target discovery, execution of all test suites with exit status 0. |
| **F24** | Documentation (README/ARCH) | M4 | Tier 1, Tier 5 | $\ge 5$ | Validation of required documentation files, build instructions, and architecture schema accuracy. |
| **F25** | E2E Test Suite Validation | M5 | Meta / All | $\ge 5$ | Meta-validation: 100% test pass rate across Tiers 1-4, zero flaky tests, clean runner termination. |
| **F26** | Adversarial Hardening (Tier 5) | M5 | Tier 5 | $\ge 15$ | White-box adversarial testing, corrupt save files, invalid JSON, NaN temperatures, memory leak verification. |

---

## 4. Test Architecture & Runner Harness Specification

### 4.1 Directory & File Layout
The E2E test harness resides entirely within the `tests/` directory:

```
c:\Users\Drilla\Desktop\Sandbox/
├── tests/
│   ├── CMakeLists.txt              # CTest & sandbox_e2e_tests target definition
│   └── e2e/
│       ├── e2e_framework.hpp       # Test macros, assertions, WorldFixture, helpers
│       ├── test_main.cpp           # CLI parser, test runner, ANSI console reporter
│       ├── tier1_features.cpp      # Tier 1: Functional Equivalence tests (>= 120 tests)
│       ├── tier2_boundaries.cpp    # Tier 2: Boundary & Edge case tests (>= 90 tests)
│       ├── tier3_pairwise.cpp      # Tier 3: Combinatorial interaction tests (>= 25 tests)
│       └── tier4_workloads.cpp     # Tier 4: Real-world large scale scenarios (>= 5 workloads)
```

### 4.2 Test Framework Interfaces (`e2e_framework.hpp`)
The framework is completely self-contained and header-only, requiring no external testing dependencies (no GoogleTest, Catch2, or Boost).

#### Test Registration Macro
```cpp
#define E2E_TEST(suite, name, tier) ...
```
- `suite`: Logical grouping (e.g., `PowderPhysics`, `HeatDiffusion`, `RLESerialization`).
- `name`: Descriptive test name (e.g., `SandPilesAtAngleOfRepose`).
- `tier`: Numerical tier identifier (`1`, `2`, `3`, `4`, `5`).

#### Assertion Macros
```cpp
ASSERT_TRUE(condition)
ASSERT_FALSE(condition)
ASSERT_EQ(actual, expected)
ASSERT_NE(actual, expected)
ASSERT_FLOAT_NEAR(actual, expected, epsilon)
ASSERT_GT(actual, expected)
ASSERT_LT(actual, expected)
ASSERT_GE(actual, expected)
ASSERT_LE(actual, expected)
```
Upon assertion failure:
1. Records source file name and line number (`__FILE__`, `__LINE__`).
2. Records stringified expression and formatted evaluated values.
3. Marks the current test as FAILED.
4. Returns early from the test function to prevent cascading segfaults, while permitting subsequent tests in the runner to continue.

#### Headless Test Fixture & Helpers
```cpp
namespace sandbox::test {

class WorldFixture {
public:
    WorldFixture(int widthChunks = 2, int heightChunks = 2);
    ~WorldFixture();

    sim::World& GetWorld();
    void Step(int count = 1);

    // Coordinate & inspection utilities
    int CountMaterial(sim::MaterialId matId) const;
    int CountMaterialInRect(sim::MaterialId matId, int x0, int y0, int x1, int y1) const;
    float GetAverageTempInRect(int x0, int y0, int x1, int y1) const;
    bool IsRectPure(sim::MaterialId matId, int x0, int y0, int x1, int y1) const;

    // Fast material ID helpers
    sim::MaterialId ID(const std::string& name) const;
};

} // namespace sandbox::test
```

### 4.3 CLI Invocation & Semantics (`test_main.cpp`)

#### Command-Line Flags
| Flag | Description | Example |
|:---|:---|:---|
| `--all` | Execute all test tiers (Tiers 1 to 4) | `sandbox_e2e_tests --all` |
| `--tier=N` | Execute only tests belonging to Tier N ($N \in \{1, 2, 3, 4, 5\}$) | `sandbox_e2e_tests --tier=1` |
| `--test=<substring>` | Execute tests whose suite or name contains `<substring>` | `sandbox_e2e_tests --test=Powder` |
| `--list` | Print a list of all registered tests and exit | `sandbox_e2e_tests --list` |
| `--verbose` | Print individual assertion passes, step timings, and debug stats | `sandbox_e2e_tests --verbose` |
| `--help` | Display usage instructions and available options | `sandbox_e2e_tests --help` |

#### Console Output Format & ANSI Colors
- Automatically enables Windows Virtual Terminal Processing (`ENABLE_VIRTUAL_TERMINAL_PROCESSING`) via `SetConsoleMode` on Windows stdout.
- Formatted output standard:
  ```
  [==========] Running 255 tests from 26 suites.
  [ RUN      ] PowderPhysics.SandFallsDownInVacuum (Tier 1)
  [       OK ] PowderPhysics.SandFallsDownInVacuum (1.2 ms)
  [ RUN      ] LiquidPhysics.WaterLevelsHorizontally (Tier 1)
  [  FAILED  ] LiquidPhysics.WaterLevelsHorizontally (4.5 ms)
               tests/e2e/tier1_features.cpp:142: ASSERT_EQ(actual, expected)
                 Expected: 64
                 Actual:   63
  [==========] 255 tests executed. 254 passed, 1 failed. (Total time: 482.1 ms)
  ```

#### Exit Codes
- `0`: All executed tests passed.
- `1`: One or more tests failed (assertion violation or unhandled exception).
- `2`: Command-line syntax or invocation error (unknown flag, invalid tier argument).

---

## 5. Real-World Application Scenarios (Tier 4 Specifications)

Tier 4 verifies large-scale emergent behaviors. Each scenario must run deterministically to completion and satisfy quantitative invariant formulas:

```
+-----------------------------------------------------------------------------------------------+
|                                 TIER 4 WORKLOAD SCENARIOS                                     |
|                                                                                               |
|   1. Dam Break (Hydrodynamics)        2. Volcano (Thermodynamics & Phase Transition)          |
|      - Water mass conservation           - Heat diffusion & latent heat                       |
|      - Wave propagation & leveling       - Magma rise, ice melting, steam buoyancy            |
|                                                                                               |
|   3. Chemical Cascade (Reactions)     4. Bunker Bombing (Explosions & Structural Impact)      |
|      - Density layering (oil/water)      - Shockwave raycasting & barrier dissipation         |
|      - Acid etching, combustion, gas     - Crater formation & sympathetic secondary blast     |
|                                                                                               |
|   5. 300,000+ Sand Pour (Massive Concurrency, Performance & Sleep Cascade)                    |
|      - 300k+ particles, 60 TPS, multi-threaded 4-phase checkerboard, sleep transition         |
+-----------------------------------------------------------------------------------------------+
```

### Scenario 1: Dam Break (Hydrodynamics & Obstacle Flow)
- **Objective**: Validate liquid cellular automata, gravity acceleration, lateral dispersion, obstacle collision, and mass conservation.
- **World Dimensions**: $256 \times 128$ cells ($4 \times 2$ chunks).
- **Initial Setup**:
  - A solid stone retaining wall ($x = 80$, $y \in [32, 127]$, thickness 4 cells).
  - Left reservoir ($x \in [10, 79]$, $y \in [40, 127]$) filled with Water (approx. 6,160 water cells).
  - Right basin contains stepped stone obstacles at $(x=120, y=100)$ and $(x=160, y=110)$.
- **Trigger**: At $t = 10$ steps, a breach of 12 vertical cells is removed from the dam base ($x \in [80, 83]$, $y \in [110, 122]$).
- **Simulation Duration**: 400 steps ($\approx 6.67$ seconds simulated).
- **Quantitative Assertions**:
  1. *Mass Conservation*: $\text{WaterCount}(t=400) \equiv \text{WaterCount}(t=0)$. Zero water particles lost or created.
  2. *Kinematic Flow*: Water front reaches $x \ge 200$ before $t = 180$ steps.
  3. *Equilibrium State*: At $t = 400$, water in the right basin forms a contiguous horizontal surface where $\max(y) - \min(y) \le 1$ cell across any span $> 20$ cells.
  4. *Performance*: Mean tick duration across 400 steps $\le 2.0$ ms.

### Scenario 2: Volcano (Magma Dynamics, Thermal Diffusion & Phase Transition)
- **Objective**: Validate thermal conduction, high-temperature phase transitions, gas buoyancy, and liquid solidification.
- **World Dimensions**: $256 \times 256$ cells ($4 \times 4$ chunks).
- **Initial Setup**:
  - Stone volcano caldera with slopes rising from $(x=32, y=200)$ to rim $(x=100, y=80)$ and $(x=156, y=80)$ to $(x=224, y=200)$.
  - Hollow magma chamber ($x \in [110, 146]$, $y \in [120, 240]$) filled with Lava ($T = 1200^\circ\text{C}$).
  - Caldera rim coated with Ice ($x \in [90, 110]$, $y \in [70, 80]$, $T = -15^\circ\text{C}$).
  - Ambient atmosphere: Air ($T = 20^\circ\text{C}$).
- **Trigger**: Magma rises through central conduit under thermal expansion / upward pressure.
- **Simulation Duration**: 600 steps.
- **Quantitative Assertions**:
  1. *Thermal Conduction*: Stone conduit walls at distance $\le 3$ cells from lava attain $T \ge 300^\circ\text{C}$ by $t = 300$.
  2. *Phase Transitions*:
     - 100% of ice cells within 10 cells of rising lava reach $T \ge 0^\circ\text{C}$ and melt into Water.
     - Melted water contacting lava reaches $T \ge 100^\circ\text{C}$ and transforms into Steam.
  3. *Buoyant Ascent*: Steam ascends vertically ($v_y < 0$) and reaches upper world boundary ($y \le 10$).
  4. *Cooling & Solidification*: Lava splashing over the outer rim exposed to ambient air cools below its freezing point and transforms back into Stone.
  5. *Thermodynamic Bound*: No cell exhibits $T < -273.15^\circ\text{C}$ or $T > 10,000^\circ\text{C}$.

### Scenario 3: Chemical Cascade (Acid, Oil, Water, Fire & Gas Multi-Reaction)
- **Objective**: Validate multi-material density stratification, acid digestion, flammable combustion, and vapor generation.
- **World Dimensions**: $192 \times 192$ cells ($3 \times 3$ chunks).
- **Initial Setup**:
  - Chamber 1: Bottom layer Water ($y \in [140, 180]$, density $1000 \text{ kg/m}^3$), top layer Oil ($y \in [100, 139]$, density $800 \text{ kg/m}^3$).
  - Chamber 2: Reinforced Iron and Wood barriers separating Chamber 1 from an upper Acid reservoir.
  - Acid reservoir: 1,500 cells of Acid ($y \in [30, 60]$).
- **Trigger**:
  - $t = 0$: Acid valve opened onto Iron barrier.
  - $t = 150$: Fire spark introduced to Oil layer.
- **Simulation Duration**: 500 steps.
- **Quantitative Assertions**:
  1. *Density Stratification*: Oil floats above water with zero inverted particles ($\forall (x, y_{\text{oil}}, y_{\text{water}}), x_1 = x_2 \implies y_{\text{oil}} \le y_{\text{water}}$ at equilibrium).
  2. *Acid Digestion*: Acid dissolves Iron barrier in accordance with `reactions.json`; barrier breached by $t = 120$.
  3. *Combustion Propagation*: Fire consumes Oil cells at rate $\ge 5 \text{ cells/step}$, emitting Smoke and Fire particles.
  4. *Quenching*: Fire particles contacting the lower Water layer are extinguished instantly; Water is converted to Steam proportionally to heat delta.

### Scenario 4: Bunker Bombing (Underground Structure, Shockwave Propagation & Destruction)
- **Objective**: Validate raycast shockwave attenuation, material strength thresholds, structural cratering, and secondary detonations.
- **World Dimensions**: $256 \times 256$ cells ($4 \times 4$ chunks).
- **Initial Setup**:
  - Surface terrain: Dirt ($y \in [80, 140]$).
  - Underground Bunker: Reinforced Stone box ($x \in [96, 160]$, $y \in [150, 190]$, wall thickness 6 cells).
  - Bunker Interior: Air chamber containing a stockpile of 64 TNT blocks ($x \in [120, 136]$, $y \in [170, 178]$).
- **Trigger**: Surface bomb detonation (power 120, radius 40 cells) at $(x = 128, y = 78)$.
- **Simulation Duration**: 250 steps.
- **Quantitative Assertions**:
  1. *Crater Geometry*: Surface Dirt within radius 25 cells is vaporized or converted to Ash/Smoke.
  2. *Shockwave Attenuation*: Bunker Stone ceiling absorbs blast energy according to material strength ($\text{RemainingPower} = \text{Power} - \sum \text{Strength}_i$).
  3. *Sympathetic Detonation*:
     - If ceiling thickness is intact (6 cells of stone), internal TNT does NOT detonate.
     - With a weakened ceiling (2 cells of stone), blast penetrates, triggering internal TNT blocks in a violent secondary explosion at $t \approx 8$.
  4. *Thermal Pulse*: Flammable Wood markers placed on the surface within line-of-sight ignite; markers behind stone berms remain unburned (shadow verification).

### Scenario 5: 300,000+ Sand Pour (Massive Concurrency, Performance & Sleep Cascade)
- **Objective**: Validate engine scalability, zero-allocation stepping, 4-phase checkerboard multithreading, data-race freedom, and sleeping chunk cascade.
- **World Dimensions**: $1280 \times 768$ cells ($20 \times 12 = 240$ chunks = 983,040 cells total).
- **Initial Setup**:
  - Large funnel structure spanning 10 horizontal chunks.
  - 320,000 particles of Sand spawned across upper chunks ($y \in [20, 300]$).
  - Bottom floor of solid Stone at $y = 750$.
- **Trigger**: Full simulation run with all available hardware threads enabled in `ThreadPool`.
- **Simulation Duration**: 500 steps.
- **Quantitative Assertions**:
  1. *Mass Conservation & Determinism*: Total sand count at step 500 equals exactly 320,000 cells. No particles lost, duplicated, or corrupted across parallel chunk boundaries.
  2. *Performance Target*: Average simulation tick duration during peak movement ($t \in [50, 200]$) is $\le 16.666$ ms (sustaining $\ge 60$ TPS under 300,000+ moving cells).
  3. *Angle-of-Repose Resting State*: Once sand falls to the floor, it accumulates into stable conical piles with slope angle $\theta \in [30^\circ, 36^\circ]$.
  4. *Sleep Transition*: By $t = 500$, as motion ceases, settled chunks transition to `isSleeping == true`. Active chunk count drops by $\ge 80\%$, reducing tick time to $< 1.5$ ms.

---

## 6. Coverage Thresholds & Mathematical Verification Formulas

To ensure rigorous requirement coverage without arbitrary metrics, the test suite enforces a strict mathematical threshold formula for each tier:

```
+-----------------------------------------------------------------------------------------------+
|                               COVERAGE THRESHOLD FORMULAS                                     |
|                                                                                               |
|   Tier 1 (Feature Coverage):        Tests(Tier 1) >= 5 * N_tier1                              |
|   Tier 2 (Boundary Values):         Tests(Tier 2) >= 5 * N_tier2                              |
|   Tier 3 (Pairwise Interactions):   Tests(Tier 3) >= N_pairs                                  |
|   Tier 4 (Real-World Workloads):    Tests(Tier 4) >= max(5, floor(N_subsystems / 2))          |
+-----------------------------------------------------------------------------------------------+
```

### 6.1 Formula Breakdown & Minimum Target Test Counts

1. **Tier 1 — Functional Equivalence**:
   - $N_{\text{tier1}} = 24$ functional features (F1 through F24).
   - $\text{Threshold} \ge 5 \times 24 = \mathbf{120\text{ tests}}$.
   - Every single feature must have at least 5 distinct functional test cases verifying nominal operation.

2. **Tier 2 — Boundary Value Analysis**:
   - $N_{\text{tier2}} = 18$ features possessing spatial, numerical, or state boundary domains (F1-F6, F8-F16, F19, F21, F22).
   - $\text{Threshold} \ge 5 \times 18 = \mathbf{90\text{ tests}}$.
   - Tests evaluate minimums, maximums, overflows, empty inputs, saturation, and boundary crossings.

3. **Tier 3 — Pairwise Combinatorial Interactions**:
   - Pairwise matrix covers 2-way interactions between:
     - 4 Material states (Solid, Powder, Liquid, Gas)
     - 4 Chemical/Thermal reactions (Acid, Fire, Water, Lava)
     - 2 Concurrency models (Single-thread vs Multi-thread 4-phase)
     - 2 Chunk boundary alignments (Internal chunk cell vs Border cell)
   - $\text{Threshold} \ge \mathbf{25\text{ tests}}$.

4. **Tier 4 — Real-World Workloads**:
   - Covers all 6 primary simulation subsystems (Physics, Thermodynamics, Chemistry, Explosions, Multithreading, Scale).
   - $\text{Threshold} \ge \max(5, \lfloor 6 / 2 \rfloor) = \mathbf{5\text{ detailed scenarios}}$.

5. **Tier 5 — Adversarial Hardening (Milestone M5)**:
   - Malformed files, corrupted save streams, memory stress, fuzz testing.
   - $\text{Threshold} \ge \mathbf{15\text{ tests}}$.

### 6.2 Overall Suite Target
$$\mathbf{\text{Total Test Target}} = 120 + 90 + 25 + 5 + 15 = \mathbf{255\text{ test cases}}$$
Every test must pass with $100\%$ success rate ($0$ failures) to clear milestone gates.

---

## 7. Integration with CTest, CMake & CI Pipelines

### 7.1 CMake Configuration (`tests/CMakeLists.txt`)
The test target `sandbox_e2e_tests` is built as a standalone native executable and integrated directly into CTest:

```cmake
# tests/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

set(E2E_SOURCES
    e2e/test_main.cpp
    e2e/tier1_features.cpp
    e2e/tier2_boundaries.cpp
    e2e/tier3_pairwise.cpp
    e2e/tier4_workloads.cpp
    ../src/core/Config.cpp
    ../src/core/ThreadPool.cpp
    ../src/sim/Chunk.cpp
    ../src/sim/MaterialRegistry.cpp
    # Add other core sim sources as implemented in M1-M4
)

add_executable(sandbox_e2e_tests ${E2E_SOURCES})

target_include_directories(sandbox_e2e_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../external
)

# Link SDL2 statically if required by shared core types, or keep purely decoupled
target_link_libraries(sandbox_e2e_tests PRIVATE ${SDL2_TARGET})
if(WIN32)
    target_link_libraries(sandbox_e2e_tests PRIVATE winmm imm32 version setupapi user32 gdi32 shell32)
endif()

# Copy assets next to test executable so tests locate materials & reactions
add_custom_command(TARGET sandbox_e2e_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../assets
        $<TARGET_FILE_DIR:sandbox_e2e_tests>/assets
)

# Register with CTest
enable_testing()
add_test(NAME E2E_Tier1_Features     COMMAND sandbox_e2e_tests --tier=1)
add_test(NAME E2E_Tier2_Boundaries   COMMAND sandbox_e2e_tests --tier=2)
add_test(NAME E2E_Tier3_Pairwise     COMMAND sandbox_e2e_tests --tier=3)
add_test(NAME E2E_Tier4_Workloads    COMMAND sandbox_e2e_tests --tier=4)
add_test(NAME E2E_All_Tests          COMMAND sandbox_e2e_tests --all)
```

### 7.2 Execution via CLI & CTest
- Run all tests via CTest:
  ```powershell
  ctest --test-dir build -C Release --output-on-failure
  ```
- Run directly from command prompt:
  ```powershell
  .\build\bin\Release\sandbox_e2e_tests.exe --all
  .\build\bin\Release\sandbox_e2e_tests.exe --tier=4 --verbose
  ```

---

## 8. Quality Gates & Acceptance Verification

To pass Milestone M_TEST_1 and each subsequent milestone:

| Gate | Requirement | Verification Method |
|:---|:---|:---|
| **Gate 1: Compilation** | Zero compile warnings or errors under MSVC `/W4` and Clang/GCC `-Wall -Wextra`. | `cmake --build build --config Release --target sandbox_e2e_tests` |
| **Gate 2: CLI Conformance** | `--all`, `--tier=N`, `--test=<str>`, `--list`, `--help` work as specified with exit codes 0, 1, 2. | Automated CLI invocation tests in PowerShell. |
| **Gate 3: Pass Rate** | $100\%$ pass rate across all registered tests. Zero assertion failures. | CTest output returns `100% tests passed, 0 tests failed out of N`. |
| **Gate 4: Determinism** | 10 repeated runs of Tier 3 and Tier 4 produce identical results. | Multiple consecutive runs comparison. |
| **Gate 5: Memory & Leaks** | Zero unhandled exceptions, zero buffer overruns, clean heap termination. | Valgrind / ASan / Windows heap checks. |
