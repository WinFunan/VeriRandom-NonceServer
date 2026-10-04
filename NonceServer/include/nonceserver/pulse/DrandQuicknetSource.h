// DrandQuicknetSource.h: drand quicknet（chained 之外的单轮可独立校验）指派来源。
#pragma once

#include "nonceserver/pulse/PulseSource.h"

namespace nonceserver {

class DrandQuicknetSource : public IPulseSource {
public:
    static constexpr const char* kSourceId = "drand-quicknet";
    static constexpr const char* kChainHash =
        "52db9ba70e0cc0f6eaf7803dd07447a1f5477735fd3f661792ba94600c84e971";

    std::string Id() const override;
    std::chrono::seconds Period() const override;

    Result<AssignedPulse> SelectUnpublishedPulse(UtcTime nowUtc) const override;
    Result<AssignedPulse> Resolve(const AssignedPulse& assigned) const override;
    Status Verify(const AssignedPulse& assigned) const override;

    // 规范 §6.1 常量。
    static int64_t GenesisUnixSeconds();
};

}  // namespace nonceserver
