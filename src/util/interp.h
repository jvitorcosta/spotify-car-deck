#pragma once
#include <cstdint>
namespace interp {
uint32_t currentProgressMs(uint32_t lastProgressMs, uint32_t durationMs,
                           bool isPlaying, uint32_t msSincePoll);
}
