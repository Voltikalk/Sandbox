# BRIEFING — 2026-11-11T05:57:30Z

## Mission
Investigate CMake build system and test configuration to define how `sandbox_e2e_tests` and CTest should be set up, linked, and asset-resolved.

## 🔒 My Identity
- Archetype: explorer
- Roles: Build Infra Explorer
- Working directory: c:\Users\Drilla\Desktop\Sandbox
- State folder: c:\Users\Drilla\Desktop\Sandbox\.agents\teamwork\explorer_mtest1_1
- Original parent: 01dab337-aa6a-4014-b2f8-e686243da338
- Milestone: M_TEST_1

## 🔒 Key Constraints
- Read-only investigation — do NOT implement
- Follow Handoff Protocol (Observation, Logic Chain, Caveats, Conclusion, Verification Method)
- .agents/teamwork/ holds only metadata

## Current Parent
- Conversation ID: 01dab337-aa6a-4014-b2f8-e686243da338
- Updated: not yet

## Investigation State
- **Explored paths**:
  - `CMakeLists.txt` (root)
  - `CMakePresets.json`
  - `tests/test_physics.cpp`
  - `src/sim/MaterialRegistry.hpp` & `.cpp`
  - `src/sim/Chunk.hpp` & `.cpp`
  - `src/ui/Brush.hpp`
  - `src/core/Types.hpp`, `Log.hpp`, `Config.hpp`, `ThreadPool.hpp`
  - `build/Release` output tree & CMake MSVC build characteristics
- **Key findings**:
  - Root `CMakeLists.txt` lacks `enable_testing()` and `add_subdirectory(tests)`.
  - `tests/CMakeLists.txt` does not exist yet. Worker M1 placed `test_physics` directly into root `CMakeLists.txt` without CTest registration.
  - Dependencies: Header-only `nlohmann/json.hpp` via `external` include dir; static `SDL2-static` and static `imgui`. Core simulation (`Chunk`, `MaterialRegistry`, `World`, `Physics`, `ThreadPool`, `Config`) has no SDL2 or ImGui dependencies, though linking `${SDL2_TARGET}` and `imgui` provides full engine availability.
  - MSVC generator is multi-config (`Visual Studio 17 2022 x64`) with `/MT` static runtime.
  - Asset resolution: In VS multi-config builds, CTest runs from `build` or `build/tests` while executables are in `Release/`. Need 4-tier defense: (1) CTest `WORKING_DIRECTORY` property set to `${CMAKE_SOURCE_DIR}`, (2) CMake POST_BUILD copy to `$<TARGET_FILE_DIR:sandbox_e2e_tests>/assets`, (3) compile-time `SANDBOX_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"`, (4) runtime fallback resolver.
- **Unexplored areas**: Implementation of the E2E framework macros and `TEST_INFRA.md` (handled by peer explorers `explorer_mtest1_2` and `explorer_mtest1_3`).

## Key Decisions Made
- Recommend moving test targets into `tests/CMakeLists.txt` and adding `enable_testing()` + `add_subdirectory(tests)` to root `CMakeLists.txt`.
- Define `sandbox_e2e_tests` with `--all` and tier arguments registered as distinct CTest tests (`e2e_all`, `e2e_tier1`, etc.).
- Prescribe 4-layer asset resolution strategy to ensure seamless execution via CTest and direct CLI execution on Windows.

## Artifact Index
- DISPATCH.md — incoming dispatch instructions
- BRIEFING.md — persistent working memory
- progress.md — liveness heartbeat
- handoff.md — final handoff report
