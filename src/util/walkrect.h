#pragma once
// Dirty-rect bookkeeping for the walking-Pokemon blit (PURE, host-tested).
namespace walkrect {
struct Rect { int x, y, w, h; };   // w == 0 means "nothing drawn yet"
struct Plan {
    Rect push;       // rect to compose + push this frame
    bool clearPrev;  // true: prev is too far away, clear it separately first
    Rect next;       // what to pass as `prev` next frame (the sprite rect)
};
// Covers last frame's sprite rect in this frame's push when it lies within
// `slack` px, so a moving walker erases its own trail without flicker.
Plan plan(Rect prev, Rect cur, int slack);
}
