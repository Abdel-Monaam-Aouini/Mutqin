// Verified Quran text: surah -> ayah -> word, with stable IDs "surah:ayah:word" (1-based).
//
// Loads the Tanzil files from data/quran/ (read-only), verifies SHA-256 against checksums.txt,
// and applies exactly the same parsing rules as tools/quran_text/loader.py (the reference).
// No Quran text is ever stored as a literal in code.
#ifndef MUTQIN_TEXT_QURAN_H
#define MUTQIN_TEXT_QURAN_H

#include <cstddef>
#include <string>
#include <vector>

namespace mutqin {

struct Word {
    int surah = 0;
    int ayah = 0;
    int index = 0;                   // 1-based, basmala prefix not counted
    std::string text;                // exactly the source token (UTF-8)
    std::vector<std::string> marks;  // standalone waqf / sajdah signs following this word

    std::string Id() const;  // "surah:ayah:word"
};

struct Ayah {
    int surah = 0;
    int ayah = 0;
    std::vector<Word> words;
    std::string simple_text;  // Tanzil simple-clean text, for matching only
};

struct Surah {
    int number = 0;
    std::vector<std::string> basmala;  // prefix tokens of ayah 1 (empty for surah 1 and 9)
    std::vector<Ayah> ayahs;
};

class Quran {
public:
    // Verifies checksums and loads `data_dir`. On failure returns false and sets *error.
    static bool Load(const std::string& data_dir, Quran* out, std::string* error);

    const std::vector<Surah>& surahs() const { return surahs_; }
    size_t ayah_count() const;
    // nullptr if (surah, ayah) does not exist.
    const Ayah* FindAyah(int surah, int ayah) const;

    // Deterministic text dump compared against tools/quran_text/dump.py in the parity test.
    std::string DumpTsv() const;

private:
    std::vector<Surah> surahs_;
};

// True if `token` (UTF-8) consists only of Quranic marks (waqf / sajdah signs...), i.e. it is
// not a word. Code point ranges are identical to MARK_RANGES in the Python loader.
bool IsMarkOnly(const std::string& token);

}  // namespace mutqin

#endif  // MUTQIN_TEXT_QURAN_H
