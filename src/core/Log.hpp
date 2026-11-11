#pragma once

#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <string_view>

namespace sandbox::core {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
private:
    std::mutex m_mutex;
    LogLevel m_minLevel{LogLevel::Debug};

    Logger() = default;

public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void SetMinLevel(LogLevel level) noexcept {
        m_minLevel = level;
    }

    template <typename... Args>
    void Log(LogLevel level, std::string_view prefix, Args&&... args) {
        if (level < m_minLevel) return;

        std::ostringstream ss;
        (ss << ... << std::forward<Args>(args));

        // Получаем локальное время
        auto now = std::chrono::system_clock::now();
        auto nowTime = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::tm tmBuffer{};
#if defined(_WIN32)
        localtime_s(&tmBuffer, &nowTime);
#else
        localtime_r(&nowTime, &tmBuffer);
#endif

        const char* levelStr = "INFO";
        const char* colorCode = "\033[0m"; // сброс

        switch (level) {
            case LogLevel::Debug:   levelStr = "DEBUG"; colorCode = "\033[36m"; break; // Cyan
            case LogLevel::Info:    levelStr = "INFO "; colorCode = "\033[32m"; break; // Green
            case LogLevel::Warning: levelStr = "WARN "; colorCode = "\033[33m"; break; // Yellow
            case LogLevel::Error:   levelStr = "ERROR"; colorCode = "\033[31;1m"; break; // Bold Red
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << colorCode << "[" << std::put_time(&tmBuffer, "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count() << "]"
                  << " [" << levelStr << "] [" << prefix << "] "
                  << ss.str() << "\033[0m\n";
    }
};

} // namespace sandbox::core

#define LOG_DEBUG(prefix, ...) ::sandbox::core::Logger::Instance().Log(::sandbox::core::LogLevel::Debug, prefix, __VA_ARGS__)
#define LOG_INFO(prefix, ...)  ::sandbox::core::Logger::Instance().Log(::sandbox::core::LogLevel::Info, prefix, __VA_ARGS__)
#define LOG_WARN(prefix, ...)  ::sandbox::core::Logger::Instance().Log(::sandbox::core::LogLevel::Warning, prefix, __VA_ARGS__)
#define LOG_ERROR(prefix, ...) ::sandbox::core::Logger::Instance().Log(::sandbox::core::LogLevel::Error, prefix, __VA_ARGS__)

