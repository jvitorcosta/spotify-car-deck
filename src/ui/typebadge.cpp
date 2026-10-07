#include "typebadge.h"
#include <cctype>
#include <cstring>

namespace typebadge {

static const char* const TYPES[] = {"normal", "fire", "water", "electric", "grass", "ice",
                                    "fighting", "poison", "ground", "flying", "psychic", "bug",
                                    "rock", "ghost", "dragon", "dark", "steel", "fairy"};

static bool sameIgnoreCase(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b)
        if (std::tolower((unsigned char)*a) != *b) return false;
    return *a == '\0' && *b == '\0';
}

bool label(const char* type, char* out, size_t n) {
    if (n) out[0] = '\0';
    if (!type || !type[0]) return false;
    for (const char* t : TYPES) {
        if (!sameIgnoreCase(type, t)) continue;
        size_t len = strlen(t);
        if (len + 1 > n) return false;
        for (size_t i = 0; i <= len; ++i) out[i] = (char)std::toupper((unsigned char)t[i]);
        return true;
    }
    return false;
}

}
