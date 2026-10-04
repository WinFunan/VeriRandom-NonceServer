// Random.h: 基于 Windows CNG 的密码学安全随机数（CSPRNG）。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nonceserver {

// 失败时抛出 std::runtime_error（CSPRNG 不可用属于致命错误）。
void SecureRandomBytes(void* buffer, std::size_t length);
std::vector<uint8_t> SecureRandomBytes(std::size_t length);

std::string SecureRandomHex(std::size_t length);
std::string SecureRandomBase64Url(std::size_t length);

}  // namespace nonceserver
