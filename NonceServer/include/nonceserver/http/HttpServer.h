// HttpServer.h: 基于 Winsock 的最小 HTTP/1.1 服务端。
// 连接可经 ITlsContext 包装为 TLS；不实现 keep-alive（每连接处理一个请求）。
#pragma once

#include "nonceserver/common/Result.h"
#include "nonceserver/config/ServerConfig.h"
#include "nonceserver/http/Router.h"
#include "nonceserver/tls/ITlsContext.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace nonceserver {

class HttpServer {
public:
    HttpServer(ServerConfig config, const Router& router, std::shared_ptr<ITlsContext> tlsContext);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // 阻塞式接受连接，直到 Stop() 被调用。
    Status Start();
    void Stop();

private:
    void AcceptLoop();
    void HandleConnection(std::uintptr_t clientSocket);

    ServerConfig config_;
    const Router& router_;
    std::shared_ptr<ITlsContext> tlsContext_;
    std::atomic<bool> running_{false};
    std::uintptr_t listenSocket_ = static_cast<std::uintptr_t>(~0ull);

    std::mutex workersMutex_;
    std::vector<std::thread> workers_;
};

}  // namespace nonceserver
