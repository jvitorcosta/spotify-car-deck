#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace lrc {
struct LrcLine { uint32_t tMs; std::string text; };
std::vector<LrcLine> parse(const std::string& synced);
int currentIndex(const std::vector<LrcLine>& lines, uint32_t posMs);
}
