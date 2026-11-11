# Handoff Report: Test Infrastructure & Specification Design (`TEST_INFRA.md`)

- **Agent**: explorer_mtest1_3 (Test Infra Spec Explorer)
- **Milestone**: M_TEST_1 (Test Infra & Runner Harness)
- **Target Deliverable**: `c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md` (Drafted at `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_3\proposed_TEST_INFRA.md`)

---

## 1. Observation

Direct observations from examining the codebase, specifications, and project metadata:

1. **`ORIGINAL_REQUEST.md` Requirements**:
   - `R1. Мир, Чанки и базовая физика сыпучих и жидких материалов`: Lines 13-18 require 64x64 SoA chunks cache-line aligned, MaterialRegistry with JSON loading, POWDER and LIQUID physics, density mutual displacement, streaming texture with GL_NEAREST, and brush tool.
   - `R2. Термодинамика, реакции, газы и взрывы`: Lines 20-25 require discrete heat diffusion, phase transitions, ReactionSystem with `reactions.json`, GAS and FIRE physics with lifetime and buoyancy, and ExplosionSystem shockwaves.
   - `R3. Многопоточная симуляция и оптимизация`: Lines 26-30 require 4-phase checkerboard parallel updates, sleeping chunk mechanisms with boundary cascades, and 300,000+ active cells at stable 60 TPS.
   - `R4. Пользовательский интерфейс, сохранение, тесты`: Lines 31-36 require Dear ImGui UI, binary RLE save/load, CTest modular test suites, headless `--benchmark` CLI mode, and documentation.

2. **`PROJECT.md` Feature Inventory & Interface Contracts**:
   - Lines 105-133 enumerate 26 features (F1 to F26).
   - Lines 150-199 define public interfaces for `Chunk`, `World`, `CellFlags`, `MaterialId`:
     - `Chunk::SIZE = 64`, `Chunk::AREA = 4096`, `alignas(core::CACHE_LINE_SIZE)` arrays: `material`, `temperature`, `flags`, `lifetime`.
     - `World::SetCell`, `GetCellMaterial`, `GetCellTemperature`, `SwapCells`, `WakeNeighbors`.
   - Lines 201-242 define `MaterialState` (`Solid`, `Powder`, `Liquid`, `Gas`), `Material` struct with density, dispersion, flammability, phase transition points, and `MaterialRegistry::Instance()`.
   - Lines 245-268 define `ReactionResult` and `ReactionSystem::Instance()`.
   - Lines 270-286 define `SaveHeader` (magic `'P','I','X','S'`), `SaveWorldToFile()`, `LoadWorldFromFile()`.

3. **`SCOPE.md` Architecture & Milestones**:
   - Lines 4-14 establish opaque-box requirement-driven testing architecture running headlessly without UI/SDL rendering dependencies.
   - Test runner executable: `sandbox_e2e_tests` integrated into CMake and registered with CTest.
   - Test directory layout:
     - `tests/e2e/e2e_framework.hpp`
     - `tests/e2e/test_main.cpp`
     - `tests/e2e/tier1_features.cpp`
     - `tests/e2e/tier2_boundaries.cpp`
     - `tests/e2e/tier3_pairwise.cpp`
     - `tests/e2e/tier4_workloads.cpp`
   - Lines 47-51 outline milestones M_TEST_1 to M_TEST_5.

4. **Codebase State**:
   - `CMakeLists.txt`: Root CMake configuration builds `sandbox` executable, links `imgui` and `SDL2-static`, and copies `assets/` to output directory via post-build command. Tests are not yet linked in root CMake.
   - `tests/`: Directory currently exists but is empty.
   - `assets/materials/`: Contains 18 material JSON files (`acid.json`, `air.json`, `ash.json`, `dirt.json`, `fire.json`, `glass.json`, `gunpowder.json`, `ice.json`, `iron.json`, `lava.json`, `oil.json`, `sand.json`, `smoke.json`, `steam.json`, `stone.json`, `tnt.json`, `water.json`, `wood.json`).

---

## 2. Logic Chain

1. **Test Philosophy Derivation**:
   - From `SCOPE.md` lines 4-6 and `PROJECT.md` lines 5-36, the simulation core is strictly decoupled from presentation (SDL2/ImGui).
   - Therefore, the E2E test harness must be **100% headless** and **opaque-box**, testing exclusively through public contracts (`World`, `MaterialRegistry`, `ReactionSystem`, `Serialization`, and CLI dispatcher).
   - Tests do not require a display server, GPU driver, or window manager, guaranteeing fast, deterministic execution across all CI and local environments.

2. **Methodology Selection**:
   - From `ORIGINAL_REQUEST.md` (broad multi-physics domain) and the Project Pattern requirements:
     - **Category-Partition**: Divides continuous domains (solid/powder/liquid/gas, cold/ambient/hot/pyrogenic) into equivalence classes for Tier 1.
     - **Boundary Value Analysis**: Probes the edges of chunk arrays ($64 \times 64$), world borders ($x=0, y=0$), saturation counts ($0$ to $4096$), lifetimes ($0$ to $255$), and temperature limits for Tier 2.
     - **Pairwise Combinatorial**: Isolates $O(N^2)$ cross-system couplings (density swaps, thermal reactions, multi-phase contact) for Tier 3.
     - **Real-World Workloads**: Evaluates emergent macroscopic physics (wave flow, caldera thermal plumes, multi-reagent fires, blast craters, 300k sand pour) for Tier 4.

3. **Feature Inventory Mapping (F1 to F26)**:
   - Every feature from `PROJECT.md` lines 106-132 is explicitly assigned to its primary milestone, test tiers, target test count, and verification focus.
   - F1-F24 represent the complete functional domain.

4. **Coverage Threshold Mathematical Formulation**:
   - Minimum formula:
     - $\text{Tier 1} \ge 5 \times N_{\text{tier1}} = 5 \times 24 = \mathbf{120\text{ tests}}$.
     - $\text{Tier 2} \ge 5 \times N_{\text{tier2}} = 5 \times 18 = \mathbf{90\text{ tests}}$ (for all features with boundary domains).
     - $\text{Tier 3} \ge \mathbf{25\text{ tests}}$ (all-pairs matrix of material states, reactions, concurrency, borders).
     - $\text{Tier 4} \ge \max(5, \lfloor 6 / 2 \rfloor) = \mathbf{5\text{ realistic workload scenarios}}$.
     - $\text{Tier 5} \ge \mathbf{15\text{ adversarial/stress tests}}$.
     - **Total Suite Target**: $\ge \mathbf{255\text{ test cases}}$, requiring a $100\%$ pass rate.

5. **Real-World Workload Specification (Tier 4)**:
   - Five complete scenario specifications were formulated with explicit initial configurations, dimensions, triggers, step durations, and mathematical/physical invariant assertions:
     - Scenario 1: Dam Break (Hydrodynamics, mass conservation, horizontal surface leveling).
     - Scenario 2: Volcano (Magma rise, conductive heat, ice $\to$ water $\to$ steam phase changes, gas buoyancy, lava cooling to stone).
     - Scenario 3: Chemical Cascade (Oil/water density separation, acid eating iron, oil combustion, water quenching).
     - Scenario 4: Bunker Bombing (Surface blast crater, raycast shockwave absorption by stone, secondary sympathetic TNT detonation).
     - Scenario 5: 300,000+ Sand Pour (Funnel flow, 4-phase checkerboard multithreading, determinism, angle of repose $30^\circ\text{--}36^\circ$, sleeping chunk transition).

6. **Specification Document Deliverable**:
   - The full, production-ready specification has been authored and saved to:
     `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_3\proposed_TEST_INFRA.md`
   - It is ready to be published to `c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md` by the downstream worker agent.

---

## 3. Caveats

1. **Source Code Write Restriction**:
   - Per the Explorer Archetype rules, Explorer agents do not directly modify source code or root project files outside their state folder.
   - Consequently, `TEST_INFRA.md` was authored as `proposed_TEST_INFRA.md` within `.agents/teamwork/explorer_mtest1_3/`. The worker agent in Milestone M_TEST_1 will copy/publish this file to `c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md`.
2. **Simulation Implementations in Progress**:
   - Milestone M1 (simulation core, World, Chunk, MaterialRegistry) is being developed concurrently. The test specifications and interface contracts in `proposed_TEST_INFRA.md` are aligned with `PROJECT.md § Interface Contracts`. If any minor interface adjustments occur during M1, the test fixture in `e2e_framework.hpp` provides abstraction buffering.
3. **Hardware-Dependent Benchmark Timing**:
   - For Scenario 5 (300k sand pour) and F17/F22, execution timing ($\le 16.666$ ms per step) is evaluated in Release builds on multi-core Windows x64 machines; Debug builds will run slower and should use higher relative threshold multipliers or skip microsecond-strict timing assertions.

---

## 4. Conclusion

1. The test specification `TEST_INFRA.md` has been completely designed and authored in `proposed_TEST_INFRA.md`.
2. It thoroughly specifies:
   - Opaque-box, requirement-driven, 100% headless testing philosophy.
   - Formal methodologies: Category-Partition, Boundary Value Analysis, Pairwise Combinatorial, Real-World Workload Testing.
   - Exhaustive mapping of all 26 features (F1 through F26) to test tiers and target counts.
   - Standalone CLI runner architecture (`sandbox_e2e_tests`), command-line flags (`--all`, `--tier=N`, `--test=<str>`, `--list`, `--verbose`, `--help`), exit codes (`0`, `1`, `2`), and ANSI console reporting.
   - Detailed quantitative specifications for 5 Tier 4 Real-World Application Scenarios (Dam Break, Volcano, Chemical Cascade, Bunker Bombing, 300k Sand Pour).
   - Mathematical coverage threshold formulas establishing a total target of $\ge 255$ tests.
   - Integration blueprints for `tests/CMakeLists.txt` and CTest.
   - Quality gates and criteria for clearing milestones M_TEST_1 through M_TEST_5.
3. The downstream Worker agent can immediately publish `proposed_TEST_INFRA.md` to the project root `TEST_INFRA.md` without any missing requirements or unresolved questions.

---

## 5. Verification Method

To independently verify this specification and its downstream implementation:

1. **Document Inspection**:
   - Verify that `c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_3\proposed_TEST_INFRA.md` exists and contains all 8 required sections:
     ```powershell
     Get-Item "c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_3\proposed_TEST_INFRA.md"
     ```
   - Check line count, headers, and completeness:
     ```powershell
     Select-String -Path "c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_3\proposed_TEST_INFRA.md" -Pattern "^## "
     ```
2. **Feature Coverage Check**:
   - Confirm that features F1 through F26 are present in the feature inventory table:
     ```powershell
     1..26 | ForEach-Object { $pat = "\*\*F$_\*\*"; if (-not (Select-String -Path "c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_3\proposed_TEST_INFRA.md" -Pattern $pat)) { Write-Error "Missing F$_" } }
     ```
3. **Downstream Worker Publishing**:
   - When the worker executes, verify that `TEST_INFRA.md` is copied to the workspace root:
     ```powershell
     Test-Path "c:\Users\Drilla\Desktop\Sandbox\TEST_INFRA.md"
     ```
4. **Invalidation Conditions**:
   - If `PROJECT.md` changes the definition or count of features (F1-F26), the mapping table in Section 3 and the coverage count formulas in Section 6 must be updated accordingly.
