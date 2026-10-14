// Expected units of the Quran phoneme Zipformer (Muno459/zipformer_p-quran): groups of consecutive
// Tanzil words with the model's canonical unit sequence, read from the table written by
// tools/zipformer/expected_units.py (the table is derived from the model's files and lives with them).
#ifndef MUTQIN_INFERENCE_UNITS_H
#define MUTQIN_INFERENCE_UNITS_H

#include <string>
#include <vector>

namespace mutqin {

struct UnitGroup {
    int surah = 0;
    int ayah = 0;
    int first_word = 0;  // 1-based, inclusive
    int last_word = 0;
    std::vector<std::string> units;
};

// Reads the groups of ayahs [ayah_first, ayah_last] of `surah` from the table (one line per group:
// "surah:ayah<TAB>first-last<TAB>unit unit ..."). Fails if the file is missing, malformed, or has no
// group for one of the ayahs.
bool LoadUnitGroups(const std::string& path, int surah, int ayah_first, int ayah_last,
                    std::vector<UnitGroup>* out, std::string* error);

// Cuts every run of the same character (code point) longer than `keep` down to `keep`, so units that
// differ only in elongation compare equal (a reciter's madd length is not a mistake here). keep <= 0
// returns the unit unchanged.
std::string CollapseRuns(const std::string& unit, int keep);

}  // namespace mutqin

#endif  // MUTQIN_INFERENCE_UNITS_H
