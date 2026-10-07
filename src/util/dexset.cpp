#include "dexset.h"

namespace dexset {

bool Set::has(int dex) const {
    if (dex <= 0) return false;
    for (int i = 0; i < N; ++i)
        if (v_[i] == dex) return true;
    return false;
}

void Set::add(int dex) {
    if (dex <= 0 || has(dex)) return;
    v_[next_] = dex;
    next_ = (next_ + 1) % N;
}

}
