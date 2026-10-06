#include "artmap.h"
#include <cstring>

namespace artmap {

Map cover(int srcW, int srcH, int outW, int outH) {
    int side = srcW < srcH ? srcW : srcH;
    return {(srcW - side) / 2, (srcH - side) / 2, side, outW, outH};
}

int pickScale(int w, int h, int minSide) {
    int s = w < h ? w : h;
    for (int k = 3; k > 0; --k)
        if ((s >> k) >= minSide) return k;
    return 0;
}

void plainHttpUrl(const char* in, char* out, size_t len) {
    static const char* PFX = "https://i.scdn.co/";
    if (!len) return;
    const char* src = in ? in : "";
    if (strncmp(src, PFX, strlen(PFX)) == 0) {
        strncpy(out, "http://", len - 1);
        out[len - 1] = '\0';
        size_t used = strlen(out);
        if (used < len - 1) strncat(out, src + 8, len - 1 - used);   // skip "https://"
    } else {
        strncpy(out, src, len - 1);
        out[len - 1] = '\0';
    }
}

}
