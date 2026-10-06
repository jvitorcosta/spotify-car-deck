#pragma once
#include <string>
// Two-line greedy wrap measured by a caller-supplied width function (so the same
// logic serves the TFT fonts on device and a fake font in tests). PURE.
namespace textfit {
struct TwoLines { std::string a, b; };
using WidthFn = int (*)(const std::string&, void* ctx);
TwoLines wrapTwo(const std::string& s, int maxW, WidthFn width, void* ctx);
}
