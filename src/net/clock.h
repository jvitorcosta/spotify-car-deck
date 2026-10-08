#pragma once
// Wall clock from SNTP (UTC). begin() once WiFi is up; SNTP then re-syncs in the background
// (UDP, no TLS). Named netclock: `clock` would collide with the C library's clock().
namespace netclock {
void begin();
// Manaus minute of the day (0..1439); false until the first SNTP answer.
bool minuteOfDay(int* m);
}
