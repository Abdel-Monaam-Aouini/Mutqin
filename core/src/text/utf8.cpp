#include "text/utf8.h"

namespace mutqin {

bool DecodeUtf8(const std::string& s, std::vector<uint32_t>* out) {
    out->clear();
    size_t i = 0;
    while (i < s.size()) {
        const auto b0 = static_cast<uint8_t>(s[i]);
        uint32_t cp;
        size_t n;
        if (b0 < 0x80) { cp = b0; n = 1; }
        else if ((b0 & 0xE0) == 0xC0) { cp = b0 & 0x1F; n = 2; }
        else if ((b0 & 0xF0) == 0xE0) { cp = b0 & 0x0F; n = 3; }
        else if ((b0 & 0xF8) == 0xF0) { cp = b0 & 0x07; n = 4; }
        else return false;
        if (i + n > s.size()) return false;
        for (size_t k = 1; k < n; ++k) {
            const auto b = static_cast<uint8_t>(s[i + k]);
            if ((b & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (b & 0x3F);
        }
        static const uint32_t kMin[] = {0, 0, 0x80, 0x800, 0x10000};
        if (cp < kMin[n] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        out->push_back(cp);
        i += n;
    }
    return true;
}

}  // namespace mutqin
