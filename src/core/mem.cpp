#include "mem.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_attr.h>

namespace mem {

static const uint32_t CAPS = MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL;

// Failed allocations are only counted here: they mostly happen inside the WiFi driver while
// packets burst in, and printing from there (115200 baud, ~5 ms a line) slowed the very
// download that was short of memory. mem::log() reports them.
// The hook runs on either core, possibly with the flash cache off, so it lives in IRAM and
// only does atomic adds and plain stores (the old `x = x + 1` could lose counts).
static uint32_t s_fails = 0;
static uint32_t s_failTotal = 0;
static volatile uint32_t s_lastSize = 0;
static volatile uint32_t s_lastCaps = 0;

size_t byteFree() { return heap_caps_get_free_size(CAPS); }
size_t byteLargest() { return heap_caps_get_largest_free_block(CAPS); }

void log(const char* where) {
    uint32_t n = __atomic_exchange_n(&s_fails, 0u, __ATOMIC_RELAXED);
    // Free stack of the calling task (the network task, whose stack was cut to 10 KB).
    unsigned stackFree = (unsigned)uxTaskGetStackHighWaterMark(nullptr);
    if (n)
        Serial.printf("[mem] %s free=%u largest=%u stackfree=%u allocfail=%u (last %u B caps=0x%x)\n",
                      where, (unsigned)byteFree(), (unsigned)byteLargest(), stackFree,
                      (unsigned)n, (unsigned)s_lastSize, (unsigned)s_lastCaps);
    else
        Serial.printf("[mem] %s free=%u largest=%u stackfree=%u\n", where, (unsigned)byteFree(),
                      (unsigned)byteLargest(), stackFree);
}

static void IRAM_ATTR onAllocFail(size_t size, uint32_t caps, const char*) {
    __atomic_fetch_add(&s_fails, 1u, __ATOMIC_RELAXED);
    __atomic_fetch_add(&s_failTotal, 1u, __ATOMIC_RELAXED);
    s_lastSize = (uint32_t)size;
    s_lastCaps = caps;
}

uint32_t failTotal() { return __atomic_load_n(&s_failTotal, __ATOMIC_RELAXED); }

void installFailHook() { heap_caps_register_failed_alloc_callback(onAllocFail); }

}
