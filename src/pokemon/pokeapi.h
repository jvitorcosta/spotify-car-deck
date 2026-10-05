#pragma once
#include "app_state.h"

// Random Pokémon pick: id + name + first type + sprite URL via PokeAPI.
namespace pokeapi {

// Picks a random national-dex id in [1,1025] (esp_random()), fetches
// https://pokeapi.co/api/v2/pokemon/<id> with an ArduinoJson filter that
// only keeps `name` and `types[0].type.name`, and fills st.pokedexNum /
// st.pokeName / st.pokeType / st.pokeSpriteUrl. Returns true on success;
// st is left unmodified on failure.
bool pickRandom(AppState& st);

}
