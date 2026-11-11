# ==============================================================================
# tests/CMakeLists.txt - Sandbox2D Test Suite & Runner Configuration
# ==============================================================================

# Common include directories for all test targets
set(TEST_COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}        # tests/
    ${CMAKE_CURRENT_SOURCE_DIR}/e2e    # tests/e2e/
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

# Automatically include tier implementation files when created in subsequent milestones
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

# Output executable directly to build/<Config>/ alongside sandbox
set_target_properties(sandbox_e2e_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

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
add_test(NAME e2e_tier5 COMMAND sandbox_e2e_tests --tier=5)

# Set working directory to project root for all CTest runs
set_tests_properties(
    e2e_all
    e2e_tier1
    e2e_tier2
    e2e_tier3
    e2e_tier4
    e2e_tier5
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

    set_target_properties(test_physics PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

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

# ------------------------------------------------------------------------------
# 4. Challenger 2 Adversarial Stress Test Suite
# ------------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_challenger2_stress.cpp")
    add_executable(test_challenger2_stress
        test_challenger2_stress.cpp
        ${CORE_SOURCES}
        ${SIM_SOURCES}
        ${UI_SOURCES}
    )

    target_include_directories(test_challenger2_stress PRIVATE
        ${TEST_COMMON_INCLUDES}
    )

    target_link_libraries(test_challenger2_stress PRIVATE
        ${SDL2_TARGET}
        imgui
    )

    if(WIN32)
        target_link_libraries(test_challenger2_stress PRIVATE winmm imm32 version setupapi user32 gdi32 shell32)
    endif()

    set_target_properties(test_challenger2_stress PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    add_custom_command(TARGET test_challenger2_stress POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:test_challenger2_stress>/assets
    )

    add_test(NAME unit_test_challenger2_stress COMMAND test_challenger2_stress)
    set_tests_properties(unit_test_challenger2_stress PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

# ------------------------------------------------------------------------------
# 5. Milestone 2 Unit Test Suite: test_thermo_reactions
# ------------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_thermo_reactions.cpp")
    add_executable(test_thermo_reactions
        test_thermo_reactions.cpp
        ${CORE_SOURCES}
        ${SIM_SOURCES}
        ${UI_SOURCES}
    )

    target_include_directories(test_thermo_reactions PRIVATE
        ${TEST_COMMON_INCLUDES}
    )

    target_link_libraries(test_thermo_reactions PRIVATE
        ${SDL2_TARGET}
        imgui
    )

    if(WIN32)
        target_link_libraries(test_thermo_reactions PRIVATE winmm imm32 version setupapi user32 gdi32 shell32)
    endif()

    set_target_properties(test_thermo_reactions PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    add_custom_command(TARGET test_thermo_reactions POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:test_thermo_reactions>/assets
    )

    add_test(NAME unit_test_thermo_reactions COMMAND test_thermo_reactions)
    set_tests_properties(unit_test_thermo_reactions PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()


