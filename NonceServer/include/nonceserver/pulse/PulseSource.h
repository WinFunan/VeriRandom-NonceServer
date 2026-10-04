// PulseSource.h: 指派脉冲来源抽象（规范 §6）。
#pragma once

#include "nonceserver/common/Result.h"
#include "nonceserver/domain/Commitment.h"

#include <chrono>
#include <string>

namespace nonceserver {

class IPulseSource {
public:
    virtual ~IPulseSource() = default;

    virtual std::string Id() const = 0;
    virtual std::chrono::seconds Period() const = 0;

    // 选定一个在 nowUtc 时刻尚未发布、且至少留下一个完整周期的具体脉冲（规范 §4.1、§14-4）。
    virtual Result<AssignedPulse> SelectUnpublishedPulse(UtcTime nowUtc) const = 0;

    // 拉取并返回已解析的脉冲（用于消费阶段的校验）。
    virtual Result<AssignedPulse> Resolve(const AssignedPulse& assigned) const = 0;

    // 按内置材料校验脉冲签名/身份（规范 §6、§9）。
    virtual Status Verify(const AssignedPulse& assigned) const = 0;
};

}  // namespace nonceserver
