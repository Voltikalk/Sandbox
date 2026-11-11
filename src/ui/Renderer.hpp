#pragma once

#include "sim/World.hpp"
#include "sim/MaterialRegistry.hpp"
#include "core/Types.hpp"
#include <SDL.h>
#include <vector>

namespace sandbox::ui {

class WorldRenderer {
private:
    SDL_Renderer*          m_sdlRenderer{nullptr};
    SDL_Texture*           m_texture{nullptr};
    core::i32              m_worldWidth{0};
    core::i32              m_worldHeight{0};
    std::vector<core::u32> m_pixelBuffer;
    core::u32              m_backgroundColor{0xFF161212}; // Little-Endian RGBA(18, 18, 22, 255)

public:
    WorldRenderer(SDL_Renderer* renderer, core::i32 worldWidth, core::i32 worldHeight);
    ~WorldRenderer();

    WorldRenderer(const WorldRenderer&) = delete;
    WorldRenderer& operator=(const WorldRenderer&) = delete;

    // Update pixel buffer from dirty chunks and stream to GPU
    void Render(sim::World& world, SDL_Renderer* renderer, const SDL_Rect* viewport = nullptr);

    // Coordinate conversion: converts screen pixels (sx, sy) to world cells (wx, wy)
    [[nodiscard]] bool ScreenToWorld(core::i32 screenX, core::i32 screenY,
                                     const SDL_Rect& viewport,
                                     core::i32& worldX, core::i32& worldY) const noexcept;

    void MarkAllDirty() noexcept;

private:
    void UpdateChunkPixels(const sim::Chunk& chunk);
};

} // namespace sandbox::ui
