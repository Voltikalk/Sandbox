#include "Config.hpp"
#include "Log.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

namespace sandbox::core {

bool AppConfig::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_WARN("Config", "Не удалось открыть конфиг: ", filepath, ", используются значения по умолчанию.");
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("window")) {
            const auto& w = j["window"];
            if (w.contains("title")) window.title = w["title"].get<std::string>();
            if (w.contains("width")) window.width = w["width"].get<i32>();
            if (w.contains("height")) window.height = w["height"].get<i32>();
            if (w.contains("vsync")) window.vsync = w["vsync"].get<bool>();
            if (w.contains("resizable")) window.resizable = w["resizable"].get<bool>();
        }

        if (j.contains("sim")) {
            const auto& s = j["sim"];
            if (s.contains("world_width")) sim.worldWidth = s["world_width"].get<i32>();
            if (s.contains("world_height")) sim.worldHeight = s["world_height"].get<i32>();
            if (s.contains("target_tps")) sim.targetTPS = s["target_tps"].get<i32>();
            if (s.contains("max_substeps")) sim.maxSubsteps = s["max_substeps"].get<i32>();
            if (s.contains("gravity")) sim.gravity = s["gravity"].get<f32>();
            if (s.contains("thread_count")) sim.threadCount = s["thread_count"].get<u32>();
        }

        if (j.contains("render")) {
            const auto& r = j["render"];
            if (r.contains("show_chunk_grid")) render.showChunkGrid = r["show_chunk_grid"].get<bool>();
            if (r.contains("show_debug_overlay")) render.showDebugOverlay = r["show_debug_overlay"].get<bool>();
            if (r.contains("enable_glow")) render.enableGlow = r["enable_glow"].get<bool>();
        }

        LOG_INFO("Config", "Конфигурация успешно загружена из ", filepath);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Config", "Ошибка парсинга конфига ", filepath, ": ", e.what());
        return false;
    }
}

bool AppConfig::SaveToFile(const std::string& filepath) const {
    try {
        nlohmann::json j;
        j["window"]["title"] = window.title;
        j["window"]["width"] = window.width;
        j["window"]["height"] = window.height;
        j["window"]["vsync"] = window.vsync;
        j["window"]["resizable"] = window.resizable;

        j["sim"]["world_width"] = sim.worldWidth;
        j["sim"]["world_height"] = sim.worldHeight;
        j["sim"]["target_tps"] = sim.targetTPS;
        j["sim"]["max_substeps"] = sim.maxSubsteps;
        j["sim"]["gravity"] = sim.gravity;
        j["sim"]["thread_count"] = sim.threadCount;

        j["render"]["show_chunk_grid"] = render.showChunkGrid;
        j["render"]["show_debug_overlay"] = render.showDebugOverlay;
        j["render"]["enable_glow"] = render.enableGlow;

        std::ofstream file(filepath);
        if (!file.is_open()) {
            LOG_ERROR("Config", "Не удалось открыть файл для записи: ", filepath);
            return false;
        }

        file << j.dump(4);
        LOG_INFO("Config", "Конфигурация успешно сохранена в ", filepath);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Config", "Исключение при сохранении конфига: ", e.what());
        return false;
    }
}

} // namespace sandbox::core
