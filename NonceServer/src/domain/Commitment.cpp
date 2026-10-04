#include "nonceserver/domain/Commitment.h"

namespace nonceserver {

namespace {

std::string GetString(const Json& json, std::string_view key) {
    const Json* value = json.find(key);
    return value ? value->asString() : std::string();
}

int64_t GetInt(const Json& json, std::string_view key, int64_t fallback = 0) {
    const Json* value = json.find(key);
    return value ? value->asInt64(fallback) : fallback;
}

UtcTime GetTime(const Json& json, std::string_view key) {
    const Json* value = json.find(key);
    if (value && value->isString()) {
        if (const auto parsed = ParseRfc3339(value->asString())) return *parsed;
    }
    return UtcTime{};
}

}  // namespace

Json CommitmentInput::ToJson() const {
    Json json(Json::Object{});
    json["protocolVersion"] = protocolVersion;
    json["clientId"] = clientId;
    if (!clientNonce.empty()) json["clientNonce"] = clientNonce;
    json["drawKind"] = drawKind;
    json["count"] = count;
    json["samplingMode"] = samplingMode;
    json["algorithmProfile"] = algorithmProfile;
    json["rosterDigest"] = rosterDigest;
    json["rosterSize"] = rosterSize;
    return json;
}

CommitmentInput CommitmentInput::FromJson(const Json& json) {
    CommitmentInput input;
    input.protocolVersion = static_cast<int>(GetInt(json, "protocolVersion", 1));
    input.clientId = GetString(json, "clientId");
    input.clientNonce = GetString(json, "clientNonce");
    input.drawKind = GetString(json, "drawKind");
    input.count = GetInt(json, "count", 1);
    input.samplingMode = GetString(json, "samplingMode");
    input.algorithmProfile = GetString(json, "algorithmProfile");
    input.rosterDigest = GetString(json, "rosterDigest");
    input.rosterSize = GetInt(json, "rosterSize", 0);
    return input;
}

Json AssignedPulse::ToJson() const {
    Json json(Json::Object{});
    json["source"] = source;
    if (!chainHash.empty()) json["chainHash"] = chainHash;
    if (!chainIndex.empty()) json["chainIndex"] = chainIndex;
    if (source == "nist-beacon-v2") {
        if (!pulseIndex.empty()) json["pulseIndex"] = pulseIndex;
    } else {
        json["round"] = round;
    }
    json["publishedAt"] = FormatRfc3339(publishedAt);
    return json;
}

AssignedPulse AssignedPulse::FromJson(const Json& json) {
    AssignedPulse pulse;
    pulse.source = GetString(json, "source");
    pulse.chainHash = GetString(json, "chainHash");
    pulse.round = GetInt(json, "round", 0);
    pulse.chainIndex = GetString(json, "chainIndex");
    pulse.pulseIndex = GetString(json, "pulseIndex");
    pulse.publishedAt = GetTime(json, "publishedAt");
    return pulse;
}

std::string AssignedPulse::SourceIdentity() const {
    return chainHash.empty() ? chainIndex : chainHash;
}

std::string AssignedPulse::RoundIdentity() const {
    if (pulseIndex.empty()) return std::to_string(round);
    return pulseIndex;
}

Json Consumption::ToJson() const {
    Json json(Json::Object{});
    json["proofId"] = proofId;
    json["proofHash"] = proofHash;
    json["reportedAtUtc"] = FormatRfc3339(reportedAtUtc);
    return json;
}

Consumption Consumption::FromJson(const Json& json) {
    Consumption consumption;
    consumption.proofId = GetString(json, "proofId");
    consumption.proofHash = GetString(json, "proofHash");
    consumption.reportedAtUtc = GetTime(json, "reportedAtUtc");
    return consumption;
}

Json CommitmentRecord::ToRecordJson() const {
    Json json(Json::Object{});
    json["sequence"] = sequence;
    json["commitmentId"] = commitmentId;
    json["issuedAtUtc"] = FormatRfc3339(issuedAtUtc);
    json["clientId"] = clientId;
    if (!clientNonce.empty()) json["clientNonce"] = clientNonce;
    json["input"] = input.ToJson();
    json["inputDigest"] = inputDigest;
    json["nonce"] = nonce;
    json["assignedPulse"] = assignedPulse.ToJson();
    json["commitmentHash"] = commitmentHash;
    json["prevHash"] = prevHash;
    json["recordHash"] = recordHash;
    json["pulseStatus"] = pulseStatus;
    if (!responseSignature.empty()) json["responseSignature"] = responseSignature;
    if (consumed) json["consumed"] = consumed->ToJson();
    return json;
}

CommitmentRecord CommitmentRecord::FromRecordJson(const Json& json) {
    CommitmentRecord record;
    record.sequence = GetInt(json, "sequence", 0);
    record.commitmentId = GetString(json, "commitmentId");
    record.issuedAtUtc = GetTime(json, "issuedAtUtc");
    record.clientId = GetString(json, "clientId");
    record.clientNonce = GetString(json, "clientNonce");
    if (const Json* input = json.find("input")) record.input = CommitmentInput::FromJson(*input);
    record.inputDigest = GetString(json, "inputDigest");
    record.nonce = GetString(json, "nonce");
    if (const Json* pulse = json.find("assignedPulse")) {
        record.assignedPulse = AssignedPulse::FromJson(*pulse);
    }
    record.commitmentHash = GetString(json, "commitmentHash");
    record.prevHash = GetString(json, "prevHash");
    record.recordHash = GetString(json, "recordHash");
    if (json.contains("pulseStatus")) record.pulseStatus = GetString(json, "pulseStatus");
    record.responseSignature = GetString(json, "responseSignature");
    if (const Json* consumed = json.find("consumed")) {
        record.consumed = Consumption::FromJson(*consumed);
    }
    return record;
}

Json CommitmentResponse::ToJson() const {
    Json json(Json::Object{});
    json["protocolVersion"] = protocolVersion;
    json["sequence"] = sequence;
    json["commitmentId"] = commitmentId;
    json["issuedAtUtc"] = FormatRfc3339(issuedAtUtc);
    json["nonce"] = nonce;
    json["inputDigest"] = inputDigest;
    json["commitmentHash"] = commitmentHash;
    json["assignedPulse"] = assignedPulse.ToJson();
    Json head(Json::Object{});
    head["sequence"] = chainHeadSequence;
    head["recordHash"] = chainHeadRecordHash;
    json["chainHead"] = head;
    json["serverKeyId"] = serverKeyId;
    json["signature"] = signature;
    return json;
}

Json ChainHead::ToJson() const {
    Json json(Json::Object{});
    json["sequence"] = sequence;
    json["recordHash"] = recordHash;
    return json;
}

}  // namespace nonceserver
