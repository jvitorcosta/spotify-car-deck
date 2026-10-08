#include "netplan.h"
#include "text.h"
#include <cstring>

namespace netplan {

bool TrackGen::update(const char* track) {
    if (!track || !track[0]) return false;
    if (strncmp(track, last_, sizeof(last_) - 1) == 0) return false;
    txt::copy(last_, track, sizeof(last_));
    ++gen_;
    return true;
}

Work freshWork(bool walkerReady) { return {!walkerReady, true, true, true, true}; }

Step next(const Work& w, bool lyricsReady) {
    if (w.art) return Step::Art;
    if (w.lyrics && lyricsReady) return Step::Lyrics;
    if (w.genre) return Step::Genre;
    if (w.walk) return Step::Walk;
    if (w.prefetch) return Step::Prefetch;
    return Step::None;
}

void done(Work& w, Step s) {
    switch (s) {
        case Step::Walk:     w.walk = false; break;
        case Step::Art:      w.art = false; break;
        case Step::Lyrics:   w.lyrics = false; break;
        case Step::Prefetch: w.prefetch = false; break;
        case Step::Genre:    w.genre = false; break;
        case Step::None:     break;
    }
}

bool canRun(Step s, unsigned largest) {
    switch (s) {
        case Step::Art:      return largest >= ART_NEED;
        case Step::Lyrics:
        case Step::Genre:
        case Step::Walk:
        case Step::Prefetch: return largest >= TLS_NEED;
        case Step::None:     return true;
    }
    return true;
}

bool LyricsRetry::onResult(lyricstatus::Result r, uint32_t nowMs) {
    ++attempts_;
    if (r != lyricstatus::Result::TempError || attempts_ >= 3) {
        waiting_ = false;
        return true;
    }
    waiting_ = true;
    since_ = nowMs;
    wait_ = attempts_ == 1 ? RETRY1_MS : RETRY2_MS;
    return false;
}

}
