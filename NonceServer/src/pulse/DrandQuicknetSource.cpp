#include "nonceserver/pulse/DrandQuicknetSource.h"

namespace nonceserver {

std::string DrandQuicknetSource::Id() const { return kSourceId; }

std::chrono::seconds DrandQuicknetSource::Period() const { return std::chrono::seconds(3); }

int64_t DrandQuicknetSource::GenesisUnixSeconds() { return 1692803367; }

Result<AssignedPulse> DrandQuicknetSource::SelectUnpublishedPulse(UtcTime nowUtc) const {
    const int64_t genesis = GenesisUnixSeconds();
    const int64_t period = Period().count();
    const int64_t issuedMs = nowUtc.time_since_epoch().count();

    // 目标：publishedAt >= issuedAt + 1 个完整周期，且严格晚于 issuedAt。
    // publishedAt 只取整秒，因此先把下界向上取整到秒。
    const int64_t targetMs = issuedMs + period * 1000;
    int64_t targetSeconds = targetMs / 1000;
    if (targetMs % 1000 != 0) ++targetSeconds;

    // round = floor((t - genesis) / period)，再推进到满足下界。
    int64_t round = (targetSeconds - genesis) / period;
    if (round < 0) round = 0;
    while (genesis + round * period < targetSeconds) ++round;

    AssignedPulse pulse;
    pulse.source = kSourceId;
    pulse.chainHash = kChainHash;
    pulse.round = round;
    pulse.publishedAt =
        UtcTime(std::chrono::milliseconds((genesis + round * period) * 1000));
    return pulse;
}

Result<AssignedPulse> DrandQuicknetSource::Resolve(const AssignedPulse&) const {
    // TODO(§6.1): GET https://api.drand.sh/{chainHash}/public/{round}
    //   - 需要 HTTPS 客户端（可复用 Schannel 的客户端侧，或独立 HTTP 客户端）；
    //   - 返回 randomness / signature，供 Verify 使用。
    return Status::Error(ErrorCode::NotImplemented,
                         "drand pulse resolution is not implemented yet");
}

Status DrandQuicknetSource::Verify(const AssignedPulse&) const {
    // TODO(§6.1/§9): 使用内置链公钥对 round 的 BLS (bls-unchained-g1-rfc9380) 签名做校验。
    //   注意：BLS12-381 校验需要专门实现或依赖库，当前自包含方案尚未提供。
    return Status::Error(ErrorCode::NotImplemented,
                         "drand BLS signature verification is not implemented yet");
}

}  // namespace nonceserver
