#pragma once
#include <stddef.h>
// Byte-addressable internal RAM — what byte buffers, mbedTLS and the WiFi driver can use.
// ESP.getFreeHeap()/getMaxAllocHeap() also count 32-bit-only IRAM and read ~60 KB too high;
// that hid an exhausted heap (README "Design & performance history").
namespace mem {
size_t byteFree();
size_t byteLargest();
void log(const char* where);   // "[mem] <where> free=... largest=..."
void installFailHook();        // "[allocfail] <size> caps=... largest=... free=..." on every failed malloc
}
