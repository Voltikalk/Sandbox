#include "sim/MaterialRegistry.hpp"
#include "core/Log.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace sandbox::sim {

namespace fs = std::filesystem;
using json = nlohmann::json;

MaterialRegistry::MaterialRegistry() {
    // Air is guaranteed at ID 0
    m_emptyMaterial.id = EMPTY_MATERIAL_ID;
    m_emptyMaterial.name = "air";
    m_emptyMaterial.category = "gases";
    m_emptyMaterial.color = core::Color4u(0, 0, 0, 0);
    m_emptyMaterial.state = MaterialState::Gas;
    m_emptyMaterial.density = 0.0f;
    RegisterMaterial(m_emptyMaterial);
}

MaterialRegistry& MaterialRegistry::Instance() {
    static MaterialRegistry instance;
    return instance;
}

void MaterialRegistry::Clear() {
    m_materials.clear();
    m_nameToId.clear();
    RegisterMaterial(m_emptyMaterial); // Re-seed Air at ID 0
}

MaterialId MaterialRegistry::RegisterMaterial(Material mat) {
    auto it = m_nameToId.find(mat.name);
    if (it != m_nameToId.end()) {
        m_materials[it->second] = mat;
        return it->second;
    }

    if (mat.name == "air") {
        mat.id = EMPTY_MATERIAL_ID;
        if (m_materials.empty()) {
            m_materials.push_back(mat);
        } else {
            m_materials[0] = mat;
        }
        m_nameToId["air"] = EMPTY_MATERIAL_ID;
        return EMPTY_MATERIAL_ID;
    }

    if (mat.id != EMPTY_MATERIAL_ID) {
        if (m_materials.size() <= mat.id) {
            m_materials.resize(mat.id + 1, m_emptyMaterial);
        }
        m_materials[mat.id] = mat;
        m_nameToId[mat.name] = mat.id;
        return mat.id;
    }

    MaterialId newId = static_cast<MaterialId>(m_materials.size());
    mat.id = newId;
    m_materials.push_back(mat);
    m_nameToId[mat.name] = newId;
    return newId;
}

const Material& MaterialRegistry::Get(MaterialId id) const noexcept {
    if (id < m_materials.size()) {
        return m_materials[id];
    }
    return m_emptyMaterial;
}

MaterialId MaterialRegistry::FindByName(const std::string& name) const noexcept {
    auto it = m_nameToId.find(name);
    if (it != m_nameToId.end()) {
        return it->second;
    }
    return EMPTY_MATERIAL_ID;
}

size_t MaterialRegistry::Count() const noexcept {
    return m_materials.size();
}

const std::vector<Material>& MaterialRegistry::GetAll() const noexcept {
    return m_materials;
}

bool MaterialRegistry::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_WARN("MaterialRegistry", "Failed to open material file: ", filePath);
        return false;
    }

    try {
        json j;
        file >> j;

        Material mat;
        mat.name = j.value("name", "unnamed");
        mat.category = j.value("category", "solids");
        if (j.contains("id")) {
            mat.id = static_cast<MaterialId>(j["id"].get<int>());
        }

        // Parse State
        std::string stateStr = j.value("state", "Solid");
        if (stateStr == "Powder" || stateStr == "POWDER") mat.state = MaterialState::Powder;
        else if (stateStr == "Liquid" || stateStr == "LIQUID") mat.state = MaterialState::Liquid;
        else if (stateStr == "Gas" || stateStr == "GAS") mat.state = MaterialState::Gas;
        else mat.state = MaterialState::Solid;

        // Parse Color
        if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
            mat.color.r = static_cast<core::u8>(j["color"][0].get<int>());
            mat.color.g = static_cast<core::u8>(j["color"][1].get<int>());
            mat.color.b = static_cast<core::u8>(j["color"][2].get<int>());
            mat.color.a = j["color"].size() >= 4 ? static_cast<core::u8>(j["color"][3].get<int>()) : 255;
        }

        // Color Variation
        if (j.contains("color_variation")) mat.colorVariation = static_cast<core::u8>(j["color_variation"].get<int>());
        else if (j.contains("colorVariation")) mat.colorVariation = static_cast<core::u8>(j["colorVariation"].get<int>());

        // Mechanical properties
        mat.density = j.value("density", 1000.0f);
        mat.dispersion = j.value("dispersion", 0);
        mat.strength = j.value("strength", 0.0f);

        // Thermal properties
        if (j.contains("thermal_conductivity")) mat.thermalConductivity = j["thermal_conductivity"].get<core::f32>();
        else if (j.contains("thermalConductivity")) mat.thermalConductivity = j["thermalConductivity"].get<core::f32>();

        if (j.contains("specific_heat")) mat.specificHeat = j["specific_heat"].get<core::f32>();
        else if (j.contains("specificHeat")) mat.specificHeat = j["specificHeat"].get<core::f32>();
        else if (j.contains("heat_capacity")) mat.specificHeat = j["heat_capacity"].get<core::f32>();

        if (j.contains("melt_point")) mat.meltPoint = j["melt_point"].get<core::f32>();
        else if (j.contains("meltPoint")) mat.meltPoint = j["meltPoint"].get<core::f32>();

        if (j.contains("boil_point")) mat.boilPoint = j["boil_point"].get<core::f32>();
        else if (j.contains("boilPoint")) mat.boilPoint = j["boilPoint"].get<core::f32>();

        if (j.contains("freeze_point")) mat.freezePoint = j["freeze_point"].get<core::f32>();
        else if (j.contains("freezePoint")) mat.freezePoint = j["freezePoint"].get<core::f32>();

        // Phase targets
        if (j.contains("melt_target")) mat.meltTargetName = j["melt_target"].get<std::string>();
        else if (j.contains("meltTarget")) mat.meltTargetName = j["meltTarget"].get<std::string>();

        if (j.contains("boil_target")) mat.boilTargetName = j["boil_target"].get<std::string>();
        else if (j.contains("boilTarget")) mat.boilTargetName = j["boilTarget"].get<std::string>();

        if (j.contains("freeze_target")) mat.freezeTargetName = j["freeze_target"].get<std::string>();
        else if (j.contains("freezeTarget")) mat.freezeTargetName = j["freezeTarget"].get<std::string>();

        // Flammability & Lifetime
        mat.flammability = j.value("flammability", 0.0f);
        if (j.contains("ignition_temp")) mat.ignitionTemp = j["ignition_temp"].get<core::f32>();
        else if (j.contains("ignitionTemp")) mat.ignitionTemp = j["ignitionTemp"].get<core::f32>();

        mat.lifetime = static_cast<core::u8>(j.value("lifetime", 0));

        RegisterMaterial(mat);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("MaterialRegistry", "JSON parse error in ", filePath, ": ", e.what());
        return false;
    }
}

bool MaterialRegistry::LoadFromDirectory(const std::string& directoryPath) {
    if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
        LOG_ERROR("MaterialRegistry", "Directory does not exist: ", directoryPath);
        return false;
    }

    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            paths.push_back(entry.path());
        }
    }

    // Sort to ensure air.json is processed first
    std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b) {
        if (a.stem() == "air") return true;
        if (b.stem() == "air") return false;
        return a.stem() < b.stem();
    });

    for (const auto& p : paths) {
        LoadFromFile(p.string());
    }

    ResolvePhaseTargets();
    LOG_INFO("MaterialRegistry", "Successfully loaded ", m_materials.size(), " materials from ", directoryPath);
    return true;
}

void MaterialRegistry::ResolvePhaseTargets() {
    for (auto& mat : m_materials) {
        if (!mat.meltTargetName.empty()) {
            mat.meltMaterialId = FindByName(mat.meltTargetName);
        }
        if (!mat.boilTargetName.empty()) {
            mat.boilMaterialId = FindByName(mat.boilTargetName);
        }
        if (!mat.freezeTargetName.empty()) {
            mat.freezeMaterialId = FindByName(mat.freezeTargetName);
        }
    }
}

} // namespace sandbox::sim
