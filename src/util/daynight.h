#pragma once
#include <cstdint>
// Night mode schedule: 18:00-06:00 Manaus time (UTC-4 all year). Pure: the UTC seconds come
// from net/clock (SNTP), the result drives the palette and the backlight in main.cpp.
namespace daynight {
constexpr int NIGHT_START = 18 * 60;          // 18:00
constexpr int NIGHT_END   = 6 * 60;           // 06:00
constexpr int32_t UTC_OFFSET_S = -4 * 3600;   // Manaus (America/Manaus), no DST
constexpr int64_t SYNCED_AFTER = 1704067200;  // 2024-01-01 00:00 UTC
// True from NIGHT_START to midnight and from midnight to NIGHT_END.
bool isNight(int minuteOfDay);
// False while the clock still counts from 1970 (no SNTP answer yet).
bool synced(int64_t utcSeconds);
// Local minute of the day (0..1439) for a UTC time and an offset in seconds.
int minuteOfDay(int64_t utcSeconds, int32_t offsetS);
}
