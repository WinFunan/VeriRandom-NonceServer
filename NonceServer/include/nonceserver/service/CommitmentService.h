// CommitmentService.h: 预承诺协议的业务编排（规范 §4、§5、§7）。
#pragma once

#include "nonceserver/clock/Clock.h"
#include "nonceserver/common/Result.h"
#include "nonceserver/config/ServerConfig.h"
#include "nonceserver/crypto/Ecdsa.h"
#include "nonceserver/domain/Commitment.h"
#include "nonceserver/pulse/PulseSourceRegistry.h"
#include "nonceserver/storage/ICommitmentStore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nonceserver {

class CommitmentService {
public:
    CommitmentService(ServerConfig config, ICommitmentStore& store, IClock& clock,
                      PulseSourceRegistry& pulseSources, EcdsaP256Key signingKey);
    CommitmentService(const CommitmentService&) = delete;
    CommitmentService& operator=(const CommitmentService&) = delete;

    // 签发承诺。先落盘再返回（由存储层保证 fsync）。
    Result<CommitmentResponse> CreateCommitment(const CommitmentInput& input);

    Result<CommitmentRecord> GetCommitment(const std::string& commitmentId) const;
    Result<std::vector<CommitmentRecord>> ListIssued(int64_t fromSequence,
                                                     std::size_t limit) const;
    ChainHead GetChainHead() const;

    Status RecordConsumption(const DrawDisclosure& disclosure);

    const std::string& ServerKeyId() const { return serverKeyId_; }

private:
    Status ValidateInput(const CommitmentInput& input) const;
    Result<CommitmentResponse> BuildResponse(const CommitmentRecord& record) const;

    ServerConfig config_;
    ICommitmentStore& store_;
    IClock& clock_;
    PulseSourceRegistry& pulseSources_;
    EcdsaP256Key signingKey_;
    std::string serverKeyId_;
};

}  // namespace nonceserver
