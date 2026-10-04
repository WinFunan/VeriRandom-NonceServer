#include "nonceserver/service/CommitmentService.h"

#include "nonceserver/canonical/CanonicalHash.h"
#include "nonceserver/common/Base64.h"
#include "nonceserver/common/Hex.h"
#include "nonceserver/common/Uuid.h"
#include "nonceserver/crypto/Random.h"
#include "nonceserver/log/Logger.h"

#include <algorithm>
#include <utility>

namespace nonceserver {

namespace {
constexpr std::size_t kNonceBytes = 32;  // >= 256 位（规范 §4.1）
}  // namespace

CommitmentService::CommitmentService(ServerConfig config, ICommitmentStore& store, IClock& clock,
                                     PulseSourceRegistry& pulseSources, EcdsaP256Key signingKey)
    : config_(std::move(config)),
      store_(store),
      clock_(clock),
      pulseSources_(pulseSources),
      signingKey_(std::move(signingKey)) {
    serverKeyId_ = signingKey_.KeyId();
}

Status CommitmentService::ValidateInput(const CommitmentInput& input) const {
    if (input.protocolVersion != 1) {
        return Status::Error(ErrorCode::InvalidArgument, "unsupported protocolVersion");
    }
    if (input.clientId.empty()) {
        return Status::Error(ErrorCode::InvalidArgument, "clientId is required");
    }
    if (input.drawKind.empty()) {
        return Status::Error(ErrorCode::InvalidArgument, "drawKind is required");
    }
    if (input.count < 1 || input.rosterSize < 0) {
        return Status::Error(ErrorCode::InvalidArgument, "invalid count/rosterSize");
    }
    const auto roster = FromHex(input.rosterDigest);
    if (!roster || roster->size() != 32) {
        return Status::Error(ErrorCode::InvalidArgument, "rosterDigest must be 32-byte hex");
    }
    return Status::Ok();
}

Result<CommitmentResponse> CommitmentService::CreateCommitment(const CommitmentInput& input) {
    const Status valid = ValidateInput(input);
    if (!valid.ok()) return valid;

    auto source = pulseSources_.Get(config_.defaultPulseSource);
    if (!source) {
        return Status::Error(ErrorCode::InvalidArgument,
                             "unknown pulse source: " + config_.defaultPulseSource);
    }

    CommitmentRecord record;
    record.commitmentId = GenerateUuidV4();
    record.issuedAtUtc = clock_.Now();
    record.clientId = input.clientId;
    record.clientNonce = input.clientNonce;
    record.input = input;
    record.nonce = SecureRandomBase64Url(kNonceBytes);

    const auto pulse = source->SelectUnpublishedPulse(record.issuedAtUtc);
    if (!pulse.ok()) return pulse.status();
    record.assignedPulse = pulse.value();
    record.inputDigest = ComputeInputDigest(record.input);

    const auto appended = store_.Append(record, [this](CommitmentRecord& pending) -> Status {
        pending.commitmentHash = ComputeCommitmentHash(pending);
        pending.recordHash =
            ComputeRecordHash(pending.sequence, pending.prevHash, pending.commitmentHash);
        // 响应的 chainHead 即本记录自身的哈希；据此对本记录签名并持久化。
        const std::string projection = ComputeResponseSigningProjection(
            pending.commitmentHash, pending.recordHash, serverKeyId_);
        const auto signature = signingKey_.SignDer(
            reinterpret_cast<const uint8_t*>(projection.data()), projection.size());
        if (!signature.ok()) return signature.status();
        pending.responseSignature = Base64UrlEncode(signature.value());
        return Status::Ok();
    });
    if (!appended.ok()) return appended.status();

    return BuildResponse(appended.value());
}

Result<CommitmentResponse> CommitmentService::BuildResponse(const CommitmentRecord& record) const {
    CommitmentResponse response;
    response.protocolVersion = 1;
    response.sequence = record.sequence;
    response.commitmentId = record.commitmentId;
    response.issuedAtUtc = record.issuedAtUtc;
    response.nonce = record.nonce;
    response.inputDigest = record.inputDigest;
    response.commitmentHash = record.commitmentHash;
    response.assignedPulse = record.assignedPulse;
    // 响应在签发时刻的链头即本记录自身（§7：链头随响应回带，由客户端各自留存）。
    response.chainHeadSequence = record.sequence;
    response.chainHeadRecordHash = record.recordHash;
    response.serverKeyId = serverKeyId_;

    if (!record.responseSignature.empty()) {
        response.signature = record.responseSignature;
        return response;
    }

    const std::string projection = ComputeResponseSigningProjection(
        response.commitmentHash, response.chainHeadRecordHash, response.serverKeyId);
    const auto signature = signingKey_.SignDer(
        reinterpret_cast<const uint8_t*>(projection.data()), projection.size());
    if (!signature.ok()) return signature.status();
    response.signature = Base64UrlEncode(signature.value());

    return response;
}

Result<CommitmentRecord> CommitmentService::GetCommitment(const std::string& commitmentId) const {
    return store_.GetById(commitmentId);
}

Result<std::vector<CommitmentRecord>> CommitmentService::ListIssued(int64_t fromSequence,
                                                                   std::size_t limit) const {
    if (limit == 0) limit = static_cast<std::size_t>(config_.defaultPageSize);
    limit = std::min(limit, static_cast<std::size_t>(config_.maxPageSize));
    return store_.ListBySequence(fromSequence, limit);
}

ChainHead CommitmentService::GetChainHead() const { return store_.Head(); }

Status CommitmentService::RecordConsumption(const DrawDisclosure& disclosure) {
    if (disclosure.commitmentId.empty()) {
        return Status::Error(ErrorCode::InvalidArgument, "commitmentId is required");
    }
    Consumption consumption;
    consumption.proofId = disclosure.proofId;
    consumption.proofHash = disclosure.proofHash;
    consumption.reportedAtUtc = clock_.Now();
    return store_.MarkConsumed(disclosure.commitmentId, consumption);
}

}  // namespace nonceserver
