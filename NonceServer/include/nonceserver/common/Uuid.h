// Uuid.h: 生成 RFC 4122 version 4 UUID（基于 CSPRNG）。
#pragma once

#include <string>

namespace nonceserver {

std::string GenerateUuidV4();

}  // namespace nonceserver
