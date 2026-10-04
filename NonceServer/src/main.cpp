// VeriRandom-NonceServer: 预承诺协议服务端入口。
// 规范见仓库根目录 dual-draw-server-integration.md。

#include "nonceserver/Version.h"
#include "nonceserver/clock/Clock.h"
#include "nonceserver/config/ServerConfig.h"
#include "nonceserver/crypto/Ecdsa.h"
#include "nonceserver/http/ApiController.h"
#include "nonceserver/http/HttpServer.h"
#include "nonceserver/http/Router.h"
#include "nonceserver/log/Logger.h"
#include "nonceserver/pulse/DrandQuicknetSource.h"
#include "nonceserver/pulse/NistBeaconSource.h"
#include "nonceserver/pulse/PulseSourceRegistry.h"
#include "nonceserver/service/CommitmentService.h"
#include "nonceserver/storage/FileCommitmentStore.h"
#include "nonceserver/tls/TlsFactory.h"

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

std::atomic<nonceserver::HttpServer*> g_server{nullptr};

BOOL WINAPI ConsoleHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) {
        if (auto* server = g_server.load()) server->Stop();
        return TRUE;
    }
    return FALSE;
}

int Fail(const std::string& message) {
    nonceserver::LogError(message);
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace nonceserver;

    LogInfo(std::string(kServerName) + " " + kServerVersion + " (protocol v" +
            std::to_string(kSupportedProtocolVersion) + ")");

    const std::string configPath = argc > 1 ? argv[1] : "server.config.json";
    const auto configResult = ServerConfig::LoadFromFile(configPath);
    if (!configResult.ok()) return Fail("failed to load config: " + configResult.status().message());
    ServerConfig config = configResult.value();
    if (const Status valid = config.Validate(); !valid.ok()) {
        return Fail("invalid config: " + valid.message());
    }

    // 只可追加的承诺链存储。
    StoreOptions storeOptions;
    storeOptions.directory = config.storageDirectory;
    FileCommitmentStore store(std::move(storeOptions));
    if (const Status status = store.Initialize(); !status.ok()) {
        return Fail("failed to initialize storage: " + status.message());
    }

    // 服务端签名密钥（独立于 TLS 证书，规范 §11）。
    bool created = false;
    auto keyResult = LoadOrCreateSigningKey(config.serverKeyFile, &created);
    if (!keyResult.ok()) return Fail("failed to load signing key: " + keyResult.status().message());
    if (created) LogInfo("generated new signing key: " + config.serverKeyFile);
    EcdsaP256Key signingKey = std::move(keyResult.value());
    LogInfo("server key id: " + signingKey.KeyId());

    SystemClock clock;
    PulseSourceRegistry pulseSources;
    pulseSources.Register(std::make_shared<DrandQuicknetSource>());
    pulseSources.Register(std::make_shared<NistBeaconSource>());
    if (!pulseSources.Contains(config.defaultPulseSource)) {
        return Fail("defaultPulseSource is not registered: " + config.defaultPulseSource);
    }

    CommitmentService service(config, store, clock, pulseSources, std::move(signingKey));

    std::shared_ptr<ITlsContext> tlsContext;
    if (config.tls.enabled) {
        tlsContext = CreateSchannelTlsContext(config.tls);
        if (const Status status = tlsContext->Initialize(); !status.ok()) {
            return Fail("failed to initialize TLS: " + status.message());
        }
    }

    Router router;
    ApiController controller(service);
    controller.RegisterRoutes(router);

    HttpServer server(config, router, tlsContext);
    g_server.store(&server);
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    const Status started = server.Start();
    g_server.store(nullptr);
    if (!started.ok()) return Fail("server error: " + started.message());
    return 0;
}
