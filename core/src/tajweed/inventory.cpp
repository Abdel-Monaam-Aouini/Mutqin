#include "tajweed/inventory.h"

#include <map>

namespace mutqin {
namespace {

constexpr PhonemeDef kDefs[kPhonemeCount] = {
    {"hz", PhonemeKind::kHamza, 0x0621},     {"b", PhonemeKind::kConsonant, 0x0628},
    {"t", PhonemeKind::kConsonant, 0x062A},  {"th", PhonemeKind::kConsonant, 0x062B},
    {"j", PhonemeKind::kConsonant, 0x062C},  {"H", PhonemeKind::kConsonant, 0x062D},
    {"kh", PhonemeKind::kConsonant, 0x062E}, {"d", PhonemeKind::kConsonant, 0x062F},
    {"dh", PhonemeKind::kConsonant, 0x0630}, {"r", PhonemeKind::kConsonant, 0x0631},
    {"z", PhonemeKind::kConsonant, 0x0632},  {"s", PhonemeKind::kConsonant, 0x0633},
    {"sh", PhonemeKind::kConsonant, 0x0634}, {"S", PhonemeKind::kConsonant, 0x0635},
    {"D", PhonemeKind::kConsonant, 0x0636},  {"T", PhonemeKind::kConsonant, 0x0637},
    {"Z", PhonemeKind::kConsonant, 0x0638},  {"E", PhonemeKind::kConsonant, 0x0639},
    {"gh", PhonemeKind::kConsonant, 0x063A}, {"f", PhonemeKind::kConsonant, 0x0641},
    {"q", PhonemeKind::kConsonant, 0x0642},  {"k", PhonemeKind::kConsonant, 0x0643},
    {"l", PhonemeKind::kConsonant, 0x0644},  {"m", PhonemeKind::kConsonant, 0x0645},
    {"n", PhonemeKind::kConsonant, 0x0646},  {"h", PhonemeKind::kConsonant, 0x0647},
    {"w", PhonemeKind::kConsonant, 0x0648},  {"y", PhonemeKind::kConsonant, 0x064A},
    {"a", PhonemeKind::kVowel, 0},           {"i", PhonemeKind::kVowel, 0},
    {"u", PhonemeKind::kVowel, 0},
};

}  // namespace

const PhonemeDef* Phonemes() {
    return kDefs;
}

const char* SymbolForLetter(uint32_t codepoint) {
    for (const PhonemeDef& d : kDefs) {
        if (d.codepoint != 0 && d.codepoint == codepoint)
            return d.symbol;
    }
    return nullptr;
}

int TokenId(const std::string& symbol) {
    if (symbol == "<blank>")
        return 0;
    for (size_t i = 0; i < kPhonemeCount; ++i) {
        if (symbol == kDefs[i].symbol)
            return static_cast<int>(i) + 1;
    }
    return -1;
}

const char* VariantName(Variant v) {
    switch (v) {
        case Variant::kBoth:
            return "both";
        case Variant::kContinueOnly:
            return "continue_only";
        case Variant::kWaqfOnly:
            return "waqf_only";
        case Variant::kStartOnly:
            return "start_only";
        case Variant::kAfterContinue:
            return "after_continue";
    }
    return "";
}

}  // namespace mutqin
