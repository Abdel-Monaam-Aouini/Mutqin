// Phoneme inventory (docs/PHONEME_INVENTORY.md), C++ port of tools/phonemizer/inventory.py.
// Symbol order is part of the contract: token ids derive from it.
#ifndef MUTQIN_TAJWEED_INVENTORY_H
#define MUTQIN_TAJWEED_INVENTORY_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace mutqin {

enum class PhonemeKind { kConsonant, kHamza, kVowel };

struct PhonemeDef {
    const char* symbol;  // stable ASCII symbol = acoustic model token
    PhonemeKind kind;
    uint32_t codepoint;  // base Arabic letter, 0 for vowels
};

constexpr size_t kPhonemeCount = 31;  // 27 consonants + hamza + 3 short vowels

// The inventory in contract order (hamza first, then alphabetical, then a i u).
const PhonemeDef* Phonemes();

// Symbol for a base letter code point, or nullptr if the letter has none (alef, wasla...).
const char* SymbolForLetter(uint32_t codepoint);

// CTC token id: <blank> = 0, then the symbols in contract order. -1 for an unknown symbol.
int TokenId(const std::string& symbol);

// Where a phoneme applies (spec section 1 plus start_only / after_continue, see PHONEME_INVENTORY.md).
enum class Variant { kBoth, kContinueOnly, kWaqfOnly, kStartOnly, kAfterContinue };
const char* VariantName(Variant v);

}  // namespace mutqin

#endif  // MUTQIN_TAJWEED_INVENTORY_H
