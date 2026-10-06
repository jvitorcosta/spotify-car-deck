#pragma once
#include "app_state.h"
namespace pick {
// Sets st's Pokémon fields (number, name, type, fallback sprite URL) from the bundled dex.
void choose(AppState& st, int n);
}
