// Hex.h: 小写十六进制编解码。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nonceserver {

std::string ToHex(const uint8_t* data, size_t length);
std::string ToHex(const std::vector<uint8_t>& data);

// 非法输入（奇数长度或非十六进制字符）返回 std::nullopt。
std::optional<std::vector<uint8_t>> FromHex(std::string_view hex);

}  // namespace nonceserver
