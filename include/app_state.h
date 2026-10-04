#pragma once
#include <Arduino.h>

enum class PlaybackStatus { Unknown, Playing, Paused, Stopped, Offline };

struct AppState {
    char trackName[96]; char artist[96]; char album[96]; char context[64];
    char albumArtUrl[160];
    uint32_t progressMs; uint32_t durationMs; uint32_t lastPollMs;
    bool isPlaying; bool shuffle; int repeat; int popularity; int volume;
    char deviceName[48];
    int pokedexNum; char pokeName[24]; char pokeType[16]; char pokeSpriteUrl[160];
    PlaybackStatus status;
};
