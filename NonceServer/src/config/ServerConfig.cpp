#include "nonceserver/config/ServerConfig.h"

#include "nonceserver/log/Logger.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace nonceserver {

namespace {

std::string GetString(const Json& json, std::string_view key, const std::string& fallback) {
    const Json* value = json.find(key);
    return value && value->isString() ? value->asString() : fallback;
}

int GetInt(const Json& json, std::string_view key, int fallback) {
    const Json* value = json.find(key);
    return value ? static_cast<int>(value->asInt64(fallback)) : fallback;
}

bool GetBool(const Json& json, std::string_view key, bool fallback) {
    const Json* value = json.find(key);
    return value && value->isBool() ? value->asBool() : fallback;
}

}  // namespace

Json TlsConfig::ToJson() const {
    Json json(Json::Object{});
    json["enabled"] = enabled;
    json["certificateStoreLocation"] = certificateStoreLocation;
    json["certificateStoreName"] = certificateStoreName;
    json["certificateThumbprint"] = certificateThumbprint;
    json["certificateSubject"] = certificateSubject;
    return json;
}

TlsConfig TlsConfig::FromJson(const Json& json) {
    TlsConfig tls;
    tls.enabled = GetBool(json, "enabled", tls.enabled);
    tls.certificateStoreLocation =
        GetString(json, "certificateStoreLocation", tls.certificateStoreLocation);
    tls.certificateStoreName = GetString(json, "certificateStoreName", tls.certificateStoreName);
    tls.certificateThumbprint = GetString(json, "certificateThumbprint", tls.certificateThumbprint);
    tls.certificateSubject = GetString(json, "certificateSubject", tls.certificateSubject);
    return tls;
}

ServerConfig ServerConfig::Default() { return ServerConfig{}; }

Result<ServerConfig> ServerConfig::LoadFromFile(const std::string& path) {
    ServerConfig config = Default();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        LogWarn("config file not found, using defaults: " + path);
        return config;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return Status::Error(ErrorCode::StorageFailure, "cannot open config file: " + path);
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();

    std::string error;
    const auto parsed = Json::parse(buffer.str(), &error);
    if (!parsed || !parsed->isObject()) {
        return Status::Error(ErrorCode::InvalidArgument,
                             "invalid config JSON (" + error + "): " + path);
    }
    const Json& json = *parsed;

    config.host = GetString(json, "host", config.host);
    config.port = static_cast<uint16_t>(GetInt(json, "port", config.port));
    config.storageDirectory = GetString(json, "storageDirectory", config.storageDirectory);
    config.serverKeyFile = GetString(json, "serverKeyFile", config.serverKeyFile);
    config.defaultPulseSource = GetString(json, "defaultPulseSource", config.defaultPulseSource);
    config.defaultPageSize = GetInt(json, "defaultPageSize", config.defaultPageSize);
    config.maxPageSize = GetInt(json, "maxPageSize", config.maxPageSize);
    if (const Json* tls = json.find("tls")) config.tls = TlsConfig::FromJson(*tls);

    return config;
}

Status ServerConfig::Validate() const {
    if (port == 0) {
        return Status::Error(ErrorCode::InvalidArgument, "port must be non-zero");
    }
    if (defaultPageSize <= 0 || maxPageSize <= 0 || defaultPageSize > maxPageSize) {
        return Status::Error(ErrorCode::InvalidArgument, "invalid page size configuration");
    }
    if (storageDirectory.empty()) {
        return Status::Error(ErrorCode::InvalidArgument, "storageDirectory must not be empty");
    }
    if (tls.enabled) {
        if (tls.certificateThumbprint.empty() && tls.certificateSubject.empty()) {
            return Status::Error(
                ErrorCode::InvalidArgument,
                "TLS enabled but neither certificateThumbprint nor certificateSubject configured");
        }
        if (tls.certificateStoreLocation != "LocalMachine" &&
            tls.certificateStoreLocation != "CurrentUser") {
            return Status::Error(ErrorCode::InvalidArgument,
                                 "certificateStoreLocation must be LocalMachine or CurrentUser");
        }
    }
    return Status::Ok();
}

Json ServerConfig::ToJson() const {
    Json json(Json::Object{});
    json["host"] = host;
    json["port"] = port;
    json["storageDirectory"] = storageDirectory;
    json["serverKeyFile"] = serverKeyFile;
    json["defaultPulseSource"] = defaultPulseSource;
    json["defaultPageSize"] = defaultPageSize;
    json["maxPageSize"] = maxPageSize;
    json["tls"] = tls.ToJson();
    return json;
}

}  // namespace nonceserver
