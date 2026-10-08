#include "genrebadge.h"
#include "theme.h"

namespace genrebadge {

using theme::rgb;
struct IdBadge { uint32_t id; uint8_t badge; };
#include "genre_data.inc"

static constexpr int BADGE_N = (int)(sizeof(BADGES) / sizeof(BADGES[0]));
static constexpr int ID_N = (int)(sizeof(IDS) / sizeof(IDS[0]));

int count() { return BADGE_N; }
const Badge& at(int index) { return BADGES[index]; }
int idCount() { return ID_N; }
uint32_t idAt(int i) { return IDS[i].id; }

uint8_t forGenreId(uint32_t id) {
    int lo = 0, hi = ID_N - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (IDS[mid].id < id) lo = mid + 1;
        else if (IDS[mid].id > id) hi = mid - 1;
        else return IDS[mid].badge;
    }
    return (uint8_t)(BADGE_N - 1);   // "???"
}

}
