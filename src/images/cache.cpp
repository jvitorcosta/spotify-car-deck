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
    Serial.printf("[cache] SD %s\n", ready ? "ready" : "unavailable");
    return ready;
}

String spritePath(int dex) { return "/sprites/" + String(dex) + ".png"; }

bool has(int dex) { return ready && SD.exists(spritePath(dex)); }

bool save(int dex, const uint8_t* data, size_t n) {
    if (!ready) return false;
    fs::File f = SD.open(spritePath(dex), FILE_WRITE);
    if (!f) return false;
    f.write(data, n);
    f.close();
    return true;
}

fs::File open(int dex) { return ready ? SD.open(spritePath(dex), FILE_READ) : fs::File(); }

}
