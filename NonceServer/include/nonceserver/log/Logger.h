// Logger.h: 极简线程安全日志。绝不记录名单内容；nonce 之外不写敏感信息（规范 §11）。
#pragma once

#include <string>
#include <string_view>

namespace nonceserver {

enum class LogLevel { Debug = 0, Info, Warn, Error };

class Logger {
public:
    static Logger& Instance();

    void SetLevel(LogLevel level);
    LogLevel level() const { return level_; }

    void Write(LogLevel level, std::string_view message);

private:
    Logger() = default;
    LogLevel level_ = LogLevel::Info;
};

void LogDebug(std::string_view message);
void LogInfo(std::string_view message);
void LogWarn(std::string_view message);
void LogError(std::string_view message);

}  // namespace nonceserver
