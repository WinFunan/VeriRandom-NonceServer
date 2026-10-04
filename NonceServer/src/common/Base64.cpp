#include "nonceserver/common/Base64.h"

namespace nonceserver {

namespace {
constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int DecodeChar(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-' || c == '+') return 62;
    if (c == '_' || c == '/') return 63;
    return -1;
}
}  // namespace

std::string Base64UrlEncode(const uint8_t* data, size_t length) {
    std::string out;
    out.reserve(((length + 2) / 3) * 4);

    size_t i = 0;
    for (; i + 3 <= length; i += 3) {
        const uint32_t chunk = (static_cast<uint32_t>(data[i]) << 16) |
                               (static_cast<uint32_t>(data[i + 1]) << 8) |
                               static_cast<uint32_t>(data[i + 2]);
        out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
        out.push_back(kAlphabet[chunk & 0x3F]);
    }

    const size_t remaining = length - i;
    if (remaining == 1) {
        const uint32_t chunk = static_cast<uint32_t>(data[i]) << 16;
        out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
    } else if (remaining == 2) {
        const uint32_t chunk = (static_cast<uint32_t>(data[i]) << 16) |
                               (static_cast<uint32_t>(data[i + 1]) << 8);
        out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
        out.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
    }
    return out;  // base64url 不填充
}

std::string Base64UrlEncode(const std::vector<uint8_t>& data) {
    return Base64UrlEncode(data.data(), data.size());
}

std::optional<std::vector<uint8_t>> Base64UrlDecode(std::string_view text) {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char c : text) {
        if (c == '=' || c == '\r' || c == '\n') continue;
        cleaned.push_back(c);
    }

    // base64url 去掉填充后，长度模 4 只能为 0/2/3。
    if (cleaned.size() % 4 == 1) return std::nullopt;

    std::vector<uint8_t> out;
    out.reserve((cleaned.size() / 4) * 3 + 2);

    uint32_t buffer = 0;
    int bits = 0;
    for (char c : cleaned) {
        const int v = DecodeChar(c);
        if (v < 0) return std::nullopt;
        buffer = (buffer << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

}  // namespace nonceserver
