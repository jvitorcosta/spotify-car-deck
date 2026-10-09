#pragma once
#include <cstddef>
#include <cstdint>
// Audio output for the greeting clips; one implementation per board, picked by build_src_filter:
// board/cyd/sink_dac.cpp (built-in 8-bit DAC) and board/s3/sink_es8311.cpp (ES8311 codec).
// Used by one playback task at a time (src/audio/greeting.cpp).
namespace audiosink {
extern const int GREETING_VOLUME;   // percent of the clip's level (<= 100), per board
extern const int FINALE_VOLUME;
bool open(int clipRate);                    // false: no audio this time (the sink logs why)
void write(const int16_t* s, size_t n);     // clip-rate samples, volume applied; blocks on a full DMA
void close();                               // plays out what's queued, then silence and release
}
