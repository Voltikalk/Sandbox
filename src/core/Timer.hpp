#pragma once

#include "Types.hpp"
#include <chrono>
#include <string_view>
#include <array>
#include <numeric>

namespace sandbox::core {

// Высокоточный измеритель времени
class HighResClock {
public:
    using ClockType = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<ClockType>;

    [[nodiscard]] static TimePoint Now() noexcept {
        return ClockType::now();
    }

    [[nodiscard]] static f64 ElapsedSeconds(TimePoint start, TimePoint end) noexcept {
        return std::chrono::duration<f64>(end - start).count();
    }

    [[nodiscard]] static f64 ElapsedMilliseconds(TimePoint start, TimePoint end) noexcept {
        return std::chrono::duration<f64, std::milli>(end - start).count();
    }

    [[nodiscard]] static i64 ElapsedMicroseconds(TimePoint start, TimePoint end) noexcept {
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
};

// Таймер кадров и расчет сглаженного FPS
class FrameTimer {
private:
    HighResClock::TimePoint m_lastTime;
    HighResClock::TimePoint m_currentTime;
    f64 m_deltaTime{0.0166666667}; // начальное ~60 FPS

    static constexpr size_t HISTORY_SIZE = 60;
    std::array<f64, HISTORY_SIZE> m_frameTimes{};
    size_t m_historyIndex{0};
    f64 m_averageFrameTimeMs{16.6666};
    f64 m_currentFps{60.0};

public:
    FrameTimer() {
        Reset();
    }

    void Reset() noexcept {
        m_lastTime = HighResClock::Now();
        m_currentTime = m_lastTime;
        m_frameTimes.fill(16.6666);
    }

    // Вызывать в начале каждого кадра
    void Tick() noexcept {
        m_currentTime = HighResClock::Now();
        m_deltaTime = HighResClock::ElapsedSeconds(m_lastTime, m_currentTime);
        m_lastTime = m_currentTime;

        // Ограничиваем от резких скачков (например при паузе/дебаге)
        if (m_deltaTime > 0.2) {
            m_deltaTime = 0.2;
        }

        f64 dtMs = m_deltaTime * 1000.0;
        m_frameTimes[m_historyIndex] = dtMs;
        m_historyIndex = (m_historyIndex + 1) % HISTORY_SIZE;

        f64 sum = std::accumulate(m_frameTimes.begin(), m_frameTimes.end(), 0.0);
        m_averageFrameTimeMs = sum / static_cast<f64>(HISTORY_SIZE);
        m_currentFps = (m_averageFrameTimeMs > 0.0001) ? (1000.0 / m_averageFrameTimeMs) : 0.0;
    }

    [[nodiscard]] f64 GetDeltaTimeSeconds() const noexcept { return m_deltaTime; }
    [[nodiscard]] f64 GetAverageFrameTimeMs() const noexcept { return m_averageFrameTimeMs; }
    [[nodiscard]] f64 GetCurrentFps() const noexcept { return m_currentFps; }
};

// RAII таймер для замера выполнения блока кода
class ScopeTimer {
private:
    std::string_view m_name;
    HighResClock::TimePoint m_start;
    f64* m_outDurationMs{nullptr};

public:
    explicit ScopeTimer(std::string_view name, f64* outDurationMs = nullptr)
        : m_name(name), m_start(HighResClock::Now()), m_outDurationMs(outDurationMs) {}

    ~ScopeTimer() {
        f64 elapsed = HighResClock::ElapsedMilliseconds(m_start, HighResClock::Now());
        if (m_outDurationMs) {
            *m_outDurationMs = elapsed;
        }
    }
};

} // namespace sandbox::core

