#include "walkrect.h"
namespace walkrect {
Plan plan(Rect prev, Rect cur, int slack) {
    Plan p{cur, false, cur};
    if (prev.w <= 0) return p;
    bool near = prev.x >= cur.x - slack && prev.x + prev.w <= cur.x + cur.w + slack &&
                prev.y >= cur.y - slack && prev.y + prev.h <= cur.y + cur.h + slack;
    if (!near) { p.clearPrev = true; return p; }
    int x0 = prev.x < cur.x ? prev.x : cur.x;
    int y0 = prev.y < cur.y ? prev.y : cur.y;
    int x1 = (prev.x + prev.w > cur.x + cur.w) ? prev.x + prev.w : cur.x + cur.w;
    int y1 = (prev.y + prev.h > cur.y + cur.h) ? prev.y + prev.h : cur.y + cur.h;
    p.push = {x0, y0, x1 - x0, y1 - y0};
    return p;
}
}
