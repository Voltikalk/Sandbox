#include "sim/World.hpp"
#include <utility>

namespace sandbox::sim {

World::World(core::i32 width, core::i32 height) {
    // Clamp dimensions to multiples of CHUNK_SIZE (64)
    m_chunksX = std::max(1, (width + Chunk::SIZE - 1) / Chunk::SIZE);
    m_chunksY = std::max(1, (height + Chunk::SIZE - 1) / Chunk::SIZE);
    m_width = m_chunksX * Chunk::SIZE;
    m_height = m_chunksY * Chunk::SIZE;

    size_t totalChunks = static_cast<size_t>(m_chunksX) * static_cast<size_t>(m_chunksY);
    m_chunks.reserve(totalChunks);

    for (core::i32 cy = 0; cy < m_chunksY; ++cy) {
        for (core::i32 cx = 0; cx < m_chunksX; ++cx) {
            m_chunks.push_back(std::make_unique<Chunk>(cx, cy));
        }
    }
}

Chunk* World::GetChunk(core::i32 cx, core::i32 cy) noexcept {
    if (!IsChunkInBounds(cx, cy)) return nullptr;
    return m_chunks[ChunkIndex(cx, cy)].get();
}

const Chunk* World::GetChunk(core::i32 cx, core::i32 cy) const noexcept {
    if (!IsChunkInBounds(cx, cy)) return nullptr;
    return m_chunks[ChunkIndex(cx, cy)].get();
}

Chunk* World::GetChunkByIndex(size_t index) noexcept {
    if (index >= m_chunks.size()) return nullptr;
    return m_chunks[index].get();
}

const Chunk* World::GetChunkByIndex(size_t index) const noexcept {
    if (index >= m_chunks.size()) return nullptr;
    return m_chunks[index].get();
}

Chunk* World::GetChunkAtWorld(core::i32 x, core::i32 y) noexcept {
    if (!IsInBounds(x, y)) return nullptr;
    return GetChunk(WorldToChunkX(x), WorldToChunkY(y));
}

const Chunk* World::GetChunkAtWorld(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return nullptr;
    return GetChunk(WorldToChunkX(x), WorldToChunkY(y));
}

bool World::SetCell(core::i32 x, core::i32 y, MaterialId mat, core::f32 temp,
                    core::u8 flags, core::u8 lifetime) noexcept {
    if (!IsInBounds(x, y)) return false;

    core::i32 cx = WorldToChunkX(x);
    core::i32 cy = WorldToChunkY(y);
    core::i32 lx = WorldToLocalX(x);
    core::i32 ly = WorldToLocalY(y);

    Chunk* chunk = GetChunk(cx, cy);
    if (!chunk) return false;

    // Newly set cells receive opposite of current parity so they are eligible to simulate on the current tick
    core::u8 finalFlags = (flags & ~FLAG_PARITY) | (m_currentParity ^ FLAG_PARITY);

    chunk->SetCell(lx, ly, mat, temp, finalFlags, lifetime);

    // Wake neighbors if cell is placed at the chunk border
    WakeNeighborsForCell(cx, cy, lx, ly);

    return true;
}

MaterialId World::GetCellMaterial(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return EMPTY_MATERIAL_ID;
    const Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    return chunk ? chunk->material[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] : EMPTY_MATERIAL_ID;
}

core::f32 World::GetCellTemperature(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return 20.0f;
    const Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    return chunk ? chunk->temperature[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] : 20.0f;
}

void World::SetCellTemperature(core::i32 x, core::i32 y, core::f32 temp) noexcept {
    if (!IsInBounds(x, y)) return;
    Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    if (chunk) {
        chunk->temperature[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] = temp;
        chunk->isDirty = true;
    }
}

core::u8 World::GetCellFlags(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return FLAG_NONE;
    const Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    return chunk ? chunk->flags[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] : FLAG_NONE;
}

void World::SetCellFlags(core::i32 x, core::i32 y, core::u8 flags) noexcept {
    if (!IsInBounds(x, y)) return;
    Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    if (chunk) {
        chunk->flags[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] = flags;
        chunk->isDirty = true;
    }
}

core::u8 World::GetCellLifetime(core::i32 x, core::i32 y) const noexcept {
    if (!IsInBounds(x, y)) return 0;
    const Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    return chunk ? chunk->lifetime[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] : 0;
}

void World::SetCellLifetime(core::i32 x, core::i32 y, core::u8 lifetime) noexcept {
    if (!IsInBounds(x, y)) return;
    Chunk* chunk = GetChunk(WorldToChunkX(x), WorldToChunkY(y));
    if (chunk) {
        chunk->lifetime[Chunk::Index(WorldToLocalX(x), WorldToLocalY(y))] = lifetime;
        chunk->isDirty = true;
    }
}

void World::SwapCells(core::i32 x1, core::i32 y1, core::i32 x2, core::i32 y2) noexcept {
    if (!IsInBounds(x1, y1) || !IsInBounds(x2, y2)) return;

    core::i32 cx1 = WorldToChunkX(x1);
    core::i32 cy1 = WorldToChunkY(y1);
    core::i32 lx1 = WorldToLocalX(x1);
    core::i32 ly1 = WorldToLocalY(y1);
    core::i32 idx1 = Chunk::Index(lx1, ly1);

    core::i32 cx2 = WorldToChunkX(x2);
    core::i32 cy2 = WorldToChunkY(y2);
    core::i32 lx2 = WorldToLocalX(x2);
    core::i32 ly2 = WorldToLocalY(y2);
    core::i32 idx2 = Chunk::Index(lx2, ly2);

    Chunk* c1 = GetChunk(cx1, cy1);
    Chunk* c2 = (cx1 == cx2 && cy1 == cy2) ? c1 : GetChunk(cx2, cy2);

    if (!c1 || !c2) return;

    // Track active cell count changes if swapping between different chunks
    if (c1 != c2) {
        bool m1Empty = (c1->material[idx1] == EMPTY_MATERIAL_ID);
        bool m2Empty = (c2->material[idx2] == EMPTY_MATERIAL_ID);
        if (m1Empty && !m2Empty) {
            ++c1->activeCellCount;
            if (c2->activeCellCount > 0) --c2->activeCellCount;
        } else if (!m1Empty && m2Empty) {
            if (c1->activeCellCount > 0) --c1->activeCellCount;
            ++c2->activeCellCount;
        }
        if (c1->activeCellCount == 0) c1->ResetActiveY();
        if (c2->activeCellCount == 0) c2->ResetActiveY();
    }

    // Direct memory swap of cell properties
    std::swap(c1->material[idx1], c2->material[idx2]);
    std::swap(c1->temperature[idx1], c2->temperature[idx2]);
    std::swap(c1->lifetime[idx1], c2->lifetime[idx2]);

    // Stamp current tick parity to prevent double-updates
    c1->flags[idx1] = (c1->flags[idx1] & ~FLAG_PARITY) | m_currentParity;
    c2->flags[idx2] = (c2->flags[idx2] & ~FLAG_PARITY) | m_currentParity;

    c1->isDirty = true;
    c1->WakeUp();

    // Expand active vertical bounds in chunks receiving non-empty material
    if (c1->material[idx1] != EMPTY_MATERIAL_ID) {
        c1->ExpandActiveY(ly1);
    }
    if (c2->material[idx2] != EMPTY_MATERIAL_ID) {
        c2->ExpandActiveY(ly2);
    }

    if (c1 != c2) {
        c2->isDirty = true;
        c2->WakeUp();
    }

    // Wake neighbor chunks if cells touch chunk borders
    WakeNeighborsForCell(cx1, cy1, lx1, ly1);
    if (c1 != c2 || lx1 != lx2 || ly1 != ly2) {
        WakeNeighborsForCell(cx2, cy2, lx2, ly2);
    }
}

void World::WakeNeighborsForCell(core::i32 cx, core::i32 cy, core::i32 lx, core::i32 ly) noexcept {
    // Orthogonal edge neighbors
    if (lx == 0 && cx > 0) WakeChunk(cx - 1, cy);
    if (lx == Chunk::SIZE - 1 && cx + 1 < m_chunksX) WakeChunk(cx + 1, cy);
    if (ly == 0 && cy > 0) WakeChunk(cx, cy - 1);
    if (ly == Chunk::SIZE - 1 && cy + 1 < m_chunksY) WakeChunk(cx, cy + 1);

    // Diagonal corner neighbors
    if (lx == 0 && ly == 0 && cx > 0 && cy > 0)
        WakeChunk(cx - 1, cy - 1);
    if (lx == Chunk::SIZE - 1 && ly == 0 && cx + 1 < m_chunksX && cy > 0)
        WakeChunk(cx + 1, cy - 1);
    if (lx == 0 && ly == Chunk::SIZE - 1 && cx > 0 && cy + 1 < m_chunksY)
        WakeChunk(cx - 1, cy + 1);
    if (lx == Chunk::SIZE - 1 && ly == Chunk::SIZE - 1 && cx + 1 < m_chunksX && cy + 1 < m_chunksY)
        WakeChunk(cx + 1, cy + 1);
}

void World::WakeChunk(core::i32 cx, core::i32 cy) noexcept {
    Chunk* chunk = GetChunk(cx, cy);
    if (chunk) {
        chunk->WakeUp();
    }
}

void World::WakeNeighbors(core::i32 cx, core::i32 cy) noexcept {
    for (core::i32 dy = -1; dy <= 1; ++dy) {
        for (core::i32 dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            WakeChunk(cx + dx, cy + dy);
        }
    }
}

void World::WakeChunkAtWorld(core::i32 x, core::i32 y) noexcept {
    if (IsInBounds(x, y)) {
        WakeChunk(WorldToChunkX(x), WorldToChunkY(y));
    }
}

void World::AdvanceTick() noexcept {
    m_currentParity ^= FLAG_PARITY;
}

void World::Clear(MaterialId fillMat, core::f32 temp) noexcept {
    for (auto& chunk : m_chunks) {
        if (chunk) {
            chunk->Clear(fillMat, temp);
        }
    }
}

core::u32 World::GetTotalActiveCells() const noexcept {
    core::u32 total = 0;
    for (const auto& chunk : m_chunks) {
        if (chunk) {
            total += chunk->activeCellCount;
        }
    }
    return total;
}

core::u32 World::GetAwakeChunkCount() const noexcept {
    core::u32 awake = 0;
    for (const auto& chunk : m_chunks) {
        if (chunk && !chunk->isSleeping) {
            ++awake;
        }
    }
    return awake;
}

} // namespace sandbox::sim
