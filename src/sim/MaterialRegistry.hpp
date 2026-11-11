#pragma once

#include "Material.hpp"
#include <vector>
#include <unordered_map>
#include <string>

namespace sandbox::sim {

class MaterialRegistry {
private:
    std::vector<Material> m_materials;
    std::unordered_map<std::string, MaterialId> m_nameToId;
    Material m_emptyMaterial;

    MaterialRegistry();

public:
    static MaterialRegistry& Instance();

    // Singleton semantics: delete copy & move
    MaterialRegistry(const MaterialRegistry&) = delete;
    MaterialRegistry& operator=(const MaterialRegistry&) = delete;
    MaterialRegistry(MaterialRegistry&&) = delete;
    MaterialRegistry& operator=(MaterialRegistry&&) = delete;

    // Loading & Registration
    bool LoadFromDirectory(const std::string& directoryPath);
    bool LoadFromFile(const std::string& filePath);
    void ResolvePhaseTargets();

    MaterialId RegisterMaterial(Material mat);
    
    // Accessors
    [[nodiscard]] const Material& Get(MaterialId id) const noexcept;
    [[nodiscard]] MaterialId FindByName(const std::string& name) const noexcept;
    [[nodiscard]] size_t Count() const noexcept;
    [[nodiscard]] const std::vector<Material>& GetAll() const noexcept;

    // Resetting for testing
    void Clear();
};

} // namespace sandbox::sim
