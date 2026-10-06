#include "netplan.h"
#include <cstring>

namespace netplan {

bool TrackGen::update(const char* track) {
    if (!track || !track[0]) return false;
    if (strncmp(track, last_, sizeof(last_) - 1) == 0) return false;
    strncpy(last_, track, sizeof(last_) - 1);
    last_[sizeof(last_) - 1] = '\0';
    ++gen_;
    return true;
}

Work freshWork(bool walkerReady) { return {!walkerReady, true, true, true}; }

Step next(const Work& w) {
    if (w.walk) return Step::Walk;
    if (w.art) return Step::Art;
    if (w.lyrics) return Step::Lyrics;
    if (w.prefetch) return Step::Prefetch;
    return Step::None;
}

void done(Work& w, Step s) {
    switch (s) {
        case Step::Walk:     w.walk = false; break;
        case Step::Art:      w.art = false; break;
        case Step::Lyrics:   w.lyrics = false; break;
        case Step::Prefetch: w.prefetch = false; break;
        case Step::None:     break;
    }
}

}
