#include "nonceserver/clock/Clock.h"

namespace nonceserver {

UtcTime SystemClock::Now() const {
    return std::chrono::time_point_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now());
}

}  // namespace nonceserver
