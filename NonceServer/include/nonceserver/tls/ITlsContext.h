// ITlsContext.h: TLS 上下文抽象。HttpServer 只依赖本接口，不感知 Schannel。
#pragma once

#include "nonceserver/common/Result.h"
#include "nonceserver/net/Transport.h"

#include <cstdint>
#include <memory>

namespace nonceserver {

class ITlsContext {
public:
    virtual ~ITlsContext() = default;

    // 初始化（加载证书/获取凭据）。失败则服务端不应监听。
    virtual Status Initialize() = 0;

    // 为一个已接受的明文 socket 执行 TLS 服务端握手，返回加密传输。
    // 证书自动续期后，本方法内部会检测并重建凭据。
    virtual Result<std::unique_ptr<ITransport>> WrapServerSocket(std::uintptr_t socket) = 0;
};

}  // namespace nonceserver
