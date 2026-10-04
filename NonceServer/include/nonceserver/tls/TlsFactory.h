// TlsFactory.h: 创建基于 Windows Schannel + 证书存储的 TLS 上下文。
#pragma once

#include "nonceserver/config/ServerConfig.h"
#include "nonceserver/tls/ITlsContext.h"

#include <memory>

namespace nonceserver {

// 证书从 Windows 证书存储（默认 LocalMachine\My）按指纹或主题查找；
// 由外部自动续期机器人维护时，WrapServerSocket 会在每次连接时刷新。
std::shared_ptr<ITlsContext> CreateSchannelTlsContext(const TlsConfig& config);

}  // namespace nonceserver
