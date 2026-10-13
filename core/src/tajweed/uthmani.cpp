#include "tajweed/uthmani.h"

#include <algorithm>
#include <cstdio>

namespace mutqin {

bool LetterUnit::Has(uint32_t codepoint) const {
    return std::find(marks.begin(), marks.end(), codepoint) != marks.end();
}

bool IsBaseCharacter(uint32_t cp) {
    return cp == 0x0621 || cp == 0x0671                // hamza, alef wasla
           || (cp >= 0x0623 && cp <= 0x063A)           // alef/waw/yeh with hamza ... ghain
           || (cp >= 0x0641 && cp <= 0x064A)           // feh ... yeh
           || cp == 0x0640                             // tatweel
           || cp == 0x06E5 || cp == 0x06E6;            // small waw, small yeh
}

bool IsCombiningMark(uint32_t cp) {
    return (cp >= 0x064B && cp <= 0x0654) || cp == 0x0670 || cp == 0x06DC || cp == 0x06DF ||
           cp == 0x06E0 || cp == 0x06E2 || cp == 0x06E3 || cp == 0x06E7 || cp == 0x06E8 ||
           (cp >= 0x06EA && cp <= 0x06ED);
}

std::vector<LetterUnit> ParseWord(const std::vector<uint32_t>& cps) {
    std::vector<LetterUnit> units;
    LetterUnit cur;
    bool open = false;
    for (size_t i = 0; i < cps.size(); ++i) {
        const uint32_t cp = cps[i];
        if (IsBaseCharacter(cp)) {
            if (open) {
                cur.end = static_cast<int>(i);
                units.push_back(cur);
            }
            cur = LetterUnit();
            cur.ch = cp;
            cur.start = static_cast<int>(i);
            open = true;
        } else if (IsCombiningMark(cp)) {
            if (!open) {
                char buf[64];
                std::snprintf(buf, sizeof buf, "mark U+%04X at offset %zu has no base", cp, i);
                throw UthmaniParseError(buf);
            }
            cur.marks.push_back(cp);
        } else {
            char buf[64];
            std::snprintf(buf, sizeof buf, "unexpected character U+%04X at offset %zu", cp, i);
            throw UthmaniParseError(buf);
        }
    }
    if (open) {
        cur.end = static_cast<int>(cps.size());
        units.push_back(cur);
    }
    return units;
}

}  // namespace mutqin
