# Changelog

本文件记录 `VeriRandom-NonceServer` 的所有重要变更。

格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。
按规范要求，任何代码改动都应在 `[Unreleased]` 下留痕。

## [Unreleased]

### 待办（对齐规范 §8、§13）
- 实现 drand 脉冲 `Resolve`（HTTPS 取回）与 BLS12-381 验签（规范 §6.1、§9）。
- 实现 `nist-beacon-v2` 取脉冲与证书 Subject 约束校验（规范 §6.2）。
- 实现 `GET /v1/chain/proof/{sequence}` 链上包含证明（规范 §4.5）。
- 落地对账流程与未消费承诺的书面结论（规范 §8）。
- 提供全量存档导出 / 自证归档与独立验证工具（规范 §7、§13）。
- `pulse-missing` / `pulse-invalid` 状态机与超时窗口（规范 §6.3、§10）。
- NTP 时钟可信度与偏差记录（规范 §10）。
- 自动化测试：规范哈希向量、链完整性、幂等、TLS 握手。

## [0.1.0] - 2026-10-04

首个基础框架版本。按根目录规范 `dual-draw-server-integration.md`（协议版本 `1`）搭建可编译的分层骨架，
并跑通「签发 → 查询 → 披露消费」核心链路。

### Added

- **构建与工程**
  - CMake 工程：`nonceserver_core` 静态库 + `NonceServer` 可执行；C++20；`/W4 /permissive- /utf-8`。
  - `CMakePresets.json` 增加 `buildPresets`（x64/x86、Debug/Release）。
  - Windows 原生、零第三方依赖：CNG（`bcrypt`）+ Schannel（`secur32`/`crypt32`）+ Winsock + 自带 JSON。
- **公共层**：`Status`/`Result`、十六进制、base64url、RFC3339 UTC（毫秒 3 位）、UUIDv4、线程安全日志。
- **JSON**：自包含最小 JSON 解析/序列化，保留 int64/uint64 精度。
- **密码学**：CNG 实现的 SHA-256、CSPRNG、ECDSA P-256 + SHA-256（P1363 ⇄ DER 转换）。
- **领域模型**：`CommitmentInput`、`AssignedPulse`、`Consumption`、`CommitmentRecord`、`CommitmentResponse`、`ChainHead`。
- **规范哈希**（`CanonicalHash`）：`inputDigest`、`commitmentHash`、`recordHash`、响应签名投影（规范 §5、§7）。
- **存储**：`ICommitmentStore` 抽象 + 追加日志实现 `FileCommitmentStore`
  （JSONL、`fflush` + `_commit` 落盘；消费披露单独追加；启动时校验链连续性）。
- **时钟/日志/配置**：可注入 `IClock`、`ServerConfig`（含 `TlsConfig`）。
- **脉冲源**：`drand-quicknet` 确定性选取（`round`/`publishedAt`，保证 ≥1 个完整周期）；
  `nist-beacon-v2` 骨架；`PulseSourceRegistry`。
- **服务编排**：`CommitmentService`（校验、幂等、签发、查询、分页、消费披露、响应签名）。
- **HTTP**：Winsock 最小 HTTP/1.1 服务端（`HttpServer`）、模板路由（`Router`）、
  方法/路径参数与查询解析、`ApiController` 全部端点路由。
- **TLS**：`ITlsContext` + Schannel 实现；证书从 **Windows 证书存储**（默认 `LocalMachine\My`）
  按 SHA-1 指纹或主题查找并要求私钥；每次连接按指纹检测自动续期并重建凭据。
- **端点**：`POST /v1/commitments`、`GET /v1/commitments/{id}`、`GET /v1/commitments/issued`、
  `GET /v1/chain/head`、`GET /v1/chain/proof/{sequence}`（501）、`POST /v1/draws`。
- **文档**：`Agents.md`（代理/开发者指南与约束清单）、`server.config.example.json`、README 扩充。
- **程序入口**：`main.cpp` 组装存储、签名密钥、脉冲源、TLS 与 HTTP 服务，处理 Ctrl+C 优雅停止。

### Security

- 签发「先落盘（含 fsync）再响应」；落盘失败不签发。
- 承诺链只可追加；启动时重算 `recordHash` 链并拒绝缺号/断裂/被改写的日志。
- nonce 由 CSPRNG 生成（32 字节）；不使用任何可预测值。
- 响应使用独立于 TLS 的服务端签名密钥（ECDSA P-256/SHA-256，DER）。
- TLS 默认启用；明文仅在显式关闭时使用并打印警告。

### Fixed

- 修正 ECDSA DER 验签解码的整数宽度（`r`/`s` 各 32 字节）。
- 修正 TLS 密文缓冲在 `SEC_E_INCOMPLETE_MESSAGE` 时的续收逻辑，避免丢包/自旋。

### Notes

- 规范 §13.6 要求完整性来自全量、只可追加的哈希链；`GET /v1/commitments/issued` 为全量存档入口，
  按身份查询不得作为完整性依据。
- 本版本为框架骨架：核心签发链路可用，但脉冲取回/验签、链上证明、对账、全量归档自证与测试尚未实现
  （见 `[Unreleased]` 与 `Agents.md` §8）。
