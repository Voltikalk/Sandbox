# Handoff Report: CMake Build Infra & Test Configuration (Milestone M_TEST_1)

**Agent**: `explorer_mtest1_1` (Build Infra Explorer)  
**Date**: 2026-11-11  
**Milestone**: M_TEST_1 (Test Infra & Runner Harness)  
**Status**: COMPLETE  

---

## 1. Observation

Direct observations from the workspace filesystem, CMake configuration, and source inspection:

### 1.1 Root `CMakeLists.txt`
Inspected `c:\Users\Drilla\Desktop\Sandbox\CMakeLists.txt` (135 lines):
- **C++ Standard**: C++20 is strictly enforced (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`, lines 4–6).
- **Runtime Library & Compiler Flags**:
  ```cmake
  if(MSVC)
      set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
      add_compile_options(/W4 /permissive- /utf-8 /MP)
  ...
  ```
  All targets must use the static MSVC runtime (`/MT` in Release, `/MTd` in Debug) to prevent runtime mismatch linker errors (`LNK4098`, `LNK2038`).
- **Dependencies**:
  - `SDL2`: Static build configured via `add_subdirectory(external/sdl2 EXCLUDE_FROM_ALL)` with target `SDL2-static`, or system fallback `find_package(SDL2 REQUIRED CONFIG)` (lines 19–33).
  - `Dear ImGui`: Built as a static library target `imgui` from `external/imgui/*.cpp` and SDL2/Renderer2 backends (lines 36–57), linking `${SDL2_TARGET}`.
  - `nlohmann_json`: Single-header library in `external/nlohmann/json.hpp`. Available when `external` is in include directories.
- **Source Variables**:
  - `CORE_SOURCES`: `src/core/Config.cpp`, `src/core/ThreadPool.cpp` (lines 60–63).
  - `SIM_SOURCES`: `src/sim/Chunk.cpp`, `src/sim/World.cpp`, `src/sim/MaterialRegistry.cpp`, `src/sim/Physics.cpp` (lines 65–70).
  - `UI_SOURCES`: `src/ui/Renderer.cpp`, `src/ui/Brush.cpp` (lines 72–75).
- **Asset Copy**:
  ```cmake
  add_custom_command(TARGET sandbox POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:sandbox>/assets
  )
  ```
- **Test Executable in Root**:
  - Worker M1 temporarily placed `test_physics` in root `CMakeLists.txt` (lines 109–135), duplicating include paths and compile lists.
- **Absence of CTest**:
  - `enable_testing()` is **not** present in root `CMakeLists.txt`.
  - `add_subdirectory(tests)` is **not** present in root `CMakeLists.txt`.
  - No `add_test(...)` commands exist anywhere in the project.

### 1.2 `tests/` Directory and `tests/CMakeLists.txt`
- `tests/CMakeLists.txt` **does not exist**.
- `tests/test_physics.cpp` currently exists directly in `tests/`.
- In `tests/test_physics.cpp` (lines 40–46):
  ```cpp
  auto& registry = MaterialRegistry::Instance();
  bool loaded = registry.LoadFromDirectory("assets/materials");
  if (!loaded) {
      // Fallback check if relative to binary
      loaded = registry.LoadFromDirectory("../assets/materials");
  }
  assert(loaded && "Failed to load materials from assets/materials");
  ```
  This proves that running tests across different directories already encountered asset path discovery failures that required ad-hoc workarounds.

### 1.3 CMake Multi-Config Generator & Build Tree
- Inspecting `CMakePresets.json` and `build/`:
  - Generator: `Visual Studio 17 2022` (x64 multi-config).
  - Binary output location: `build/Release/sandbox.exe` and `build/Release/assets`.
  - For tests in subdirectories without explicit output directories, Visual Studio defaults to `build/tests/Release/` or `build/Release/`.
  - When CTest executes tests without `WORKING_DIRECTORY`, its working directory defaults to `${CMAKE_CURRENT_BINARY_DIR}` (`build/tests/`), where `assets/materials` does not exist by default.

---

## 2. Logic Chain

1. **CTest Root Registration Requirement**:
   - In CMake, CTest discovery requires `enable_testing()` to be invoked at the root `CMakeLists.txt` scope before any `add_subdirectory(...)` containing tests.
   - Without `enable_testing()` at root, any `add_test()` in `tests/CMakeLists.txt` is silently ignored by `ctest`.
   - Therefore, root `CMakeLists.txt` must contain `enable_testing()` and `add_subdirectory(tests)`.

2. **Source Modularization vs Recompilation**:
   - Currently, `sandbox` and `test_physics` each recompile `${CORE_SOURCES}`, `${SIM_SOURCES}`, and `${UI_SOURCES}`.
   - For `sandbox_e2e_tests`, re-compiling all simulation sources again would bloat build times and create maintenance overhead.
   - However, `tests/CMakeLists.txt` inherits variables from the parent directory (`${CORE_SOURCES}`, `${SIM_SOURCES}`, `${SDL2_TARGET}`, `imgui`).
   - The cleanest design is to either link a static `sandbox_core` library or consume `${CORE_SOURCES}` + `${SIM_SOURCES}` directly for headless simulation testing.
   - The simulation core (`Chunk`, `World`, `MaterialRegistry`, `Physics`, `ThreadPool`, `Config`) has zero dependency on SDL2 or ImGui. However, linking `${SDL2_TARGET}` and `imgui` to test targets ensures any cross-component fixtures or headless color buffer checks link without unresolved symbols.

3. **CTest Command Line & Tier Filtering Architecture**:
   - According to `SCOPE.md` and `PROJECT.md`, the E2E test runner executable `sandbox_e2e_tests` must support command line arguments:
     `--all`, `--tier=1`, `--tier=2`, `--tier=3`, `--tier=4`, `--verbose`.
   - By registering individual CTest tests for each tier in `tests/CMakeLists.txt`, developers and CI can run granular subsets:
     - `ctest -R e2e_all`: runs all tiers in a single process.
     - `ctest -R e2e_tier1`: runs only Tier 1 feature coverage tests.
     - `ctest -R e2e_tier2`: runs only Tier 2 boundary tests.
     - `ctest -R e2e_tier3`: runs only Tier 3 pairwise tests.
     - `ctest -R e2e_tier4`: runs only Tier 4 workload tests.
     - `ctest`: runs all registered test suites.

4. **Reliable Asset Discovery on Windows (4-Layer Defense)**:
   - Observation 1.2 and 1.3 demonstrate that asset lookup is fragile when switching between IDE runs, CTest runs, and direct CLI runs.
   - A single mechanism is insufficient because:
     - Setting CTest `WORKING_DIRECTORY` fixes `ctest`, but does not fix direct execution `.\build\Release\sandbox_e2e_tests.exe`.
     - Copying assets via `POST_BUILD` fixes direct execution from the target directory, but does not fix execution from root or arbitrary working directories.
     - Relative path fallbacks (`"../assets"`) fail in multi-config folders (`build/tests/Release`).
   - Therefore, a **4-Layer Defense** is required:
     1. **CTest Property**: `set_tests_properties(... PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")` ensures all CTest runs execute with project root as working directory.
     2. **POST_BUILD Copy**: `add_custom_command(TARGET sandbox_e2e_tests POST_BUILD ... copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:sandbox_e2e_tests>/assets)` guarantees `assets/` exists alongside the `.exe` for standalone execution.
     3. **Compile-Time Definition**: `target_compile_definitions(sandbox_e2e_tests PRIVATE SANDBOX_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")` bakes the source asset directory path into the binary.
     4. **Harness Fallback Resolver**: A helper in `e2e_framework.hpp` resolves paths in sequence: CWD -> Executable Directory -> Compile-Time Macro (`SANDBOX_ASSETS_DIR`) -> Parent Directory Traversal.

---

## 3. Caveats

1. **Concurrent Worker M1 Activity**:
   - `teamwork_worker_m1` is currently implementing M1 simulation features and temporarily added `test_physics` to root `CMakeLists.txt`.
   - When migrating `test_physics` to `tests/CMakeLists.txt`, care must be taken to coordinate with `teamwork_worker_m1` so changes are not overwritten. Root `CMakeLists.txt` should be updated to delegate all testing targets to `tests/CMakeLists.txt`.
2. **Missing `assets/reactions.json`**:
   - Currently, `assets/materials/` contains 18 JSON files, but `assets/reactions.json` does not exist yet (scheduled for Milestone M2).
   - Test harness asset loader fixtures should verify directory existence and handle missing `reactions.json` gracefully with clear error logging until M2 generates the reaction configuration.
3. **Multi-Config vs Single-Config Generators**:
   - Multi-config generators (MSVC Visual Studio) place binaries in `Release/` or `Debug/`.
   - Single-config generators (Ninja) place binaries directly in the build root.
   - The proposed CMake solution uses generator expressions (`$<TARGET_FILE:...>`, `$<TARGET_FILE_DIR:...>`), ensuring 100% portability across both generators.

---

## 4. Conclusion & Concrete Recommendations

### 4.1 Recommended Changes to Root `CMakeLists.txt`

1. **Add Testing Support**: Add `enable_testing()` immediately after project and runtime flags.
2. **Organize Core / Simulation Sources**: Ensure `CORE_SOURCES` and `SIM_SOURCES` are accessible to `tests/CMakeLists.txt` (or encapsulated in a static library `sandbox_core`).
3. **Delegate Tests**: Remove `test_physics` from root `CMakeLists.txt` and replace it with `add_subdirectory(tests)` at the end of the file.

#### Exact Diff for Root `CMakeLists.txt`:
```cmake
# --- After lines 17 (compiler options) ---
enable_testing()

...

# --- Replace lines 108-135 (old test_physics block) with ---
# 4. Включение поддиректории с тестами и CTest
add_subdirectory(tests)
```

---

### 4.2 Proposed File: `tests/CMakeLists.txt`

Create `c:\Users\Drilla\Desktop\Sandbox\tests\CMakeLists.txt` with the following complete, production-ready content:

```cmake
# ==============================================================================
# tests/CMakeLists.txt - Sandbox2D Test Suite & Runner Configuration
# ==============================================================================

# Common include directories for all test targets
set(TEST_COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}        # tests/ (for e2e/e2e_framework.hpp)
    ${CMAKE_SOURCE_DIR}/src            # src/ (for core/*.hpp, sim/*.hpp, ui/*.hpp)
    ${CMAKE_SOURCE_DIR}/external       # external/ (for nlohmann/json.hpp)
    ${CMAKE_SOURCE_DIR}/external/sdl2/include
)

# ------------------------------------------------------------------------------
# 1. E2E Test Suite Executable: sandbox_e2e_tests
# ------------------------------------------------------------------------------
set(E2E_SOURCES
    e2e/test_main.cpp
)

# Automatically include tier implementation files when created
file(GLOB E2E_TIER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/e2e/tier*.cpp"
)

add_executable(sandbox_e2e_tests
    ${E2E_SOURCES}
    ${E2E_TIER_SOURCES}
    ${CORE_SOURCES}
    ${SIM_SOURCES}
)

target_include_directories(sandbox_e2e_tests PRIVATE
    ${TEST_COMMON_INCLUDES}
)

target_compile_definitions(sandbox_e2e_tests PRIVATE
    SANDBOX_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

target_link_libraries(sandbox_e2e_tests PRIVATE
    ${SDL2_TARGET}
    imgui
)

if(WIN32)
    target_link_libraries(sandbox_e2e_tests PRIVATE winmm imm32 version setupapi user32 gdi32 shell32)
endif()

# Asset deployment: copy assets adjacent to test binary
add_custom_command(TARGET sandbox_e2e_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:sandbox_e2e_tests>/assets
)

# ------------------------------------------------------------------------------
# 2. CTest Registration for sandbox_e2e_tests
# ------------------------------------------------------------------------------
# Master test suite: runs all tiers
add_test(NAME e2e_all COMMAND sandbox_e2e_tests --all)

# Granular tier tests
add_test(NAME e2e_tier1 COMMAND sandbox_e2e_tests --tier=1)
add_test(NAME e2e_tier2 COMMAND sandbox_e2e_tests --tier=2)
add_test(NAME e2e_tier3 COMMAND sandbox_e2e_tests --tier=3)
add_test(NAME e2e_tier4 COMMAND sandbox_e2e_tests --tier=4)

# Set working directory to project root for all CTest runs
set_tests_properties(
    e2e_all
    e2e_tier1
    e2e_tier2
    e2e_tier3
    e2e_tier4
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ------------------------------------------------------------------------------
# 3. Unit Test Targets (Physics & Modular Tests)
# ------------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_physics.cpp")
    add_executable(test_physics
        test_physics.cpp
        ${CORE_SOURCES}
        ${SIM_SOURCES}
        ${UI_SOURCES}
    )

    target_include_directories(test_physics PRIVATE
        ${TEST_COMMON_INCLUDES}
    )

    target_link_libraries(test_physics PRIVATE
        ${SDL2_TARGET}
        imgui
    )

    if(WIN32)
        target_link_libraries(test_physics PRIVATE winmm imm32 version setupapi user32 gdi32 shell32)
    endif()

    add_custom_command(TARGET test_physics POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:test_physics>/assets
    )

    add_test(NAME unit_test_physics COMMAND test_physics)
    set_tests_properties(unit_test_physics PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()
```

---

### 4.3 Asset Resolver Implementation for `e2e_framework.hpp`

In `tests/e2e/e2e_framework.hpp`, include this asset resolver implementation:

```cpp
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace sandbox::test {

inline std::string ResolveAssetPath(const std::string& relativePath) {
    namespace fs = std::filesystem;

    // 1. Check relative to current working directory
    if (fs::exists(relativePath)) {
        return relativePath;
    }

    // 2. Check relative to executable directory (Windows GetModuleFileName)
#if defined(_WIN32)
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len > 0) {
        fs::path exeDir = fs::path(buffer).parent_path();
        if (fs::exists(exeDir / relativePath)) {
            return (exeDir / relativePath).string();
        }
        if (relativePath.rfind("assets/", 0) == 0) {
            std::string sub = relativePath.substr(7);
            if (fs::exists(exeDir / "assets" / sub)) {
                return (exeDir / "assets" / sub).string();
            }
        }
    }
#endif

    // 3. Check compile-time configured SANDBOX_ASSETS_DIR
#ifdef SANDBOX_ASSETS_DIR
    fs::path baseAssets(SANDBOX_ASSETS_DIR);
    if (relativePath == "assets" || relativePath == "assets/") {
        if (fs::exists(baseAssets)) return baseAssets.string();
    }
    if (relativePath.rfind("assets/", 0) == 0) {
        std::string sub = relativePath.substr(7);
        if (fs::exists(baseAssets / sub)) {
            return (baseAssets / sub).string();
        }
    }
    if (fs::exists(baseAssets / relativePath)) {
        return (baseAssets / relativePath).string();
    }
#endif

    // 4. Search upward up to 4 directory levels
    fs::path searchDir = fs::current_path();
    for (int i = 0; i < 4; ++i) {
        if (fs::exists(searchDir / relativePath)) {
            return (searchDir / relativePath).string();
        }
        if (!searchDir.has_parent_path()) break;
        searchDir = searchDir.parent_path();
    }

    return relativePath; // Fallback
}

} // namespace sandbox::test
```

---

## 5. Verification Method

To independently verify the CMake build infra and CTest integration once implemented:

1. **Configure CMake with Presets**:
   ```powershell
   cmake --preset release
   # OR
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   ```
   *Expected result*: CMake configuration succeeds, detecting `enable_testing()` and processing `tests/CMakeLists.txt`.

2. **Build Test Target**:
   ```powershell
   cmake --build build --config Release --target sandbox_e2e_tests
   ```
   *Expected result*: `sandbox_e2e_tests.exe` builds cleanly with exit code 0. Post-build copies `assets/` to the output folder.

3. **Verify CTest Test Registration**:
   ```powershell
   ctest --test-dir build -N
   ```
   *Expected output*: Lists all registered tests:
   ```
     Test #1: e2e_all
     Test #2: e2e_tier1
     Test #3: e2e_tier2
     Test #4: e2e_tier3
     Test #5: e2e_tier4
     Test #6: unit_test_physics
   ```

4. **Execute CTest Suite**:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
   *Expected result*: All registered tests run from the designated `WORKING_DIRECTORY` (`c:\Users\Drilla\Desktop\Sandbox`), discovering `assets/materials/*.json` without path errors and reporting 100% pass.

5. **Invalidation Conditions**:
   - Failure of CTest to list registered test targets (`ctest -N` shows 0 tests).
   - Linker errors indicating missing `MultiThreaded` CRT runtime matching.
   - `MaterialRegistry::LoadFromDirectory` returning false due to unresolvable asset paths.
