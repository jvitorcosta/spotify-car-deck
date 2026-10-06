#include "pick.h"
#include <string.h>
#include "dex.h"

namespace pick {

void choose(AppState& st, int n) {
    st.pokedexNum = n;
    strncpy(st.pokeName, dex::name(n), sizeof(st.pokeName) - 1);
    st.pokeName[sizeof(st.pokeName) - 1] = '\0';
    strncpy(st.pokeType, dex::type(n), sizeof(st.pokeType) - 1);
    st.pokeType[sizeof(st.pokeType) - 1] = '\0';
    dex::spriteUrl(n, st.pokeSpriteUrl, sizeof(st.pokeSpriteUrl));
    Serial.printf("[poke] #%d %s (%s)\n", n, st.pokeName, st.pokeType);
}

}
