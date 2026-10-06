#include "pokeapi.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <cctype>

namespace pokeapi {

bool pickRandom(AppState& st) {
    // Retry a few times: parsing directly from the HTTP stream occasionally
    // fails (chunked transfer / hiccup), which previously showed the name as "?".
    for (int attempt = 0; attempt < 3; ++attempt) {
        int id = (int)(esp_random() % 1025) + 1;    // 1..1025
        char url[80];
        snprintf(url, sizeof(url), "https://pokeapi.co/api/v2/pokemon/%d", id);

        WiFiClientSecure client; client.setInsecure();
        HTTPClient https;
        https.useHTTP10(true);   // no chunking -> the raw stream is plain JSON
        if (!https.begin(client, url)) continue;
        int rc = https.GET();
        if (rc != 200) { Serial.printf("[poke] GET rc=%d (retry)\n", rc); https.end(); continue; }

        // Stream-parse through a filter (name + first type) so the multi-hundred-KB
        // body is never held in RAM; a full getString() fails on a fragmented heap.
        JsonDocument filter;
        filter["name"] = true;
        filter["types"][0]["type"]["name"] = true;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, https.getStream(),
                                                   DeserializationOption::Filter(filter));
        https.end();
        if (err) { Serial.printf("[poke] json err %s (retry)\n", err.c_str()); continue; }

        const char* nm = doc["name"] | "";
        if (!nm[0]) continue;   // empty name -> retry

        st.pokedexNum = id;
        strncpy(st.pokeName, nm, sizeof(st.pokeName) - 1);
        st.pokeName[sizeof(st.pokeName) - 1] = '\0';
        st.pokeName[0] = (char)toupper((unsigned char)st.pokeName[0]);  // Pikachu, not pikachu
        strncpy(st.pokeType, doc["types"][0]["type"]["name"] | "normal", sizeof(st.pokeType) - 1);
        st.pokeType[sizeof(st.pokeType) - 1] = '\0';
        snprintf(st.pokeSpriteUrl, sizeof(st.pokeSpriteUrl),
            "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/%d.png", id);
        Serial.printf("[poke] #%d %s (%s)\n", id, st.pokeName, st.pokeType);
        return true;
    }
    Serial.println("[poke] pickRandom failed after retries");
    return false;
}

}
