# Agents.md

面向在本仓库上工作的 AI 代理 / 开发者的操作指南。**动手前请先读完本节与权威规范。**

---

## 1. 项目定位

`VeriRandom-NonceServer` 是一个用 **VC++（MSVC，C++20）** 编写的预承诺（pre-commitment）随机数服务端，
为 VeriRandom 客户端在抽取前签发可审计的承诺记录，使「用了某个承诺却不说」在外部可被发现，
并保证抽取输入在指派脉冲发布后无法被事后修改。

- **权威规范**：[`dual-draw-server-integration.md`](./dual-draw-server-integration.md)（协议版本 `1`）。
  **规范与代码冲突时，以规范为准**，并同步更新两者。
- 信任边界、能证明什么/不能证明什么，规范 §0 / §13.4 已明确；**不要**在文档或代码注释里夸大安全性。

---

## 2. 不可违背的约束（改代码前必读）

1. **只可追加**：承诺链存储层不得出现 UPDATE / DELETE 路径。`consumed` 通过追加「消费披露」记录实现。
2. **先落盘再响应**：签发必须先写入并 `fsync`（`_commit`），失败则返回错误、**不得签发**（规范 §3）。
3. **幂等**：同一 `clientId` + `clientNonce` 必须返回同一条承诺，不得新签（规范 §3）。
4. **响应必签名**：ECDSA P-256 + SHA-256，DER 编码，覆盖规范 §5 的响应投影。
5. **nonce 必须 CSPRNG ≥256 位**，不得用计数器/时间戳/任何可预测值，也不得掺入客户端可控状态（规范 §11）。
6. **指派脉冲点名到具体脉冲**，且 `publishedAt` 严格晚于 `issuedAtUtc` 并至少留 1 个完整周期；
   签发后**不得改派**、不得静默换源（规范 §4.1、§6.3、§14-4/5）。
7. **仅 HTTPS/TLS**：默认启用 TLS，明文仅用于本地开发并会打印警告（规范 §11）。
   TLS 证书来自 **Windows 证书存储**（默认 `LocalMachine\My`），由外部自动续期机器人维护。
8. **日志**：不得记录名单内容（只有摘要）；除 `nonce` 外不得写敏感信息（规范 §11）。
9. **规范哈希集中在** [`CanonicalHash.cpp`](./NonceServer/src/canonical/CanonicalHash.cpp)——跨端逐字节一致，
   任何改动都必须与客户端同步，否则两侧哈希不一致。

---

## 3. 目录结构

```
dual-draw-server-integration.md   权威协议规范
CMakeLists.txt / CMakePresets.json 顶层构建
NonceServer/
  CMakeLists.txt                   nonceserver_core 静态库 + NonceServer 可执行
  include/nonceserver/             公共头（对外接口）
    common/    Result(Status/Result)、Hex、Base64、Time(RFC3339)、Uuid
    json/      自包含最小 JSON（解析/序列化，支持 int64/uint64）
    crypto/    Sha256、Random(CSPRNG)、Ecdsa(P-256 DER)
    domain/    Commitment 领域模型（记录/请求/响应/披露）
    canonical/ 规范哈希（inputDigest/commitmentHash/recordHash/响应投影）
    storage/   ICommitmentStore 抽象
    clock/     IClock 抽象
    pulse/     IPulseSource / drand / NIST / 注册表
    config/    ServerConfig（含 TLS 配置）
    log/       Logger
    service/   CommitmentService 业务编排
    http/      HttpTypes / Router / HttpServer / ApiController
    net/       ITransport 字节流抽象
    tls/       ITlsContext + TlsFactory
    Version.h
  src/                             实现（镜像 include 结构）+ tls/SchannelTls.cpp
server.config.example.json         示例配置
```

**依赖方向**：`http → service → {storage, clock, pulse, crypto, canonical, domain} → common`。
底层不得反向依赖上层；新增功能优先通过现有抽象注入，而不是直接跨层调用。

---

## 4. 构建与运行

**前置**：Visual Studio 2022（Desktop C++ 工作负载）、Windows 10+ SDK、CMake ≥ 3.20。
无第三方依赖：密码学走 Windows CNG（`bcrypt`），TLS 走 Schannel（`secur32`/`crypt32`），
HTTP 基于 Winsock，JSON 为仓库自带实现。

```powershell
# 在 VS 的 Developer PowerShell 中（或先执行 vcvars64.bat）
cmake --preset x64-debug
cmake --build --preset x64-debug

# 运行（配置路径可省略，默认 ./server.config.json）
.\out\build\x64-debug\NonceServer\NonceServer.exe server.config.example.json
```

无 CMake 时的直接编译（仅用于快速验证，正式构建仍以 CMake 为准）：使用 `cl.exe`
编译 `NonceServer/src` 下的全部 `.cpp`（`/I NonceServer\include`），再链接
`bcrypt.lib crypt32.lib secur32.lib ws2_32.lib`。新增源文件时不要忘记加入
`NonceServer/CMakeLists.txt` 的 `NONCESERVER_CORE_SOURCES` 列表。

> 注：`CMakePresets.json` 的 `windows-base` 使用 Ninja。若 CMake 在含空格的 VS 路径上把编译器
> 解析为失效的 8.3 短名，请在 Developer 环境（vcvars）中构建，或改用 Visual Studio 生成器。

---

## 5. 配置（`server.config.json`）

见 [`server.config.example.json`](./server.config.example.json)。关键项：

| 键 | 说明 |
|---|---|
| `host` / `port` | 监听地址与端口（TLS 默认端口 8443） |
| `storageDirectory` | 承诺链存储目录（`commitments.log` + `consumed.log`） |
| `serverKeyFile` | 服务端签名私钥（base64url 的 CNG 私钥 BLOB）；不存在则自动生成 |
| `defaultPulseSource` | `drand-quicknet`（默认）或 `nist-beacon-v2` |
| `tls.enabled` | 默认 `true`；仅本地开发可关 |
| `tls.certificateStoreLocation` | `LocalMachine` 或 `CurrentUser` |
| `tls.certificateStoreName` | 默认 `MY` |
| `tls.certificateThumbprint` | SHA-1 指纹（十六进制，允许空格/冒号）；优先匹配 |
| `tls.certificateSubject` | 指纹为空时按主题查找（要求含私钥） |

TLS 凭据在**每次连接**时检查证书指纹，因此外部自动续期替换证书后会自动重建 Schannel 凭据。

---

## 6. 端点一览（规范 §4）

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/v1/commitments` | 签发承诺（先落盘再签名响应） |
| GET | `/v1/commitments/{commitmentId}` | 返回记录原文（含 `recordHash`/`prevHash`/`responseSignature`） |
| GET | `/v1/commitments/issued?fromSequence=&limit=` | 分页列出**全部**已签发承诺（含未消费）+ 链头；**全量存档入口** |
| GET | `/v1/chain/head` | 当前链头 `{sequence, recordHash}` |
| GET | `/v1/chain/proof/{sequence}` | 链上包含证明（**未实现**，返回 501） |
| POST | `/v1/draws` | 消费披露（诚实客户端主动上报，非权威声明） |

规范 §13.6 要求完整性只能来自**全量、只可追加、哈希链**的存档：审计方应拉取全量记录并重算 `recordHash`
链，再在本地按 `clientId` 过滤；**按身份查询只作便利接口，不得作为完整性依据**。

---

## 7. 扩展指南

- **新增端点**：在 `ApiController::RegisterRoutes` 注册路由，处理器解析 JSON（`Json::parse`）→ 调用
  `CommitmentService` → 用 `HttpResponse::Json` 输出；错误经 `MapStatus` 映射 HTTP 状态码。
- **新增脉冲源**：实现 `IPulseSource`（`Id/Period/SelectUnpublishedPulse/Resolve/Verify`），
  在 `main.cpp` 注册到 `PulseSourceRegistry`。`SelectUnpublishedPulse` 必须确定性地给出身份与 `publishedAt`。
- **替换存储后端**：实现 `ICommitmentStore`，保持 `Append`（在锁内分配 `sequence`/`prevHash`，
  调用 `finalize` 计算哈希，落盘后返回）与幂等语义。
- **替换算法库**：`crypto/*` 为唯一密码学边界；换实现时保持接口不变，并补规范测试向量。

---

## 8. 已知缺口 / TODO（按规范优先级）

- [ ] `drand` 脉冲的 `Resolve`（HTTPS 取回）与 **BLS12-381 验签**（规范 §6.1/§9）尚未实现。
- [ ] `nist-beacon-v2` 的取脉冲与证书 Subject 约束校验尚未实现（规范 §6.2）。
- [ ] `GET /v1/chain/proof/{sequence}` 链上包含证明未实现（规范 §4.5）。
- [ ] 对账流程（§8）与未消费承诺的书面结论流程未实现。
- [ ] 全量存档导出 / 自证归档（规范 §7、§13.6）与独立验证工具未实现。
- [ ] `pulse-missing` / `pulse-invalid` 状态机与超时窗口未落地（规范 §6.3、§10）。
- [ ] NTP 时钟可信度与偏差记录未实现（规范 §10）。
- [ ] 自动化测试（哈希向量、链完整性、幂等、TLS 握手）尚缺。
- [ ] TLS 目前使用 `SCHANNEL_CRED` 默认协议；如需 TLS 1.3 可评估迁移到 `SCHANNEL_CREDENTIALS`。

---

## 9. 改动检查清单

提交任何改动前，请确认：

- [ ] 与 [`dual-draw-server-integration.md`](./dual-draw-server-integration.md) 一致；若有偏差，先改规范。
- [ ] 未破坏「只可追加 / 先落盘再响应 / 幂等 / 不改派」不变式。
- [ ] 规范哈希若变更，已同步客户端并更新 `CanonicalHash.*` 的约定注释。
- [ ] 未新增名单内容或敏感信息日志。
- [ ] **已在 [`ChangeLog.md`](./ChangeLog.md) 记录本次变更**。
- [ ] 通过构建与关键路径自测（可用 `server.config.example.json` 起服务并跑通签发→查询→披露）。
