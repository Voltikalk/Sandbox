#define SDL_MAIN_HANDLED
#include <SDL.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>

#include "core/Types.hpp"
#include "core/Log.hpp"
#include "core/Config.hpp"
#include "core/Timer.hpp"
#include "core/ThreadPool.hpp"

#include "sim/MaterialRegistry.hpp"
#include "sim/World.hpp"
#include "sim/Physics.hpp"
#include "sim/Thermodynamics.hpp"
#include "sim/ReactionSystem.hpp"
#include "sim/ExplosionSystem.hpp"
#include "ui/Renderer.hpp"
#include "ui/Brush.hpp"

#include <iostream>

using namespace sandbox::core;

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#if defined(_WIN32)
    // Установка UTF-8 кодировки для вывода в консоль Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif

    LOG_INFO("App", "Запуск Pixel Sandbox 2D Engine...");

    // 1. Загрузка конфигурации
    AppConfig config;
    config.LoadFromFile("assets/config.json");

    // 2. Инициализация подсистем SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        LOG_ERROR("SDL", "Ошибка инициализации SDL2: ", SDL_GetError());
        return -1;
    }

    u32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (config.window.resizable) {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }

    SDL_Window* window = SDL_CreateWindow(
        config.window.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config.window.width,
        config.window.height,
        windowFlags
    );

    if (!window) {
        LOG_ERROR("SDL", "Не удалось создать окно: ", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    u32 rendererFlags = SDL_RENDERER_ACCELERATED;
    if (config.window.vsync) {
        rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, rendererFlags);
    if (!renderer) {
        LOG_ERROR("SDL", "Не удалось создать рендерер: ", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // 3. Инициализация Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Загрузка системного шрифта с поддержкой кириллицы
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    const char* cyrillicFontPaths[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\tahoma.ttf"
    };
    bool fontLoaded = false;
    for (const char* path : cyrillicFontPaths) {
        FILE* f = nullptr;
#if defined(_WIN32)
        fopen_s(&f, path, "rb");
#else
        f = fopen(path, "rb");
#endif
        if (f) {
            fclose(f);
            io.Fonts->AddFontFromFileTTF(path, 17.0f, &fontConfig, io.Fonts->GetGlyphRangesCyrillic());
            fontLoaded = true;
            break;
        }
    }
    if (!fontLoaded) {
        io.Fonts->AddFontDefault();
    }

    // Настройка темы ImGui (современный темный стиль)
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // 4. Инициализация пула потоков
    ThreadPool threadPool(config.sim.threadCount);
    LOG_INFO("App", "Пул потоков готов к вычислениям.");

    // 5. Инициализация подсистем симуляции и визуализации (Milestone 1 & 2)
    if (!sandbox::sim::MaterialRegistry::Instance().LoadFromDirectory("assets/materials")) {
        sandbox::sim::MaterialRegistry::Instance().LoadFromDirectory("../assets/materials");
    }
    sandbox::sim::ReactionSystem::Instance().LoadFromFile("assets/reactions.json", sandbox::sim::MaterialRegistry::Instance());
    sandbox::sim::World world(config.sim.worldWidth, config.sim.worldHeight);
    sandbox::sim::Physics physics;
    sandbox::sim::Thermodynamics thermodynamics;
    sandbox::ui::WorldRenderer worldRenderer(renderer, config.sim.worldWidth, config.sim.worldHeight);
    sandbox::ui::Brush brush;

    // 6. Игровой цикл с фиксированным шагом симуляции
    FrameTimer frameTimer;
    f64 simAccumulator = 0.0;
    const f64 fixedTimeStep = 1.0 / static_cast<f64>(config.sim.targetTPS);

    u64 simTickCount = 0;
    f64 lastSimTimeMs = 0.0;
    bool isRunning = true;
    bool showDebugOverlay = config.render.showDebugOverlay;

    LOG_INFO("App", "Игровой цикл успешно запущен.");

    while (isRunning) {
        frameTimer.Tick();
        f64 dt = frameTimer.GetDeltaTimeSeconds();

        SDL_Rect viewport{ 0, 0, config.window.width, config.window.height };

        // Обработка событий ввода
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            brush.HandleEvent(event, worldRenderer, viewport, world);

            if (event.type == SDL_QUIT) {
                isRunning = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_F3) {
                    showDebugOverlay = !showDebugOverlay;
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    isRunning = false;
                } else if (event.key.keysym.sym == SDLK_c) {
                    world.Clear();
                    worldRenderer.MarkAllDirty();
                }
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    config.window.width = event.window.data1;
                    config.window.height = event.window.data2;
                }
            }
        }

        // Непрерывное рисование зажатой мышью
        brush.Update(worldRenderer, viewport, world);

        // Накопление времени для фиксированного шага симуляции
        if (!config.sim.paused) {
            simAccumulator += dt * config.sim.timeScale;
            i32 substeps = 0;
            while (simAccumulator >= fixedTimeStep && substeps < config.sim.maxSubsteps) {
                HighResClock::TimePoint simStart = HighResClock::Now();

                physics.Step(world);
                sandbox::sim::ReactionSystem::Instance().Step(world);
                thermodynamics.Step(world);
                sandbox::sim::ExplosionSystem::Instance().Update(world);
                world.AdvanceTick();
                simTickCount++;

                lastSimTimeMs = HighResClock::ElapsedMilliseconds(simStart, HighResClock::Now());
                simAccumulator -= fixedTimeStep;
                substeps++;
            }

            // Защита от спирали смерти (если кадр длился слишком долго)
            if (simAccumulator > fixedTimeStep * config.sim.maxSubsteps) {
                simAccumulator = 0.0;
            }
        }

        // Подготовка интерфейса ImGui
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if (showDebugOverlay) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(0.85f);
            if (ImGui::Begin("Симуляция и отладка (F3)", &showDebugOverlay, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("FPS: %.1f (%.2f ms)", frameTimer.GetCurrentFps(), frameTimer.GetAverageFrameTimeMs());
                ImGui::Text("Целевой TPS: %d (Шаг: %.2f ms)", config.sim.targetTPS, fixedTimeStep * 1000.0);
                ImGui::Text("Время тика симуляции: %.3f ms", lastSimTimeMs);
                ImGui::Text("Номер тика: %llu", simTickCount);
                ImGui::Separator();
                ImGui::Text("Размер окна: %d x %d", config.window.width, config.window.height);
                ImGui::Text("Сетка мира: %d x %d клеток", config.sim.worldWidth, config.sim.worldHeight);
                ImGui::Text("Активных клеток: %u", world.GetTotalActiveCells());
                ImGui::Text("Бодрствующих чанков: %u / %zu", world.GetAwakeChunkCount(), world.GetChunkCount());
                ImGui::Separator();

                ImGui::Checkbox("Пауза симуляции (P)", &config.sim.paused);
                ImGui::SliderFloat("Скорость времени", &config.sim.timeScale, 0.1f, 4.0f, "%.1fx");
                ImGui::Separator();

                // Управление кистью
                int radius = brush.GetRadius();
                if (ImGui::SliderInt("Радиус кисти", &radius, 1, 50)) {
                    brush.SetRadius(radius);
                }

                sandbox::sim::MaterialId curId = brush.GetMaterial();
                const auto& curMat = sandbox::sim::MaterialRegistry::Instance().Get(curId);
                ImGui::Text("Выбранный материал: %s (ID %u)", curMat.name.c_str(), curId);

                // Палитра материалов
                ImGui::Text("Палитра:");
                const auto& allMats = sandbox::sim::MaterialRegistry::Instance().GetAll();
                for (size_t i = 0; i < allMats.size(); ++i) {
                    const auto& m = allMats[i];
                    if (m.name.empty()) continue;

                    ImVec4 btnCol(m.color.r / 255.0f, m.color.g / 255.0f, m.color.b / 255.0f, 1.0f);
                    if (m.id == sandbox::sim::EMPTY_MATERIAL_ID) {
                        btnCol = ImVec4(0.2f, 0.2f, 0.2f, 1.0f); // серый для воздуха
                    }

                    ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
                    float brightness = (m.color.r * 0.299f + m.color.g * 0.587f + m.color.b * 0.114f) / 255.0f;
                    ImVec4 textCol = (brightness > 0.55f) ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, textCol);

                    if (ImGui::Button(m.name.c_str(), ImVec2(75, 24))) {
                        brush.SetMaterial(m.id);
                    }
                    ImGui::PopStyleColor(2);

                    if ((i + 1) % 4 != 0 && (i + 1) < allMats.size()) {
                        ImGui::SameLine();
                    }
                }

                ImGui::Separator();
                if (ImGui::Button("Очистить мир (C)", ImVec2(-1, 28))) {
                    world.Clear();
                    worldRenderer.MarkAllDirty();
                }

                ImGui::Separator();
                ImGui::TextDisabled("ЛКМ - Рисование, ПКМ - Стирание, Колесико - Радиус");
            }
            ImGui::End();
        }

        // Очистка и отрисовка мира и интерфейса
        SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
        SDL_RenderClear(renderer);

        // Потоковый рендеринг сетки мира
        worldRenderer.Render(world, renderer, &viewport);

        // Отрисовка ImGui поверх буфера
        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

        SDL_RenderPresent(renderer);
    }

    LOG_INFO("App", "Завершение работы приложения...");

    // Очистка ресурсов
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    LOG_INFO("App", "Приложение успешно закрыто.");
    return 0;
}
