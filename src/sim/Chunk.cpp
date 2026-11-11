#include "sim/Chunk.hpp"
#include <cstring>

namespace sandbox::sim {

Chunk::Chunk() noexcept : Chunk(0, 0) {}

Chunk::Chunk(core::i32 cx, core::i32 cy) noexcept
    : chunkX(cx), chunkY(cy) {
    Clear(EMPTY_MATERIAL_ID, 20.0f);
}

void Chunk::Clear(MaterialId fillMat, core::f32 defaultTemp) noexcept {
    if (fillMat == EMPTY_MATERIAL_ID) {
        std::memset(material, 0, sizeof(material));
        activeCellCount = 0;
        minActiveY = SIZE;
        maxActiveY = -1;
    } else {
        std::fill_n(material, AREA, fillMat);
        activeCellCount = AREA;
        minActiveY = 0;
        maxActiveY = SIZE - 1;
    }
    std::fill_n(temperature, AREA, defaultTemp);
    std::memset(flags, 0, sizeof(flags));
    std::memset(lifetime, 0, sizeof(lifetime));

    isDirty = true;
    isSleeping = false;
    sleepCounter = SLEEP_HYSTERESIS;
}

void Chunk::SetCell(core::i32 lx, core::i32 ly, MaterialId mat, core::f32 temp,
                    core::u8 flag, core::u8 life) noexcept {
    core::i32 idx = Index(lx, ly);
    MaterialId oldMat = material[idx];

    if (oldMat == EMPTY_MATERIAL_ID && mat != EMPTY_MATERIAL_ID) {
        ++activeCellCount;
    } else if (oldMat != EMPTY_MATERIAL_ID && mat == EMPTY_MATERIAL_ID) {
        if (activeCellCount > 0) --activeCellCount;
    }

    material[idx] = mat;
    temperature[idx] = temp;
    flags[idx] = flag;
    lifetime[idx] = life;

    isDirty = true;
    WakeUp();

    if (mat != EMPTY_MATERIAL_ID) {
        ExpandActiveY(ly);
    } else if (activeCellCount == 0) {
        ResetActiveY();
    }
}

void Chunk::WakeUp() noexcept {
    isSleeping = false;
    sleepCounter = SLEEP_HYSTERESIS;
}

void Chunk::StepSleep() noexcept {
    if (isSleeping) return;

    if (activeCellCount == 0) {
        isSleeping = true;
        return;
    }

    if (sleepCounter > 0) {
        --sleepCounter;
    } else {
        isSleeping = true;
    }
}

void Chunk::UpdateActiveBounds() noexcept {
    if (activeCellCount == 0) {
        minActiveY = SIZE;
        maxActiveY = -1;
        return;
    }

    core::i32 newMin = -1;
    core::i32 newMax = -1;

    for (core::i32 ly = 0; ly < SIZE; ++ly) {
        core::i32 rowOffset = ly << 6;
        for (core::i32 lx = 0; lx < SIZE; ++lx) {
            if (material[rowOffset | lx] != EMPTY_MATERIAL_ID) {
                if (newMin == -1) newMin = ly;
                newMax = ly;
                break;
            }
        }
    }

    minActiveY = (newMin != -1) ? newMin : SIZE;
    maxActiveY = (newMax != -1) ? newMax : -1;
}

} // namespace sandbox::sim
