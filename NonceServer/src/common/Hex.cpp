#include "nonceserver/common/Hex.h"

namespace nonceserver {

namespace {
constexpr char kHexDigits[] = "0123456789abcdef";

int HexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
}  // namespace

std::string ToHex(const uint8_t* data, size_t length) {
    std::string out;
    out.resize(length * 2);
    for (size_t i = 0; i < length; ++i) {
        out[2 * i] = kHexDigits[data[i] >> 4];
        out[2 * i + 1] = kHexDigits[data[i] & 0x0F];
    }
    return out;
}

std::string ToHex(const std::vector<uint8_t>& data) {
    return ToHex(data.data(), data.size());
}

std::optional<std::vector<uint8_t>> FromHex(std::string_view hex) {
    if (hex.size() % 2 != 0) return std::nullopt;
    std::vector<uint8_t> out(hex.size() / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        const int hi = HexValue(hex[2 * i]);
        const int lo = HexValue(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return out;
}

}  // namespace nonceserver
