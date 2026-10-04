// Clock.h: 可注入的时钟抽象（便于测试与未来 NTP 偏差记录，规范 §10）。
#pragma once

#include "nonceserver/common/Time.h"

namespace nonceserver {

class IClock {
public:
    virtual ~IClock() = default;
    virtual UtcTime Now() const = 0;
};

class SystemClock : public IClock {
public:
    UtcTime Now() const override;
};

}  // namespace nonceserver
