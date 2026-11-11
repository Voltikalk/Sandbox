cmake_minimum_required(VERSION 3.20)
project(Sandbox2D LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Статическая линковка рантайма MSVC (/MT в Release, /MTd в Debug)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /permissive- /utf-8 /MP)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
    endif()
endif()

enable_testing()

# 1. SDL2 (статическая сборка из external или системы)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/sdl2/CMakeLists.txt")
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/sdl2 EXCLUDE_FROM_ALL)
    set(SDL2_TARGET "SDL2-static")
else()
    find_package(SDL2 REQUIRED CONFIG)
    if(TARGET SDL2::SDL2-static)
        set(SDL2_TARGET "SDL2::SDL2-static")
    else()
        set(SDL2_TARGET "SDL2::SDL2")
    endif()
endif()

# 2. Dear ImGui (статическая библиотека с бэкендами SDL2 + SDL_Renderer2)
add_library(imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_sdl2.cpp
    external/imgui/backends/imgui_impl_sdlrenderer2.cpp
)

target_include_directories(imgui PUBLIC
    external/imgui
    external/imgui/backends
)

target_link_libraries(imgui PUBLIC ${SDL2_TARGET})

if(MSVC)
    target_compile_options(imgui PRIVATE /W0) # глушим сторонние предупреждения
else()
    target_compile_options(imgui PRIVATE -w)
endif()

# 3. Исходные файлы ядра, симуляции и интерфейса
set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ThreadPool.cpp
)

set(SIM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Chunk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/World.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/MaterialRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Physics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/Thermodynamics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/ReactionSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sim/ExplosionSystem.cpp
)

set(UI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Brush.cpp
)

set(ALL_SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${SIM_SOURCES}
    ${UI_SOURCES}
)

add_executable(sandbox ${ALL_SOURCES})

target_include_directories(sandbox PRIVATE
    src
    external
    external/sdl2/include
)

target_link_libraries(sandbox PRIVATE
    ${SDL2_TARGET}
    imgui
)

if(WIN32)
    target_link_libraries(sandbox PRIVATE winmm imm32 version setupapi user32 gdi32 shell32)
endif()

# Копирование ресурсов рядом с .exe
add_custom_command(TARGET sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:sandbox>/assets
)

# 4. Тестовая подсистема и CTest
add_subdirectory(tests)
