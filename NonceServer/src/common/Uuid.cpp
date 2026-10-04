#include "nonceserver/common/Uuid.h"

#include "nonceserver/crypto/Random.h"

#include <cstdio>

namespace nonceserver {

std::string GenerateUuidV4() {
    uint8_t bytes[16];
    SecureRandomBytes(bytes, sizeof(bytes));

    // version 4 与 variant 位。
    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

    char buffer[37];
    std::snprintf(buffer, sizeof(buffer),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                  bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14],
                  bytes[15]);
    return std::string(buffer);
}

}  // namespace nonceserver
