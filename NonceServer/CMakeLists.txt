# NonceServer: 预承诺协议服务端（VC++ / MSVC，C++20，Windows 原生自包含）。
#
# 无第三方依赖：密码学走 Windows CNG（bcrypt），TLS 走 Schannel（secur32/crypt32），
# HTTP/1.1 基于 Winsock 自研，JSON 为仓库内自带最小实现。

if (NOT WIN32)
  message(FATAL_ERROR "NonceServer 依赖 Windows CNG / Schannel / Winsock，仅支持 Windows。")
endif()

set(NONCESERVER_CORE_SOURCES
  "src/common/Status.cpp"
  "src/common/Hex.cpp"
  "src/common/Base64.cpp"
  "src/common/Time.cpp"
  "src/common/Uuid.cpp"
  "src/json/Json.cpp"
  "src/crypto/Sha256.cpp"
  "src/crypto/Random.cpp"
  "src/crypto/Ecdsa.cpp"
  "src/domain/Commitment.cpp"
  "src/canonical/CanonicalHash.cpp"
  "src/storage/FileCommitmentStore.cpp"
  "src/clock/Clock.cpp"
  "src/log/Logger.cpp"
  "src/config/ServerConfig.cpp"
  "src/pulse/DrandQuicknetSource.cpp"
  "src/pulse/NistBeaconSource.cpp"
  "src/pulse/PulseSourceRegistry.cpp"
  "src/service/CommitmentService.cpp"
  "src/http/HttpTypes.cpp"
  "src/http/Router.cpp"
  "src/http/HttpServer.cpp"
  "src/http/ApiController.cpp"
  "src/tls/SchannelTls.cpp"
)

add_library(nonceserver_core STATIC ${NONCESERVER_CORE_SOURCES})
target_include_directories(nonceserver_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
set_target_properties(nonceserver_core PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)

# Windows 系统库：CNG + Schannel + 证书存储 + Winsock。
target_link_libraries(nonceserver_core PUBLIC bcrypt crypt32 secur32 ws2_32)

add_executable(NonceServer "src/main.cpp")
target_link_libraries(NonceServer PRIVATE nonceserver_core)
set_target_properties(NonceServer PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF)

if (MSVC)
  foreach(target nonceserver_core NonceServer)
    target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
    target_compile_definitions(${target} PRIVATE _WIN32_WINNT=0x0A00 _CRT_SECURE_NO_WARNINGS)
  endforeach()
endif()

# TODO: 添加测试目标（哈希规范向量、链完整性、幂等、TLS 握手）。
