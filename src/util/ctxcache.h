#pragma once
#include <cstdint>
// Remembers which Spotify context URI (playlist/album/artist) was last resolved to a name.
// A failed lookup is retried after RETRY_MS instead of being cached for good: before, one
// failed request left the header saying "playlist" until the context changed. PURE, host-tested.
namespace ctxcache {
constexpr uint32_t RETRY_MS = 60000;
class Cache {
public:
    // True when uri is not the cached one, or its last lookup failed at least RETRY_MS ago.
    bool needsLookup(const char* uri, uint32_t nowMs) const;
    void store(const char* uri, bool ok, uint32_t nowMs);
private:
    char uri_[64] = "";
    bool ok_ = false;
    bool stored_ = false;
    uint32_t at_ = 0;
};
}
