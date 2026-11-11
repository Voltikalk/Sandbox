#include "ui/Renderer.hpp"
#include "core/Math.hpp"
#include "core/Log.hpp"
#include <algorithm>

namespace sandbox::ui {

WorldRenderer::WorldRenderer(SDL_Renderer* renderer, core::i32 worldWidth, core::i32 worldHeight)
    : m_sdlRenderer(renderer), m_worldWidth(worldWidth), m_worldHeight(worldHeight) {
    
    m_backgroundColor = core::Color4u(18, 18, 22, 255).ToU32();
    m_pixelBuffer.resize(static_cast<size_t>(m_worldWidth) * static_cast<size_t>(m_worldHeight), m_backgroundColor);

    // Create streaming texture with RGBA32 pixel format
    m_texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        m_worldWidth,
        m_worldHeight
    );

    if (!m_texture) {
        LOG_ERROR("Renderer", "Failed to create SDL streaming texture: ", SDL_GetError());
    } else {
        // Enforce GL_NEAREST point filtering for crisp pixel art
        SDL_SetTextureScaleMode(m_texture, SDL_ScaleModeNearest);
    }
}

WorldRenderer::~WorldRenderer() {
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
}

void WorldRenderer::Render(sim::World& world, SDL_Renderer* renderer, const SDL_Rect* viewport) {
    if (!m_texture) return;

    size_t chunkCount = world.GetChunkCount();
    for (size_t i = 0; i < chunkCount; ++i) {
        sim::Chunk* chunk = world.GetChunkByIndex(i);
        if (chunk && chunk->isDirty) {
            UpdateChunkPixels(*chunk);
            chunk->isDirty = false;
        }
    }

    // Stream pixels to texture
    SDL_UpdateTexture(m_texture, nullptr, m_pixelBuffer.data(), m_worldWidth * static_cast<int>(sizeof(core::u32)));

    // Render scaled texture onto window
    SDL_RenderCopy(renderer, m_texture, nullptr, viewport);
}

void WorldRenderer::UpdateChunkPixels(const sim::Chunk& chunk) {
    core::i32 startWorldX = chunk.chunkX * sim::Chunk::SIZE;
    core::i32 startWorldY = chunk.chunkY * sim::Chunk::SIZE;

    for (core::i32 ly = 0; ly < sim::Chunk::SIZE; ++ly) {
        core::i32 wy = startWorldY + ly;
        size_t rowOffset = static_cast<size_t>(wy) * static_cast<size_t>(m_worldWidth);

        for (core::i32 lx = 0; lx < sim::Chunk::SIZE; ++lx) {
            core::i32 wx = startWorldX + lx;
            core::i32 localIdx = sim::Chunk::Index(lx, ly);
            sim::MaterialId matId = chunk.material[localIdx];

            if (matId == sim::EMPTY_MATERIAL_ID) {
                m_pixelBuffer[rowOffset + wx] = m_backgroundColor;
            } else {
                const auto& mat = sim::MaterialRegistry::Instance().Get(matId);
                core::i32 noise = core::CellNoiseVariation(wx, wy, mat.colorVariation);

                core::u8 r = static_cast<core::u8>(std::clamp<core::i32>(mat.color.r + noise, 0, 255));
                core::u8 g = static_cast<core::u8>(std::clamp<core::i32>(mat.color.g + noise, 0, 255));
                core::u8 b = static_cast<core::u8>(std::clamp<core::i32>(mat.color.b + noise, 0, 255));

                m_pixelBuffer[rowOffset + wx] = core::Color4u(r, g, b, mat.color.a).ToU32();
            }
        }
    }
}

bool WorldRenderer::ScreenToWorld(core::i32 screenX, core::i32 screenY,
                                  const SDL_Rect& viewport,
                                  core::i32& worldX, core::i32& worldY) const noexcept {
    if (viewport.w <= 0 || viewport.h <= 0) return false;
    if (screenX < viewport.x || screenX >= viewport.x + viewport.w ||
        screenY < viewport.y || screenY >= viewport.y + viewport.h) {
        return false;
    }

    worldX = static_cast<core::i32>((static_cast<core::i64>(screenX - viewport.x) * m_worldWidth) / viewport.w);
    worldY = static_cast<core::i32>((static_cast<core::i64>(screenY - viewport.y) * m_worldHeight) / viewport.h);
    return (worldX >= 0 && worldX < m_worldWidth && worldY >= 0 && worldY < m_worldHeight);
}

void WorldRenderer::MarkAllDirty() noexcept {
    std::fill(m_pixelBuffer.begin(), m_pixelBuffer.end(), m_backgroundColor);
}

} // namespace sandbox::ui
