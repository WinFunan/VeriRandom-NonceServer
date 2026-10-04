// Transport.h: 字节流抽象，使 HTTP 解析层与 TLS/明文实现解耦。
#pragma once

#include "nonceserver/common/Result.h"

#include <cstddef>
#include <cstdint>

namespace nonceserver {

class ITransport {
public:
    virtual ~ITransport() = default;

    // 返回读取的字节数；0 表示对端已正常关闭连接；失败时返回错误状态。
    virtual Result<std::size_t> Read(uint8_t* buffer, std::size_t length) = 0;

    virtual Status Write(const uint8_t* data, std::size_t length) = 0;
};

}  // namespace nonceserver
