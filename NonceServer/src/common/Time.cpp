#include "nonceserver/common/Time.h"

#include <cstdio>

namespace nonceserver {

namespace {

// Howard Hinnant 的 civil-date 算法（不依赖 localtime/gmtime，避免时区与线程问题）。

int64_t DaysFromCivil(int64_t year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int64_t era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

void CivilFromDays(int64_t z, int64_t& year, unsigned& month, unsigned& day) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    year = static_cast<int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp + (mp < 10 ? 3 : -9);
    year += (month <= 2);
}

bool ParseFixed(std::string_view s, size_t pos, size_t count, int64_t& out) {
    if (pos + count > s.size()) return false;
    int64_t value = 0;
    for (size_t i = 0; i < count; ++i) {
        const char c = s[pos + i];
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
    }
    out = value;
    return true;
}

}  // namespace

std::string FormatRfc3339(UtcTime time) {
    int64_t totalMs = time.time_since_epoch().count();
    int64_t days = totalMs / 86400000;
    int64_t remainder = totalMs % 86400000;
    if (remainder < 0) {
        remainder += 86400000;
        --days;
    }

    int64_t year = 0;
    unsigned month = 0;
    unsigned day = 0;
    CivilFromDays(days, year, month, day);

    const int hour = static_cast<int>(remainder / 3600000);
    remainder %= 3600000;
    const int minute = static_cast<int>(remainder / 60000);
    remainder %= 60000;
    const int second = static_cast<int>(remainder / 1000);
    const int millis = static_cast<int>(remainder % 1000);

    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%04lld-%02u-%02uT%02d:%02d:%02d.%03dZ",
                  static_cast<long long>(year), month, day, hour, minute, second, millis);
    return std::string(buffer);
}

std::optional<UtcTime> ParseRfc3339(std::string_view text) {
    // 最短形式: YYYY-MM-DDTHH:MM:SSZ
    if (text.size() < 20) return std::nullopt;

    int64_t year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (!ParseFixed(text, 0, 4, year) || text[4] != '-') return std::nullopt;
    if (!ParseFixed(text, 5, 2, month) || text[7] != '-') return std::nullopt;
    if (!ParseFixed(text, 8, 2, day)) return std::nullopt;
    if (text[10] != 'T' && text[10] != 't') return std::nullopt;
    if (!ParseFixed(text, 11, 2, hour) || text[13] != ':') return std::nullopt;
    if (!ParseFixed(text, 14, 2, minute) || text[16] != ':') return std::nullopt;
    if (!ParseFixed(text, 17, 2, second)) return std::nullopt;

    size_t index = 19;
    int64_t millis = 0;
    if (index < text.size() && text[index] == '.') {
        ++index;
        int digits = 0;
        while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
            if (digits < 3) {
                millis = millis * 10 + (text[index] - '0');
            }
            ++digits;
            ++index;
        }
        while (digits < 3) {
            millis *= 10;
            ++digits;
        }
    }
    if (index >= text.size()) return std::nullopt;
    if (text[index] != 'Z' && text[index] != 'z') return std::nullopt;
    if (index + 1 != text.size()) return std::nullopt;

    const int64_t days = DaysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
    const int64_t seconds =
        ((days * 24 + hour) * 60 + minute) * 60 + second;
    return UtcTime(std::chrono::milliseconds(seconds * 1000 + millis));
}

}  // namespace nonceserver
