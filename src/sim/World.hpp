#pragma once

#include "sim/Chunk.hpp"
#include "core/Types.hpp"
#include <vector>
#include <memory>

namespace sandbox::sim {

class World {
public:
    // Default world dimensions: 1280x768 (20x12 chunks = 240 chunks)
    static constexpr core::i32 DEFAULT_WIDTH = 1280;
    static constexpr core::i32 DEFAULT_HEIGHT = 768;

private:
    core::i32 m_width{DEFAULT_WIDTH};
    core::i32 m_height{DEFAULT_HEIGHT};
    core::i32 m_chunksX{DEFAULT_WIDTH / Chunk::SIZE};   // 20
    core::i32 m_chunksY{DEFAULT_HEIGHT / Chunk::SIZE};  // 12
    core::u8  m_currentParity{0};                       // 0 or 1, toggled each tick

    // Array of 64-byte aligned unique chunk pointers
    std::vector<std::unique_ptr<Chunk>> m_chunks;

public:
    explicit World(core::i32 width = DEFAULT_WIDTH, core::i32 height = DEFAULT_HEIGHT);
    ~World() = default;

    // Non-copyable, movable
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

    // Dimension accessors
    [[nodiscard]] constexpr core::i32 GetWidth() const noexcept { return m_width; }
    [[nodiscard]] constexpr core::i32 GetHeight() const noexcept { return m_height; }
    [[nodiscard]] constexpr core::i32 GetChunksX() const noexcept { return m_chunksX; }
    [[nodiscard]] constexpr core::i32 GetChunksY() const noexcept { return m_chunksY; }
    [[nodiscard]] size_t GetChunkCount() const noexcept { return m_chunks.size(); }
    [[nodiscard]] constexpr core::u8 GetCurrentParity() const noexcept { return m_currentParity; }

    // Bounds checking
    [[nodiscard]] constexpr bool IsInBounds(core::i32 x, core::i32 y) const noexcept {
        return x >= 0 && x < m_width && y >= 0 && y < m_height;
    }

    [[nodiscard]] constexpr bool IsChunkInBounds(core::i32 cx, core::i32 cy) const noexcept {
        return cx >= 0 && cx < m_chunksX && cy >= 0 && cy < m_chunksY;
    }

    // Fast coordinate transformations
    [[nodiscard]] static constexpr core::i32 WorldToChunkX(core::i32 x) noexcept { return x >> 6; }
    [[nodiscard]] static constexpr core::i32 WorldToChunkY(core::i32 y) noexcept { return y >> 6; }
    [[nodiscard]] static constexpr core::i32 WorldToLocalX(core::i32 x) noexcept { return x & 63; }
    [[nodiscard]] static constexpr core::i32 WorldToLocalY(core::i32 y) noexcept { return y & 63; }
    [[nodiscard]] static constexpr core::i32 LocalToWorldX(core::i32 cx, core::i32 lx) noexcept { return (cx << 6) | lx; }
    [[nodiscard]] static constexpr core::i32 LocalToWorldY(core::i32 cy, core::i32 ly) noexcept { return (cy << 6) | ly; }

    // Chunk flat index: cy * chunksX + cx
    [[nodiscard]] constexpr size_t ChunkIndex(core::i32 cx, core::i32 cy) const noexcept {
        return static_cast<size_t>(cy) * static_cast<size_t>(m_chunksX) + static_cast<size_t>(cx);
    }

    // Chunk retrieval
    [[nodiscard]] Chunk* GetChunk(core::i32 cx, core::i32 cy) noexcept;
    [[nodiscard]] const Chunk* GetChunk(core::i32 cx, core::i32 cy) const noexcept;
    [[nodiscard]] Chunk* GetChunkByIndex(size_t index) noexcept;
    [[nodiscard]] const Chunk* GetChunkByIndex(size_t index) const noexcept;
    [[nodiscard]] Chunk* GetChunkAtWorld(core::i32 x, core::i32 y) noexcept;
    [[nodiscard]] const Chunk* GetChunkAtWorld(core::i32 x, core::i32 y) const noexcept;

    // Cell manipulation
    bool SetCell(core::i32 x, core::i32 y, MaterialId mat, core::f32 temp = 20.0f,
                 core::u8 flags = FLAG_NONE, core::u8 lifetime = 0) noexcept;
    [[nodiscard]] MaterialId GetCellMaterial(core::i32 x, core::i32 y) const noexcept;
    [[nodiscard]] core::f32  GetCellTemperature(core::i32 x, core::i32 y) const noexcept;
    void SetCellTemperature(core::i32 x, core::i32 y, core::f32 temp) noexcept;
    [[nodiscard]] core::u8   GetCellFlags(core::i32 x, core::i32 y) const noexcept;
    void SetCellFlags(core::i32 x, core::i32 y, core::u8 flags) noexcept;
    [[nodiscard]] core::u8   GetCellLifetime(core::i32 x, core::i32 y) const noexcept;
    void SetCellLifetime(core::i32 x, core::i32 y, core::u8 lifetime) noexcept;

    // Cell swap (handles intra-chunk and inter-chunk)
    void SwapCells(core::i32 x1, core::i32 y1, core::i32 x2, core::i32 y2) noexcept;

    // Sleeping & wake-up cascade
    void WakeChunk(core::i32 cx, core::i32 cy) noexcept;
    void WakeNeighbors(core::i32 cx, core::i32 cy) noexcept;
    void WakeNeighborsForCell(core::i32 cx, core::i32 cy, core::i32 lx, core::i32 ly) noexcept;
    void WakeChunkAtWorld(core::i32 x, core::i32 y) noexcept;

    // Simulation lifecycle
    void AdvanceTick() noexcept;
    void Clear(MaterialId fillMat = EMPTY_MATERIAL_ID, core::f32 temp = 20.0f) noexcept;

    // Telemetry
    [[nodiscard]] core::u32 GetTotalActiveCells() const noexcept;
    [[nodiscard]] core::u32 GetAwakeChunkCount() const noexcept;
};

} // namespace sandbox::sim
