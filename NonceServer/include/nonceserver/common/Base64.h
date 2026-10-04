// Base64.h: base64url（RFC 4648 §5，无填充）编解码。
// 规范中的 nonce / signature 均以 base64url 表示。
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nonceserver {

std::string Base64UrlEncode(const uint8_t* data, size_t length);
std::string Base64UrlEncode(const std::vector<uint8_t>& data);

// 同时接受带或不带 '=' 填充的 base64url 与标准 base64。
std::optional<std::vector<uint8_t>> Base64UrlDecode(std::string_view text);

}  // namespace nonceserver
