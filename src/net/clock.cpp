#include "clock.h"
#include <Arduino.h>
#include <time.h>
#include "../util/daynight.h"

namespace netclock {

void begin() {
    static bool started = false;
    if (started) return;
    started = true;
    configTime(0, 0, "pool.ntp.org", "time.google.com");   // UTC; daynight adds the offset
    Serial.println("[clock] SNTP started");
}

bool minuteOfDay(int* m) {
    time_t now = time(nullptr);
    if (!daynight::synced((int64_t)now)) return false;
    *m = daynight::minuteOfDay((int64_t)now, daynight::UTC_OFFSET_S);
    return true;
}

}
