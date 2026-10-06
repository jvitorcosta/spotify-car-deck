#pragma once
#include <stddef.h>
#include <stdint.h>
namespace fetch {
// HTTPS GET of `url` into the caller's buffer (no per-download malloc while TLS is open).
// Fails on non-200, unknown length, len + 1 > cap, or a stalled/short read (8 s handshake
// and read timeouts). On success buf holds *outLen bytes plus a trailing NUL.
bool httpsGetInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                  int* httpCode = nullptr);
}
