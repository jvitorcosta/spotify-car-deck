#include "ctxcache.h"
#include "text.h"
#include <cstring>

namespace ctxcache {

bool Cache::needsLookup(const char* uri, uint32_t nowMs) const {
    if (!uri) uri = "";
    if (!stored_ || strncmp(uri_, uri, sizeof(uri_) - 1) != 0) return true;
    return !ok_ && nowMs - at_ >= RETRY_MS;   // unsigned difference: survives millis() wrap
}

void Cache::store(const char* uri, bool ok, uint32_t nowMs) {
    if (!uri) uri = "";
    txt::copy(uri_, uri, sizeof(uri_));
    ok_ = ok;
    stored_ = true;
    at_ = nowMs;
}

}
