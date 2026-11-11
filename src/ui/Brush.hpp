#pragma once

#include "sim/World.hpp"
#include "ui/Renderer.hpp"
#include "core/Types.hpp"
#include <SDL.h>

namespace sandbox::ui {

class Brush {
public:
    static constexpr core::i32 MIN_RADIUS = 1;
    static constexpr core::i32 MAX_RADIUS = 50;

private:
    sim::MaterialId m_selectedMaterial{3}; // Default to Sand (ID 3)
    core::i32       m_radius{5};
    bool            m_isPainting{false};
    bool            m_isErasing{false};
    core::Point2i   m_lastMousePos{0, 0};

public:
    Brush() noexcept = default;

    // Properties
    void SetMaterial(sim::MaterialId id) noexcept { m_selectedMaterial = id; }
    [[nodiscard]] sim::MaterialId GetMaterial() const noexcept { return m_selectedMaterial; }

    void SetRadius(core::i32 r) noexcept { m_radius = std::clamp(r, MIN_RADIUS, MAX_RADIUS); }
    [[nodiscard]] core::i32 GetRadius() const noexcept { return m_radius; }

    void SetErasing(bool erasing) noexcept { m_isErasing = erasing; }
    [[nodiscard]] bool IsErasing() const noexcept { return m_isErasing; }

    [[nodiscard]] bool IsPainting() const noexcept { return m_isPainting; }

    // Public Programmatic Drawing API
    void PaintDisk(sim::World& world, core::i32 cx, core::i32 cy) noexcept;
    void PaintDisk(sim::World& world, core::i32 cx, core::i32 cy, sim::MaterialId mat, core::i32 radius) noexcept;

    void PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1) noexcept;
    void PaintStroke(sim::World& world, core::Point2i from, core::Point2i to) noexcept;
    void PaintStroke(sim::World& world, core::i32 x0, core::i32 y0, core::i32 x1, core::i32 y1, sim::MaterialId mat, core::i32 radius) noexcept;

    // Event handling
    void HandleEvent(const SDL_Event& event, const WorldRenderer& renderer, const SDL_Rect& viewport, sim::World& world) noexcept;

    // Per-frame update (for smooth continuous strokes while button is held)
    void Update(const WorldRenderer& renderer, const SDL_Rect& viewport, sim::World& world) noexcept;
};

} // namespace sandbox::ui
