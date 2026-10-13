// Task 1.2 in C++: split an Uthmani word into letter units (base character + combining marks).
// Lossless parsing, no interpretation. Port of tools/phonemizer/uthmani.py.
#ifndef MUTQIN_TAJWEED_UTHMANI_H
#define MUTQIN_TAJWEED_UTHMANI_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace mutqin {

// Code points used by the phonemizer.
constexpr uint32_t kFathatan = 0x064B, kDammatan = 0x064C, kKasratan = 0x064D;
constexpr uint32_t kFatha = 0x064E, kDamma = 0x064F, kKasra = 0x0650;
constexpr uint32_t kShadda = 0x0651, kSukun = 0x0652, kMaddahAbove = 0x0653, kHamzaAbove = 0x0654;
constexpr uint32_t kSuperscriptAlef = 0x0670;

struct UthmaniParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct LetterUnit {
    uint32_t ch = 0;              // base code point
    std::vector<uint32_t> marks;  // combining marks in source order
    int start = 0;                // [start, end) code point offsets inside the word text
    int end = 0;

    bool Has(uint32_t codepoint) const;
};

bool IsBaseCharacter(uint32_t cp);
bool IsCombiningMark(uint32_t cp);

// Throws UthmaniParseError for a mark without a base or a character outside the verified text.
std::vector<LetterUnit> ParseWord(const std::vector<uint32_t>& codepoints);

}  // namespace mutqin

#endif  // MUTQIN_TAJWEED_UTHMANI_H
