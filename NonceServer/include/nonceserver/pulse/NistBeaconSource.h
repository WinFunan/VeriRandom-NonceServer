// NistBeaconSource.h: NIST Randomness Beacon v2 指派来源骨架（规范 §6.2）。
// 默认来源为 drand-quicknet；NIST 仅作为可配置备选，尚未实现取脉冲与 PKI 校验。
#pragma once

#include "nonceserver/pulse/PulseSource.h"

namespace nonceserver {

class NistBeaconSource : public IPulseSource {
public:
    static constexpr const char* kSourceId = "nist-beacon-v2";
    static constexpr const char* kSubjectConstraint = "CN=engine.beacon.nist.gov";

    std::string Id() const override;
    std::chrono::seconds Period() const override;

    Result<AssignedPulse> SelectUnpublishedPulse(UtcTime nowUtc) const override;
    Result<AssignedPulse> Resolve(const AssignedPulse& assigned) const override;
    Status Verify(const AssignedPulse& assigned) const override;
};

}  // namespace nonceserver
