// Commitment.h: 预承诺协议的领域模型（对应规范 §3~§6）。
#pragma once

#include "nonceserver/common/Time.h"
#include "nonceserver/json/Json.h"

#include <cstdint>
#include <optional>
#include <string>

namespace nonceserver {

// 一次抽取的输入摘要所覆盖的字段（规范 §4.1 请求、§5 inputDigest）。
struct CommitmentInput {
    int protocolVersion = 1;
    std::string clientId;
    std::string clientNonce;
    std::string drawKind;
    int64_t count = 1;
    std::string samplingMode;
    std::string algorithmProfile;
    std::string rosterDigest;
    int64_t rosterSize = 0;

    Json ToJson() const;
    static CommitmentInput FromJson(const Json& json);
};

// 指派脉冲身份（规范 §6）。source 决定使用哪组字段。
struct AssignedPulse {
    std::string source;       // "drand-quicknet" | "nist-beacon-v2"
    std::string chainHash;    // drand-quicknet
    int64_t round = 0;        // drand-quicknet
    std::string chainIndex;   // nist-beacon-v2
    std::string pulseIndex;   // nist-beacon-v2
    UtcTime publishedAt{};

    Json ToJson() const;
    static AssignedPulse FromJson(const Json& json);

    // 规范 §5 中的 “chainHash(or chainIndex)”。
    std::string SourceIdentity() const;
    // 规范 §5 中的 “round(or pulseIndex)”。
    std::string RoundIdentity() const;
};

// 消费披露（规范 §4.6 / §8）。不是权威声明，只是诚实客户端的主动披露。
struct Consumption {
    std::string proofId;
    std::string proofHash;
    UtcTime reportedAtUtc{};

    Json ToJson() const;
    static Consumption FromJson(const Json& json);
};

// 承诺链记录（规范 §3）。只可追加；consumed 是唯一允许的原地字段。
struct CommitmentRecord {
    int64_t sequence = 0;
    std::string commitmentId;
    UtcTime issuedAtUtc{};
    std::string clientId;
    std::string clientNonce;
    CommitmentInput input;
    std::string inputDigest;
    std::string nonce;  // base64url
    AssignedPulse assignedPulse;
    std::string commitmentHash;
    std::string prevHash;
    std::string recordHash;
    std::string pulseStatus = "normal";  // normal | pulse-missing | pulse-invalid
    // 签发时的响应签名（base64url）。派生自 commitmentHash + recordHash + serverKeyId，
    // 不纳入 recordHash 覆盖范围，仅用于 §4.2 对账时返回“原文”。
    std::string responseSignature;
    std::optional<Consumption> consumed;

    Json ToRecordJson() const;
    static CommitmentRecord FromRecordJson(const Json& json);
};

// POST /v1/commitments 的响应体（规范 §4.1，签名前的投影）。
struct CommitmentResponse {
    int protocolVersion = 1;
    int64_t sequence = 0;
    std::string commitmentId;
    UtcTime issuedAtUtc{};
    std::string nonce;
    std::string inputDigest;
    std::string commitmentHash;
    AssignedPulse assignedPulse;
    int64_t chainHeadSequence = 0;
    std::string chainHeadRecordHash;
    std::string serverKeyId;
    std::string signature;  // base64url(DER)

    Json ToJson() const;
};

// POST /v1/draws 的请求体（规范 §4.6）。
struct DrawDisclosure {
    int protocolVersion = 1;
    std::string commitmentId;
    std::string proofId;
    std::string proofHash;
    UtcTime consumedAtUtc{};
};

// GET /v1/chain/head 的响应（规范 §4.4）。sequence 为 -1 表示链为空。
struct ChainHead {
    int64_t sequence = -1;
    std::string recordHash;

    Json ToJson() const;
};

}  // namespace nonceserver
