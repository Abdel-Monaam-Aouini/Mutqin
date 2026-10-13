// BuildWordReadings (C++) against tools/prototype/dump_words.py for every word of the Quran.
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <map>
#include <string>

#include "align/ctc.h"
#include "text/quran.h"

using namespace mutqin;

namespace {
std::string Join(const std::vector<std::string>& v) {
    std::string out;
    for (const std::string& s : v)
        out += (out.empty() ? "" : " ") + s;
    return out;
}
}  // namespace

TEST(WordsParity, CppEqualsPython) {
    const char* path = std::getenv("MUTQIN_PY_WORDS_DUMP");
    ASSERT_NE(path, nullptr);
    Quran quran;
    std::string error;
    ASSERT_TRUE(Quran::Load(MUTQIN_DATA_DIR, &quran, &error)) << error;

    std::vector<const Ayah*> ayahs;
    std::map<std::pair<int, int>, int> lens;
    for (const Surah& s : quran.surahs())
        for (const Ayah& a : s.ayahs) {
            ayahs.push_back(&a);
            lens[{a.surah, a.ayah}] = static_cast<int>(a.words.size());
        }
    PhonemizeResult result;
    ASSERT_TRUE(PhonemizeRange(ayahs, &result, &error)) << error;

    std::string want;
    {
        std::ifstream in(path);
        ASSERT_TRUE(in.good());
        std::string line;
        while (std::getline(in, line))
            want += line + "\n";
    }
    std::string got;
    for (const WordRef& w : BuildWordReadings(result.items, lens)) {
        got += std::to_string(std::get<0>(w.key)) + ":" + std::to_string(std::get<1>(w.key)) + ":" +
               std::to_string(std::get<2>(w.key)) + "\t" + Join(w.reading.cont);
        for (const auto& c : w.reading.contexts)
            got += "|" + Join(c);
        got += "\n";
    }
    EXPECT_TRUE(got == want) << "word readings differ (first lines of each):\n"
                             << got.substr(0, 200) << "\n---\n"
                             << want.substr(0, 200);
}
