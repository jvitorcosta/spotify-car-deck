#pragma once
#include <cstdint>
// The CYD's built-in 8-bit DAC word stream (board/cyd/sink_dac.cpp). In: clip-rate samples on
// the 0..65535 scale. Out: DAC words at twice the clip rate (the IDF 4.4 DAC clock divider wraps
// below ~19.6 kHz), odd words the midpoint to the next sample; first-order noise shaping feeds the
// dropped low byte into the next word, moving the hiss up towards 16 kHz. PURE, host-tested.
namespace dacstream {
constexpr int32_t MID = 32768;             // silence on the 0..65535 scale
int32_t rampUp(int i, int ramp);           // DAC idle 0 -> MID over `ramp` samples (no pop)
int32_t rampDown(int i, int ramp);         // MID -> 0
class Encoder {
public:
    // One clip-rate sample in; 0 words out on the first call, else 2 (the previous sample and
    // the midpoint to this one). Returns the count written to out.
    int push(int32_t v, uint16_t* out);
    // Ends the stream (the sample after the last is the idle 0): 2 words, or 0 if nothing was pushed.
    int finish(uint16_t* out);
private:
    uint16_t shape(int32_t v);
    bool have_ = false;
    int32_t prev_ = 0, err_ = 0;
};
}
