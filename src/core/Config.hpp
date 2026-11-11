#pragma once

#include "Types.hpp"
#include <string>

namespace sandbox::core {

struct WindowConfig {
    std::string title{"Pixel Sandbox 2D"};
    i32 width{1280};
    i32 height{720};
    bool vsync{true};
    bool resizable{true};
};

struct SimConfig {
    i32 worldWidth{1280};   // 20 чанков по 64
    i32 worldHeight{768};   // 12 чанков по 64
    i32 targetTPS{60};
    i32 maxSubsteps{5};
    f32 gravity{1.0f};
    u32 threadCount{0};     // 0 = автоопределение по числу ядер
    f32 timeScale{1.0f};
    bool paused{false};
};

struct RenderConfig {
    bool showChunkGrid{false};
    bool showDebugOverlay{false};
    bool enableGlow{true};
    f32 renderScale{1.0f};
};

struct AppConfig {
    WindowConfig window;
    SimConfig sim;
    RenderConfig render;

    bool LoadFromFile(const std::string& filepath);
    bool SaveToFile(const std::string& filepath) const;
};

} // namespace sandbox::core

