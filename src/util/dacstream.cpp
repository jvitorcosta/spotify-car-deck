#include "dacstream.h"

namespace dacstream {

int32_t rampUp(int i, int ramp) { return MID * i / ramp; }
int32_t rampDown(int i, int ramp) { return MID * (ramp - i) / ramp; }

uint16_t Encoder::shape(int32_t v) {
    const int32_t want = v + err_;
    const int32_t q = (want < 0 ? 0 : want > 65535 ? 65535 : want) & 0xFF00;
    err_ = want - q;
    return (uint16_t)q;
}

int Encoder::push(int32_t v, uint16_t* out) {
    if (!have_) {
        prev_ = v;
        have_ = true;
        return 0;
    }
    out[0] = shape(prev_);
    out[1] = shape((prev_ + v) / 2);
    prev_ = v;
    return 2;
}

int Encoder::finish(uint16_t* out) {
    if (!have_) return 0;
    have_ = false;
    out[0] = shape(prev_);
    out[1] = shape(prev_ / 2);   // midpoint to the idle 0
    return 2;
}

}
