#include "lrc.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace lrc {
// Parse one "[mm:ss.xx]text" line. Returns false if no valid tag.
static bool parseLine(const std::string& line, LrcLine& out) {
    if (line.size() < 10 || line[0] != '[') return false;
    size_t close = line.find(']');
    if (close == std::string::npos) return false;
    std::string tag = line.substr(1, close - 1); // mm:ss.xx
    size_t colon = tag.find(':');
    size_t dot = tag.find('.');
    if (colon == std::string::npos) return false;
    char* end = nullptr;
    long mm = std::strtol(tag.c_str(), &end, 10);
    if (end != tag.c_str() + colon) return false;
    long ss = std::strtol(tag.c_str() + colon + 1, &end, 10);
    long cs = 0;
    if (dot != std::string::npos) cs = std::strtol(tag.c_str() + dot + 1, nullptr, 10);
    if (mm < 0 || ss < 0 || ss > 59) return false;
    out.tMs = (uint32_t)(mm * 60000 + ss * 1000 + cs * 10);
    out.text = line.substr(close + 1);
    // trim trailing CR
    while (!out.text.empty() && (out.text.back() == '\r' || out.text.back() == '\n'))
        out.text.pop_back();
    return true;
}

std::vector<LrcLine> parse(const std::string& synced) {
    std::vector<LrcLine> out;
    std::istringstream ss(synced);
    std::string line;
    while (std::getline(ss, line)) {
        LrcLine l;
        if (parseLine(line, l)) out.push_back(l);
    }
    std::sort(out.begin(), out.end(),
              [](const LrcLine& a, const LrcLine& b){ return a.tMs < b.tMs; });
    return out;
}

int currentIndex(const std::vector<LrcLine>& lines, uint32_t posMs) {
    int idx = -1;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].tMs <= posMs) idx = (int)i; else break;
    }
    return idx;
}
}
