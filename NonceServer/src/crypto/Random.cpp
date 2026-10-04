#include "nonceserver/crypto/Random.h"

#include "nonceserver/common/Base64.h"
#include "nonceserver/common/Hex.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <stdexcept>

namespace nonceserver {

void SecureRandomBytes(void* buffer, std::size_t length) {
    auto* cursor = static_cast<uint8_t*>(buffer);
    std::size_t remaining = length;
    while (remaining > 0) {
        const ULONG chunk = static_cast<ULONG>(remaining > 0xFFFFFFFFull ? 0xFFFFFFFFull : remaining);
        const NTSTATUS status =
            BCryptGenRandom(nullptr, cursor, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status < 0) {
            throw std::runtime_error("BCryptGenRandom failed");
        }
        cursor += chunk;
        remaining -= chunk;
    }
}

std::vector<uint8_t> SecureRandomBytes(std::size_t length) {
    std::vector<uint8_t> buffer(length);
    if (length > 0) SecureRandomBytes(buffer.data(), length);
    return buffer;
}

std::string SecureRandomHex(std::size_t length) {
    return ToHex(SecureRandomBytes(length));
}

std::string SecureRandomBase64Url(std::size_t length) {
    return Base64UrlEncode(SecureRandomBytes(length));
}

}  // namespace nonceserver
