#pragma once
#include <stddef.h>
#include <stdint.h>
namespace fetch {
// HTTPS GET of `url` into a heap buffer. Fails on non-200, unknown/oversized length
// (> maxLen) or a stalled/short read. On success *out holds *outLen bytes plus a
// trailing NUL (so text can be parsed in place); caller free()s it.
bool httpsGet(const char* url, size_t maxLen, uint8_t** out, size_t* outLen,
              int* httpCode = nullptr);
}
