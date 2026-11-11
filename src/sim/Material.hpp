#pragma once

#include "core/Types.hpp"
#include <string>

namespace sandbox::sim {

using MaterialId = core::u16;
inline constexpr MaterialId EMPTY_MATERIAL_ID = 0;

enum class MaterialState : core::u8 {
    Solid = 0,
    Powder = 1,
    Liquid = 2,
    Gas = 3
};

struct Material {
    MaterialId id{EMPTY_MATERIAL_ID};
    std::string name{"air"};
    std::string category{"gases"};
    core::Color4u color{0, 0, 0, 0};
    core::u8 colorVariation{0};
    MaterialState state{MaterialState::Gas};

    // Mechanical properties
    core::f32 density{0.0f};
    core::i32 dispersion{0};
    core::f32 strength{0.0f};

    // Thermal properties
    core::f32 thermalConductivity{0.02f};
    core::f32 specificHeat{1.0f};
    core::f32 meltPoint{9999.0f};
    core::f32 boilPoint{9999.0f};
    core::f32 freezePoint{-9999.0f};

    // Phase transition targets (Names for JSON deserialization, IDs for O(1) runtime)
    std::string meltTargetName{""};
    std::string boilTargetName{""};
    std::string freezeTargetName{""};
    MaterialId meltMaterialId{EMPTY_MATERIAL_ID};
    MaterialId boilMaterialId{EMPTY_MATERIAL_ID};
    MaterialId freezeMaterialId{EMPTY_MATERIAL_ID};

    // Combustion & lifecycle
    core::f32 flammability{0.0f};
    core::f32 ignitionTemp{10000.0f};
    core::u8 lifetime{0};
};

} // namespace sandbox::sim
