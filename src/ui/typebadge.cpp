#include "typebadge.h"
#include <cctype>
#include <cstring>
#include "theme.h"
#include "../util/text.h"

namespace typebadge {

bool label(const char* type, char* out, size_t n) {
    if (n) out[0] = '\0';
    if (!type || !type[0]) return false;
    for (int k = 0; k < theme::typeCount(); ++k) {   // the 18 names live in theme only
        const char* t = theme::typeName(k);
        if (!txt::equalsIgnoreCase(type, t)) continue;
        size_t len = strlen(t);
        if (len + 1 > n) return false;
        for (size_t i = 0; i <= len; ++i) out[i] = (char)std::toupper((unsigned char)t[i]);
        return true;
    }
    return false;
}

}
