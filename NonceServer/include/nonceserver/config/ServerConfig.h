// ServerConfig.h: 服务端配置。TLS 为默认开启项（规范 §11：仅 HTTPS/TLS）。
#pragma once

#include "nonceserver/common/Result.h"
#include "nonceserver/json/Json.h"

#include <cstdint>
#include <string>

namespace nonceserver {

// TLS 服务端证书来源：Windows 证书存储（供 Schannel 使用）。
// 证书由外部自动续期机器人维护，因此支持按指纹或主题查找，并在启动/连接时刷新。
struct TlsConfig {
    bool enabled = true;
    std::string certificateStoreLocation = "LocalMachine";  // LocalMachine | CurrentUser
    std::string certificateStoreName = "MY";
    std::string certificateThumbprint;  // SHA-1 十六进制，允许空格/冒号分隔
    std::string certificateSubject;     // 指纹为空时使用

    Json ToJson() const;
    static TlsConfig FromJson(const Json& json);
};

struct ServerConfig {
    std::string host = "0.0.0.0";
    uint16_t port = 8443;
    std::string storageDirectory = "data";
    std::string serverKeyFile = "data/server-key.b64";
    std::string defaultPulseSource = "drand-quicknet";
    int defaultPageSize = 50;
    int maxPageSize = 200;
    TlsConfig tls;

    static ServerConfig Default();

    // 文件不存在时返回默认配置；存在但非法时返回错误。
    static Result<ServerConfig> LoadFromFile(const std::string& path);

    Status Validate() const;
    Json ToJson() const;
};

}  // namespace nonceserver
