// Time.h: 规范时间处理。协议要求 RFC3339 UTC、毫秒精度、固定 3 位小数。
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace nonceserver {

// 统一时间类型：system_clock 的毫秒精度时间点。
using UtcTime = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

// 例如 2026-10-04T04:59:30.123Z
std::string FormatRfc3339(UtcTime time);

// 解析 RFC3339 UTC（接受小数秒 0~多位数，按毫秒截断）。失败返回 std::nullopt。
std::optional<UtcTime> ParseRfc3339(std::string_view text);

}  // namespace nonceserver
