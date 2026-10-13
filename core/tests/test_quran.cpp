// Quran loader tests. Text is only read from data/quran/ (run tools/quran_text/download.py first);
// no Quran text is written in this file.
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "text/quran.h"

namespace fs = std::filesystem;

namespace {

const std::string kDataDir = MUTQIN_DATA_DIR;

bool HaveData() {
    return fs::exists(fs::path(kDataDir) / "checksums.txt") &&
           fs::exists(fs::path(kDataDir) / "quran-uthmani.txt") &&
           fs::exists(fs::path(kDataDir) / "quran-simple-clean.txt");
}

#define REQUIRE_DATA() \
    if (!HaveData())   \
    GTEST_SKIP() << "data/quran missing: run tools/quran_text/download.py"

const mutqin::Quran& Loaded() {
    static mutqin::Quran quran;
    static bool ok = [] {
        std::string error;
        return mutqin::Quran::Load(kDataDir, &quran, &error);
    }();
    EXPECT_TRUE(ok);
    return quran;
}

}  // namespace

TEST(QuranLoader, Counts) {
    REQUIRE_DATA();
    const auto& q = Loaded();
    EXPECT_EQ(q.surahs().size(), 114u);
    EXPECT_EQ(q.ayah_count(), 6236u);
}

TEST(QuranLoader, WordIdsAreStableAndOneBased) {
    REQUIRE_DATA();
    for (const auto& surah : Loaded().surahs()) {
        for (const auto& ayah : surah.ayahs) {
            ASSERT_FALSE(ayah.words.empty());
            for (size_t i = 0; i < ayah.words.size(); ++i) {
                EXPECT_EQ(ayah.words[i].Id(), std::to_string(surah.number) + ":" + std::to_string(ayah.ayah) +
                                                  ":" + std::to_string(i + 1));
            }
        }
    }
}

TEST(QuranLoader, BasmalaPrefixOnlyWhereExpected) {
    REQUIRE_DATA();
    for (const auto& surah : Loaded().surahs()) {
        EXPECT_EQ(surah.basmala.size(), (surah.number == 1 || surah.number == 9) ? 0u : 4u);
    }
}

TEST(QuranLoader, FindAyah) {
    REQUIRE_DATA();
    const auto& q = Loaded();
    ASSERT_NE(q.FindAyah(1, 1), nullptr);
    EXPECT_EQ(q.FindAyah(1, 1)->ayah, 1);
    EXPECT_EQ(q.FindAyah(0, 1), nullptr);
    EXPECT_EQ(q.FindAyah(115, 1), nullptr);
    EXPECT_EQ(q.FindAyah(1, 0), nullptr);
    EXPECT_EQ(q.FindAyah(1, 100000), nullptr);
}

TEST(QuranLoader, NoWordIsMarkOnly) {
    REQUIRE_DATA();
    for (const auto& surah : Loaded().surahs())
        for (const auto& ayah : surah.ayahs)
            for (const auto& w : ayah.words)
                EXPECT_FALSE(mutqin::IsMarkOnly(w.text)) << w.Id();
}

TEST(QuranLoader, ModifiedSourceFileIsRejected) {
    REQUIRE_DATA();
    fs::path tmp = fs::temp_directory_path() / "mutqin_modified_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    for (const char* name : {"quran-uthmani.txt", "quran-simple-clean.txt", "checksums.txt"}) {
        fs::copy_file(fs::path(kDataDir) / name, tmp / name);
    }
    std::ofstream(tmp / "quran-uthmani.txt", std::ios::binary | std::ios::app) << "\n";

    mutqin::Quran q;
    std::string error;
    EXPECT_FALSE(mutqin::Quran::Load(tmp.string(), &q, &error));
    EXPECT_NE(error.find("checksum mismatch"), std::string::npos) << error;
    fs::remove_all(tmp);
}

TEST(QuranLoader, MissingDirectoryIsRejected) {
    mutqin::Quran q;
    std::string error;
    EXPECT_FALSE(mutqin::Quran::Load("/nonexistent/mutqin/dir", &q, &error));
    EXPECT_FALSE(error.empty());
}

TEST(IsMarkOnly, ClassifiesByCodePointRange) {
    EXPECT_TRUE(mutqin::IsMarkOnly("\xDB\x96"));          // U+06D6, a waqf sign
    EXPECT_TRUE(mutqin::IsMarkOnly("\xDB\xA9"));          // U+06E9, sajdah sign
    EXPECT_TRUE(mutqin::IsMarkOnly("\xDB\x96\xDB\x97"));  // two marks
    EXPECT_FALSE(mutqin::IsMarkOnly(""));
    EXPECT_FALSE(mutqin::IsMarkOnly("a"));
    EXPECT_FALSE(mutqin::IsMarkOnly("\xD8\xA8"));          // U+0628, a letter
    EXPECT_FALSE(mutqin::IsMarkOnly("\xDB\x96\xD8\xA8"));  // mark followed by a letter
    EXPECT_FALSE(mutqin::IsMarkOnly("\xDB"));              // invalid UTF-8
}
