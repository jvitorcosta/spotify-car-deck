#include "cache.h"
#include <SD.h>
#include <SPI.h>
#include "pins.h"

namespace cache {

static SPIClass s_sdSPI(VSPI);
static bool s_ready = false;

bool begin() {
    s_sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    s_ready = SD.begin(SD_CS, s_sdSPI, 4000000, "/sd", 1);   // one file open at a time: the default 5 cost heap
    if (s_ready && !SD.exists("/sprites")) SD.mkdir("/sprites");
    if (s_ready && !SD.exists("/pmd")) SD.mkdir("/pmd");
    Serial.printf("[cache] SD %s\n", s_ready ? "ready" : "unavailable");
    return s_ready;
}

bool savePath(const String& path, const uint8_t* data, size_t n) {
    if (!s_ready) return false;
    fs::File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    size_t w = f.write(data, n);
    f.close();
    if (w != n) { SD.remove(path); return false; }   // don't leave a truncated file
    return true;
}

bool readInto(const String& path, uint8_t* buf, size_t cap, size_t* outLen) {
    *outLen = 0;
    if (!s_ready) return false;
    fs::File f = SD.open(path, FILE_READ);
    if (!f) return false;
    size_t n = f.size();
    if (n == 0 || n + 1 > cap) { f.close(); return false; }
    size_t got = f.read(buf, n);
    f.close();
    if (got != n) return false;
    buf[n] = 0;
    *outLen = n;
    return true;
}

void removePath(const String& path) { if (s_ready) SD.remove(path); }

String spritePath(int dex) { return "/sprites/" + String(dex) + ".png"; }
bool save(int dex, const uint8_t* data, size_t n) { return savePath(spritePath(dex), data, n); }

}
