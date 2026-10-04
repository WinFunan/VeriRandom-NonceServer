#include "nonceserver/log/Logger.h"

#include "nonceserver/common/Time.h"

#include <iostream>
#include <mutex>

namespace nonceserver {

namespace {

const char* LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

std::mutex& LogMutex() {
    static std::mutex mutex;
    return mutex;
}

}  // namespace

Logger& Logger::Instance() {
    static Logger logger;
    return logger;
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(LogMutex());
    level_ = level;
}

void Logger::Write(LogLevel level, std::string_view message) {
    std::lock_guard<std::mutex> lock(LogMutex());
    if (static_cast<int>(level) < static_cast<int>(level_)) return;

    const auto now = std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now());
    std::cout << FormatRfc3339(now) << " [" << LevelName(level) << "] " << message << '\n';
    std::cout.flush();
}

void LogDebug(std::string_view message) { Logger::Instance().Write(LogLevel::Debug, message); }
void LogInfo(std::string_view message) { Logger::Instance().Write(LogLevel::Info, message); }
void LogWarn(std::string_view message) { Logger::Instance().Write(LogLevel::Warn, message); }
void LogError(std::string_view message) { Logger::Instance().Write(LogLevel::Error, message); }

}  // namespace nonceserver
