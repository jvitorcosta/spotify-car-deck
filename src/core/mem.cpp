#include "mem.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

namespace mem {

static const uint32_t CAPS = MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL;

// Failed allocations are only counted here: they mostly happen inside the WiFi driver while
// packets burst in, and printing from there (115200 baud, ~5 ms a line) slowed the very
// download that was short of memory. mem::log() reports them.
static volatile uint32_t s_fails = 0;
static volatile uint32_t s_lastSize = 0;
static volatile uint32_t s_lastCaps = 0;

size_t byteFree() { return heap_caps_get_free_size(CAPS); }
size_t byteLargest() { return heap_caps_get_largest_free_block(CAPS); }

void log(const char* where) {
    uint32_t n = s_fails;
    s_fails = 0;
    if (n)
        Serial.printf("[mem] %s free=%u largest=%u allocfail=%u (last %u B caps=0x%x)\n", where,
                      (unsigned)byteFree(), (unsigned)byteLargest(), (unsigned)n,
                      (unsigned)s_lastSize, (unsigned)s_lastCaps);
    else
        Serial.printf("[mem] %s free=%u largest=%u\n", where, (unsigned)byteFree(),
                      (unsigned)byteLargest());
}

static void onAllocFail(size_t size, uint32_t caps, const char*) {
    s_fails = s_fails + 1;
    s_lastSize = (uint32_t)size;
    s_lastCaps = caps;
}

void installFailHook() { heap_caps_register_failed_alloc_callback(onAllocFail); }

}
