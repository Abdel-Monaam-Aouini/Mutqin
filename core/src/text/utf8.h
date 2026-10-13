// Strict UTF-8 decoding shared by the text and tajweed modules.
#ifndef MUTQIN_TEXT_UTF8_H
#define MUTQIN_TEXT_UTF8_H

#include <cstdint>
#include <string>
#include <vector>

namespace mutqin {

// Decodes `s` into code points. Rejects overlong forms, surrogates and values above U+10FFFF.
bool DecodeUtf8(const std::string& s, std::vector<uint32_t>* out);

}  // namespace mutqin

#endif  // MUTQIN_TEXT_UTF8_H
