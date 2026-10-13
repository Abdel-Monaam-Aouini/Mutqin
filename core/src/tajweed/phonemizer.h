// Reference phonemizer in C++: Uthmani ayah range -> expected phoneme items.
// Port of tools/phonemizer/phonemize.py (the Python version is the source of truth; a parity test
// compares both over the whole Quran). PROVISIONAL: the rules follow
// docs/DRAFT_ORTHOGRAPHY_TABLES.md and have not been verified by a teacher.
#ifndef MUTQIN_TAJWEED_PHONEMIZER_H
#define MUTQIN_TAJWEED_PHONEMIZER_H

#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "tajweed/inventory.h"
#include "text/quran.h"

namespace mutqin {

enum class Role { kCons, kVowel, kTanweenN };
const char* RoleName(Role r);

struct PhonemeItem {
    std::string ph;
    int word = 0;        // 1-based word index inside the ayah
    int span_start = 0;  // [start, end) code point offsets inside the word
    int span_end = 0;
    Role role = Role::kCons;
    std::vector<int> dur;            // allowed durations in counts, empty = none
    std::vector<std::string> rules;  // rule tags in the order they were added
    Variant variant = Variant::kBoth;
    int surah = 0;
    int ayah = 0;
    std::set<uint32_t> host_marks;  // marks on the host letter
    bool maddah = false;            // a maddah sign was written on / after this vowel
    uint32_t long_by = 0;           // letter / sign that made this vowel long
    std::set<std::string> flags;    // "iwad", "alef_waqf": stop-only lengthening
    std::set<uint32_t> signs;       // special signs on the host (iqlab meem, location marks)
    uint32_t letter = 0;            // code point of the host letter
};

struct Diagnostic {
    std::string word_id;
    std::string kind;
    int offset = 0;
};

struct PhonemizeResult {
    std::vector<PhonemeItem> items;
    std::vector<Diagnostic> diagnostics;
};

using WordKey = std::tuple<int, int, int>;  // (surah, ayah, word)
WordKey KeyOf(const PhonemeItem& item);

// Phonemizes consecutive ayahs; rules that look at the next word also see the next ayah.
// Returns false and sets *error if a word contains a character outside the verified text.
bool PhonemizeRange(const std::vector<const Ayah*>& ayahs, PhonemizeResult* out, std::string* error);

// Indices of the items of one concrete reading. `stops` holds the words the reciter stops at
// (all_stops = every word); a surah end is always a stop, the first word starts a recitation.
std::vector<size_t> SelectReading(const std::vector<PhonemeItem>& items, const std::set<WordKey>& stops,
                                  bool all_stops);

// Deterministic text forms used by the parity test against tools/phonemizer/dump.py.
std::string DumpResult(const PhonemizeResult& result);
std::string DumpParity(const Quran& quran);

}  // namespace mutqin

#endif  // MUTQIN_TAJWEED_PHONEMIZER_H
