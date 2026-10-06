#include "mem.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

namespace mem {

static const uint32_t CAPS = MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL;

size_t byteFree() { return heap_caps_get_free_size(CAPS); }
size_t byteLargest() { return heap_caps_get_largest_free_block(CAPS); }

void log(const char* where) {
    Serial.printf("[mem] %s free=%u largest=%u\n", where, (unsigned)byteFree(), (unsigned)byteLargest());
}

static void onAllocFail(size_t size, uint32_t caps, const char* fn) {
    ets_printf("[allocfail] %u bytes caps=0x%x in %s; largest=%u free=%u\n", (unsigned)size,
               (unsigned)caps, fn ? fn : "?", (unsigned)heap_caps_get_largest_free_block(caps),
               (unsigned)heap_caps_get_free_size(caps));
}

void installFailHook() { heap_caps_register_failed_alloc_callback(onAllocFail); }

}
