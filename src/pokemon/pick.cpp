#include "pick.h"
#include "../util/text.h"
#include <string.h>
#include "dex.h"

namespace pick {

void choose(AppState& st, int n) {
    st.pokedexNum = n;
    txt::copy(st.pokeName, dex::name(n), sizeof(st.pokeName));
    txt::copy(st.pokeType, dex::type(n), sizeof(st.pokeType));
    dex::spriteUrl(n, st.pokeSpriteUrl, sizeof(st.pokeSpriteUrl));
    Serial.printf("[poke] #%d %s (%s)\n", n, st.pokeName, st.pokeType);
}

}
