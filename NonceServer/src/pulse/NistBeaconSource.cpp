#include "nonceserver/pulse/NistBeaconSource.h"

namespace nonceserver {

std::string NistBeaconSource::Id() const { return kSourceId; }

std::chrono::seconds NistBeaconSource::Period() const { return std::chrono::seconds(60); }

Result<AssignedPulse> NistBeaconSource::SelectUnpublishedPulse(UtcTime) const {
    // TODO(§6.2): 时间戳落在整分；身份为 chainIndex + pulseIndex + timeStamp，
    //   chainIndex/pulseIndex 必须以 NIST 实际返回为准，不得写死。需要实时 API。
    return Status::Error(ErrorCode::NotImplemented,
                         "nist-beacon-v2 pulse selection is not implemented yet");
}

Result<AssignedPulse> NistBeaconSource::Resolve(const AssignedPulse&) const {
    return Status::Error(ErrorCode::NotImplemented,
                         "nist-beacon-v2 pulse resolution is not implemented yet");
}

Status NistBeaconSource::Verify(const AssignedPulse&) const {
    // TODO(§6.2): 校验叶子证书 Subject == CN=engine.beacon.nist.gov，CA 链交给系统信任库；
    //   再按证书公钥验证脉冲签名。
    return Status::Error(ErrorCode::NotImplemented,
                         "nist-beacon-v2 signature verification is not implemented yet");
}

}  // namespace nonceserver
