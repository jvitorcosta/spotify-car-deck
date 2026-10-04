#include "theme.h"
#include <cstring>
#include <cctype>

namespace theme {
struct TypeColor { const char* name; uint16_t color; };
static const TypeColor TABLE[] = {
    {"normal",0xC618},{"fire",0xF9A0},{"water",0x6B0D},{"electric",0xF721},
    {"grass",0xA9A5},{"ice",0x9EFF},{"fighting",0xB143},{"poison",0xA976},
    {"ground",0xE51C},{"flying",0xAD9D},{"psychic",0xFCAB},{"bug",0xA960},
    {"rock",0xB986},{"ghost",0x71D6},{"dragon",0x4817},{"dark",0x5967},
    {"steel",0xC618},{"fairy",0xF576},
};
static char lower(char c){ return (char)std::tolower((unsigned char)c); }

uint16_t typeColor(const char* type){
    if(!type) return GBA_NAVY;
    for(const auto& e : TABLE){
        const char* a=type; const char* b=e.name; bool eq=true;
        while(*a && *b){ if(lower(*a)!=*b){eq=false;break;} ++a;++b; }
        if(eq && *a=='\0' && *b=='\0') return e.color;
    }
    return GBA_NAVY;
}
}
