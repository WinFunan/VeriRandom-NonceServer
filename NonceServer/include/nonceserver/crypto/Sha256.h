// Sha256.h: 基于 Windows CNG 的 SHA-256。
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nonceserver {

using Sha256Digest = std::array<uint8_t, 32>;

Sha256Digest Sha256(const uint8_t* data, std::size_t length);
Sha256Digest Sha256(std::string_view data);
Sha256Digest Sha256(const std::vector<uint8_t>& data);

// 流式哈希器。构造失败（CNG 不可用）时抛出 std::runtime_error。
class Sha256Hasher {
public:
    Sha256Hasher();
    ~Sha256Hasher();
    Sha256Hasher(const Sha256Hasher&) = delete;
    Sha256Hasher& operator=(const Sha256Hasher&) = delete;

    void Update(const uint8_t* data, std::size_t length);
    void Update(std::string_view data);
    Sha256Digest Final();

private:
    void* handle_ = nullptr;  // BCRYPT_HASH_HANDLE
};

}  // namespace nonceserver
