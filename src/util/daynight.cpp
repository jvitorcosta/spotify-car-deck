#include "daynight.h"

namespace daynight {

bool isNight(int m) { return m >= NIGHT_START || m < NIGHT_END; }

bool synced(int64_t utcSeconds) { return utcSeconds >= SYNCED_AFTER; }

int minuteOfDay(int64_t utcSeconds, int32_t offsetS) {
    int64_t s = (utcSeconds + offsetS) % 86400;
    if (s < 0) s += 86400;   // before 1970 + offset: keep it in 0..86399
    return (int)(s / 60);
}

}
