#include "ui/Brush.hpp"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace sandbox::ui {

void Brush::HandleEvent(const SDL_Event& event, const WorldRenderer& renderer, const SDL_Rect& viewport, sim::World& world) noexcept {
    // If Dear ImGui wants mouse input, brush must ignore it
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
        m_isPainting = false;
        m_isErasing = false;
        return;
    }

    if (event.type == SDL_MOUSEWHEEL) {
        if (event.wheel.y > 0) {
            SetRadius(m_radius + 1);
        } else if (event.wheel.y < 0) {
            SetRadius(m_radius - 1);
        }
    } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        core::i32 wx = 0, wy = 0;
        if (renderer.ScreenToWorld(event.button.x, event.button.y, viewport, wx, wy)) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                m_isPainting = true;
                m_isErasing = false;
                m_lastMousePos = { wx, wy };
                PaintDisk(world, wx, wy, m_selectedMaterial, m_radius);
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                m_isPainting = true;
                m_isErasing = true;
                m_lastMousePos = { wx, wy };
                PaintDisk(world, wx, wy, sim::EMPTY_MATERIAL_ID, m_radius);
            }
        }
    } else if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
            m_isPainting = false;
            m_isErasing = false;
        }
    }
}

void Brush::Update(const WorldRenderer& renderer, const SDL_Rect& viewport, sim::World& world) noexcept {
    if (!m_isPainting) return;

    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
        m_isPainting = false;
        m_isErasing = false;
        return;
    }

    int mx = 0, my = 0;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);

    bool lmb = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    bool rmb = (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;

    if (!lmb && !rmb) {
        m_isPainting = false;
        return;
    }

    core::i32 wx = 0, wy = 0;
    if (renderer.ScreenToWorld(mx, my, viewport, wx, wy)) {
        core::Point2i currPos{ wx, wy };
        PaintStroke(world, m_lastMousePos, currPos);
        m_lastMousePos = currPos;
    }
}

void Brush::PaintDisk(sim::World& world, core::i32 cx, core::i32 cy) noexcept {
    sim::MaterialId mat = m_isErasing ? sim::EMPTY_MATERIAL_ID : m_selectedMaterial;
    PaintDisk(world, cx, cy, mat, m_radius);
}

void Brush::PaintDisk(sim::World& world, core::i32 cx, core::i32 cy, sim::MaterialId mat, core::i32 radius) noexcept {
    if (radius <= 1) {
        world.SetCell(cx, cy, mat);
        return;
    }

    core::i32 r2 = radius * radius;
    for (core::i32 dy = -radius; dy <= radius; ++dy) {
        core::i32 wy = cy + dy;
        if (wy < 0 || wy >= world.GetHeight()) continue;

        for (core::i32 dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                core::i32 wx = cx + dx;
                if (wx >= 0 && wx < world.GetWidth()) {
                    world.SetCell(wx, wy, mat);
                }
            }
        }
    }
}

void Brush::PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1) noexcept {
    sim::MaterialId mat = m_isErasing ? sim::EMPTY_MATERIAL_ID : m_selectedMaterial;
    PaintStroke(world, x0, y0, x1, y1, mat, m_radius);
}

void Brush::PaintStroke(sim::World& world, core::Point2i from, core::Point2i to) noexcept {
    PaintStroke(world, from.x, from.y, to.x, to.y);
}

void Brush::PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1, sim::MaterialId mat, core::i32 radius) noexcept {
    // Bresenham's line algorithm for continuous stroke interpolation across all octants
    core::i32 dx = std::abs(x1 - x0);
    core::i32 dy = -std::abs(y1 - y0);
    core::i32 sx = (x0 < x1) ? 1 : -1;
    core::i32 sy = (y0 < y1) ? 1 : -1;
    core::i32 err = dx + dy;

    while (true) {
        PaintDisk(world, x0, y0, mat, radius);
        if (x0 == x1 && y0 == y1) break;

        core::i32 e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

} // namespace sandbox::ui
