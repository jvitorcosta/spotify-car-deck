#include "textfit.h"

namespace textfit {

static std::string ellipsize(std::string s, int maxW, WidthFn width, void* ctx) {
    if (width(s, ctx) <= maxW) return s;
    while (!s.empty() && width(s + "...", ctx) > maxW) s.pop_back();
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
    return {s.substr(0, cut), ellipsize(s.substr(next), maxW, width, ctx)};
}

}
