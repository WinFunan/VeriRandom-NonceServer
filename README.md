# VeriRandom-NonceServer

一个用于向 VeriRandom 客户端提供随机数的服务端程序，实现**双抽取预承诺协议**
（[`dual-draw-server-integration.md`](./dual-draw-server-integration.md)，协议版本 `1`）。

服务端在抽取前签发可审计的**承诺**：`nonce` + 指派脉冲 + 输入摘要 + 时间，并写入只可追加的哈希链。
客户端对承诺摘要申请 TSA，从而即便服务端事后否认或归档被改写，也能证明「该承诺在指派脉冲发布之前已存在」。

> ⚠️ **信任边界**：本协议**只在抽取者与服务端运营者分离时有意义**（如教师抽取、管理员跑服务端）。
> 它**不**证明名单真实/完整、被抽者知情、程序二进制未被替换，也不防服务端与抽取者合谋。
> 完整说明见规范 §0 与 §13.4。

## 状态

当前为**基础框架（v0.1.0）**：核心「签发 → 查询 → 披露消费」链路可用。
脉冲取回/验签、链上包含证明、对账、全量归档自证与自动化测试尚待补全，
详见 [`ChangeLog.md`](./ChangeLog.md) 与 [`Agents.md`](./Agents.md) §8。

## 特性

- **VC++ / MSVC / C++20**，**Windows 原生、零第三方依赖**：
  - 密码学：Windows CNG（`bcrypt`）——SHA-256、CSPRNG、ECDSA P-256 + SHA-256（DER）。
  - TLS：Windows **Schannel**，证书取自 **Windows 证书存储**（默认 `LocalMachine\My`），支持自动续期重载。
  - HTTP：基于 Winsock 的最小 HTTP/1.1 服务端。
  - JSON：仓库自带最小实现。
- **只可追加**的承诺链存储（JSONL + `fsync`，启动时校验链完整性），消费以独立披露记录追加。
- **先落盘再响应**、`clientId`+`clientNonce` 幂等、响应独立签名。
- `drand-quicknet` 确定性指派（保证至少 1 个完整周期）；`nist-beacon-v2` 预留骨架。

## 目录结构

```
dual-draw-server-integration.md   权威协议规范
CMakeLists.txt / CMakePresets.json 顶层构建与预设
NonceServer/
  include/nonceserver/            公共接口（common/json/crypto/domain/canonical/storage/clock/pulse/config/log/service/http/net/tls）
  src/                            实现
server.config.example.json        示例配置
Agents.md                         代理/开发者指南与约束清单
ChangeLog.md                      变更日志
```

## 构建

**前置**：Visual Studio 2022（Desktop C++）、Windows 10+ SDK、CMake ≥ 3.20。

```powershell
# 在 VS 的 Developer PowerShell 中（或先执行 vcvars64.bat）
cmake --preset x64-debug
cmake --build --preset x64-debug
```

产物：`out/build/x64-debug/NonceServer/NonceServer.exe`。发布构建用 `x64-release`。

## 运行

```powershell
# 复制示例配置并按需修改（尤其是 TLS 证书选择）
Copy-Item server.config.example.json server.config.json
.\out\build\x64-debug\NonceServer\NonceServer.exe server.config.json
```

服务端签名密钥在 `serverKeyFile` 不存在时自动生成（base64url 的 CNG 私钥 BLOB）。

### TLS 证书

TLS 默认开启，证书从 Windows 证书存储获取（由外部自动续期机器人维护）：

- `LocalMachine\My`（默认）或 `CurrentUser\My`；
- 按 `certificateThumbprint`（SHA-1 十六进制，允许空格/冒号）或 `certificateSubject` 查找，**要求含私钥**；
- 每次连接按指纹检测证书变化，自动续期后无需重启即可重建凭据。

本地开发可在配置中设 `tls.enabled=false`（会打印明文警告，**不得用于生产**）。

## 端点

| 方法 | 路径 | 说明 |
|---|---|---|
| POST | `/v1/commitments` | 签发承诺 |
| GET | `/v1/commitments/{commitmentId}` | 记录原文（含 `recordHash`/`prevHash`/`responseSignature`） |
| GET | `/v1/commitments/issued?fromSequence=&limit=` | 分页列出全部已签发承诺（含未消费）+ 链头 |
| GET | `/v1/chain/head` | 当前链头 |
| GET | `/v1/chain/proof/{sequence}` | 链上包含证明（未实现，501） |
| POST | `/v1/draws` | 消费披露 |

审计/验证约定见规范 §13；完整性必须来自全量、只可追加的哈希链。

## 文档

- 协议规范：[`dual-draw-server-integration.md`](./dual-draw-server-integration.md)
- 开发者/代理指南：[`Agents.md`](./Agents.md)
- 变更记录：[`ChangeLog.md`](./ChangeLog.md)

## 许可

见 [`LICENSE`](./LICENSE)。
