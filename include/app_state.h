#pragma once
#include <Arduino.h>

enum class PlaybackStatus { Unknown, Playing, Paused, Stopped, Offline };

struct AppState {
    char trackName[96]; char artist[96]; char album[96]; char context[64];
    char albumArtUrl[160];
    uint32_t progressMs; uint32_t durationMs; uint32_t lastPollMs;
    bool isPlaying; bool shuffle;
    int repeat;            // 0 = off, 1 = context, 2 = track (zero-init = off)
    int popularity; int volume;
    char deviceName[48];
    char deviceType[16];   // Spotify device type, e.g. "Smartphone"
    int pokedexNum; char pokeName[24]; char pokeType[16]; char pokeSpriteUrl[160];
    uint32_t trackGen;     // increments on every track change (network task); 0 = none yet
    PlaybackStatus status;
};
