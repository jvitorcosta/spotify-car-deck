#include "cache.h"
#include <SD.h>
#include <SPI.h>
#include "pins.h"

namespace cache {

static SPIClass sdSPI(VSPI);
static bool ready = false;

bool begin() {
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    ready = SD.begin(SD_CS, sdSPI);
    if (ready && !SD.exists("/sprites")) SD.mkdir("/sprites");
    if (ready && !SD.exists("/pmd")) SD.mkdir("/pmd");
    Serial.printf("[cache] SD %s\n", ready ? "ready" : "unavailable");
    return ready;
}

bool hasPath(const String& path) { return ready && SD.exists(path); }

bool savePath(const String& path, const uint8_t* data, size_t n) {
    if (!ready) return false;
    fs::File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    size_t w = f.write(data, n);
    f.close();
    if (w != n) { SD.remove(path); return false; }   // don't leave a truncated file
    return true;
}

bool readAll(const String& path, size_t maxLen, uint8_t** out, size_t* outLen) {
    *out = nullptr;
    *outLen = 0;
    if (!ready) return false;
    fs::File f = SD.open(path, FILE_READ);
    if (!f) return false;
    size_t n = f.size();
    if (n == 0 || n > maxLen) { f.close(); return false; }
    uint8_t* data = (uint8_t*)malloc(n + 1);
    if (!data) { f.close(); return false; }
    size_t got = f.read(data, n);
    f.close();
    if (got != n) { free(data); return false; }
    data[n] = 0;
    *out = data;
    *outLen = n;
    return true;
}

void removePath(const String& path) { if (ready) SD.remove(path); }

String spritePath(int dex) { return "/sprites/" + String(dex) + ".png"; }
bool has(int dex) { return hasPath(spritePath(dex)); }
bool save(int dex, const uint8_t* data, size_t n) { return savePath(spritePath(dex), data, n); }
fs::File open(int dex) { return ready ? SD.open(spritePath(dex), FILE_READ) : fs::File(); }

}
