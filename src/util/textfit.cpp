#include "textfit.h"

namespace textfit {

// Fits s into maxW, appending "..." when cut; *keep = number of chars kept from s.
static std::string ellipsize(std::string s, int maxW, WidthFn width, void* ctx, size_t* keep) {
    if (width(s, ctx) <= maxW) { *keep = s.size(); return s; }
    while (!s.empty() && width(s + "...", ctx) > maxW) s.pop_back();
    *keep = s.size();
    return s + "...";
}

TwoLines wrapTwo(const std::string& s, int maxW, WidthFn width, void* ctx) {
    if (s.empty() || width(s, ctx) <= maxW) return {s, ""};
    // longest prefix that fits
    size_t fit = 0;
    while (fit < s.size() && width(s.substr(0, fit + 1), ctx) <= maxW) ++fit;
    size_t cut = s.rfind(' ', fit);
    size_t next;
    if (cut == std::string::npos || cut == 0) { cut = fit; next = fit; }   // hard split
    else next = cut + 1;
    TwoLines out;
    out.a = s.substr(0, cut);
    out.bStart = next;
    out.b = ellipsize(s.substr(next), maxW, width, ctx, &out.bKeep);
    return out;
}

}
