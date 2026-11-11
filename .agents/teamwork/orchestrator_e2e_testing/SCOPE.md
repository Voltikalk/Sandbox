# Scope: E2E Testing Track

## Architecture
- Opaque-box requirement-driven testing architecture derived from `ORIGINAL_REQUEST.md` and `PROJECT.md`.
- No dependencies on UI / SDL rendering internals; tests run headlessly.
- Independent test runner executable `sandbox_e2e_tests` integrated into CMake and registered with CTest.
- Test directory layout:
  - `tests/e2e/e2e_framework.hpp` (Opaque-box testing harness, assertions, headless runner)
  - `tests/e2e/test_main.cpp` (Main CLI entry point for E2E tests with tier filtering)
  - `tests/e2e/tier1_features.cpp` (Tier 1: >=5 tests per feature for all core simulation features)
  - `tests/e2e/tier2_boundaries.cpp` (Tier 2: >=5 boundary and corner cases per feature)
  - `tests/e2e/tier3_pairwise.cpp` (Tier 3: Pairwise cross-feature interaction tests)
  - `tests/e2e/tier4_workloads.cpp` (Tier 4: Realistic multi-feature application scenarios)

## Feature Inventory Coverage
All features from `PROJECT.md § Feature Inventory` are assigned to test tiers:
| # | Feature | Milestone Assignment | Target Test Tier |
|---|---------|---------------------|------------------|
| 1 | 64x64 SoA Chunk Layout | M_TEST_2, M_TEST_3 | Tier 1, Tier 2 |
| 2 | MaterialRegistry & Schema | M_TEST_2, M_TEST_3 | Tier 1, Tier 2 |
| 3 | Core Materials Assets | M_TEST_2, M_TEST_3 | Tier 1, Tier 2 |
| 4 | Powder Cellular Automata | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 5 | Liquid Cellular Automata | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 6 | Density Mutual Displacement | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 7 | Streaming Texture Rendering | M_TEST_1, M_TEST_2 | Tier 1 (Headless buffer / color mapping) |
| 8 | Interactive Mouse Brush | M_TEST_2, M_TEST_3 | Tier 1, Tier 2 |
| 9 | Cellular Heat Diffusion | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 10 | Material Phase Transitions | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 11 | ReactionSystem & Table | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 12 | Gas Cellular Automata | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 13 | Fire & Combustion | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 14 | ExplosionSystem | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 15 | 4-Phase Checkerboard Concurrency | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 16 | Sleeping Chunks & Wakeup Cascade | M_TEST_2, M_TEST_3, M_TEST_4 | Tier 1, Tier 2, Tier 3, Tier 4 |
| 17 | 300,000+ Cells @ 60 TPS Performance | M_TEST_2, M_TEST_4 | Tier 1, Tier 4 |
| 18 | Dear ImGui Category Palette | M_TEST_2 | Tier 1 (Registry category grouping verification) |
| 19 | UI Simulation Controls | M_TEST_2, M_TEST_3 | Tier 1, Tier 2 (Gravity vector & step logic) |
| 20 | F3 Debug Telemetry Overlay | M_TEST_2 | Tier 1 (Metrics calculation correctness) |
| 21 | Binary RLE Save/Load | M_TEST_2, M_TEST_3 | Tier 1, Tier 2 |
| 22 | Headless CLI Benchmark | M_TEST_2, M_TEST_4 | Tier 1, Tier 4 |
| 23 | CTest Modular Test Suite | M_TEST_1, M_TEST_5 | Tier 1, Tier 5 |
| 24 | Documentation | M_TEST_5 | Verification |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M_TEST_1 | Test Infra & Runner Harness | Design opaque-box test runner, assertion macros, fixture for headless World/Simulation execution, CTest registration, and publish `TEST_INFRA.md` at project root | none | DONE |
| M_TEST_2 | Tier 1 Feature Coverage Tests | >=5 test cases per feature covering F1-F6, F8-F16, F21-F22 (>=60 tests) | M_TEST_1 | IN_PROGRESS |
| M_TEST_3 | Tier 2 Boundary & Corner Cases | >=5 test cases per feature covering boundaries, limits, empty chunks, overflows, extreme temps, zero lifetimes (>=60 tests) | M_TEST_1 | PLANNED |
| M_TEST_4 | Tier 3 & Tier 4 Pairwise & Workload Scenarios | Pairwise cross-feature tests (powder+liquid, density swapping, heat+powder, reactions, explosions) + >=5 realistic application workloads (dam break, volcano, chemical cascade, bunker bombing, 300k sand pour) | M_TEST_2, M_TEST_3 | PLANNED |
| M_TEST_5 | Full Suite Build, CTest Verification & TEST_READY.md | Verify clean build of all E2E test targets, run via CTest, confirm 100% pass, generate TEST_READY.md with coverage checklist, notify parent orchestrator | M_TEST_4 | PLANNED |

## Interface Contracts
- Tests communicate exclusively through public interfaces defined in `PROJECT.md § Interface Contracts`:
  - `World(width, height)`, `GetChunk()`, `SetCell()`, `GetCellMaterial()`, `GetCellTemperature()`, `SwapCells()`
  - `MaterialRegistry::Instance().LoadFromDirectory(...)`, `Get()`, `FindByName()`
  - `ReactionSystem::Instance().LoadFromFile(...)`, `GetReaction()`
  - `SaveWorldToFile()`, `LoadWorldFromFile()`
  - Headless simulation stepping / tick loop
