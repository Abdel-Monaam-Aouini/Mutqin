#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "mutqin.h"
#include "text/quran.h"

namespace {
const std::string kDataDir = MUTQIN_DATA_DIR;
}

TEST(Api, RejectsBadArguments) {
    mq_engine* engine = reinterpret_cast<mq_engine*>(1);
    char err[128];
    EXPECT_EQ(mq_engine_create(nullptr, &engine, err, sizeof err), MQ_ERR_ARGUMENT);
    EXPECT_EQ(engine, nullptr);
    EXPECT_EQ(mq_engine_create("/does/not/exist", &engine, err, sizeof err), MQ_ERR_DATA);
    EXPECT_EQ(engine, nullptr);
    EXPECT_NE(std::string(err), "");
    EXPECT_EQ(mq_session_phoneme_count(nullptr), 0u);
    mq_engine_destroy(nullptr);
    mq_session_destroy(nullptr);
}

TEST(Api, ExpectedPhonemesOfARange) {
    mq_engine* engine = nullptr;
    char err[256];
    if (mq_engine_create(kDataDir.c_str(), &engine, err, sizeof err) != MQ_OK) {
        GTEST_SKIP() << "data/quran missing: run tools/quran_text/download.py (" << err << ")";
    }
    mq_session* session = nullptr;
    ASSERT_EQ(mq_session_create(engine, 1, 1, 7, &session, err, sizeof err), MQ_OK) << err;
    const size_t count = mq_session_phoneme_count(session);
    ASSERT_GT(count, 20u);
    mq_phoneme first{};
    ASSERT_EQ(mq_session_phoneme(session, 0, &first), MQ_OK);
    EXPECT_EQ(first.surah, 1);
    EXPECT_EQ(first.ayah, 1);
    EXPECT_EQ(first.word, 1);
    EXPECT_NE(std::string(first.symbol), "");
    mq_phoneme last{};
    ASSERT_EQ(mq_session_phoneme(session, count - 1, &last), MQ_OK);
    EXPECT_EQ(last.ayah, 7);
    EXPECT_EQ(mq_session_phoneme(session, count, &last), MQ_ERR_ARGUMENT);

    mq_session* bad = nullptr;
    EXPECT_EQ(mq_session_create(engine, 1, 1, 8, &bad, err, sizeof err), MQ_ERR_ARGUMENT);
    EXPECT_EQ(bad, nullptr);
    EXPECT_EQ(mq_session_create(engine, 1, 3, 2, &bad, err, sizeof err), MQ_ERR_ARGUMENT);
    mq_session_destroy(session);
    mq_engine_destroy(engine);
}

#include <fstream>
#include <iterator>

namespace {
std::string Slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
}  // namespace

TEST(Api, EngineFromMemoryEqualsEngineFromFolder) {
    const std::string uthmani = Slurp(kDataDir + "/quran-uthmani.txt");
    const std::string simple = Slurp(kDataDir + "/quran-simple-clean.txt");
    const std::string sums = Slurp(kDataDir + "/checksums.txt");
    if (uthmani.empty() || simple.empty() || sums.empty())
        GTEST_SKIP() << "data/quran missing: run tools/quran_text/download.py";
    char err[256];
    mq_engine* engine = nullptr;
    ASSERT_EQ(mq_engine_create_from_memory(uthmani.data(), uthmani.size(), simple.data(), simple.size(),
                                           sums.data(), sums.size(), &engine, err, sizeof err),
              MQ_OK)
        << err;
    mq_session* session = nullptr;
    ASSERT_EQ(mq_session_create(engine, 1, 1, 7, &session, err, sizeof err), MQ_OK) << err;
    EXPECT_GT(mq_session_phoneme_count(session), 20u);
    mq_session_destroy(session);
    mq_engine_destroy(engine);

    std::string tampered = uthmani;
    tampered[tampered.size() / 2] ^= 1;  // flip one bit: the checksum must catch it
    EXPECT_EQ(mq_engine_create_from_memory(tampered.data(), tampered.size(), simple.data(), simple.size(),
                                           sums.data(), sums.size(), &engine, err, sizeof err),
              MQ_ERR_DATA);
    EXPECT_NE(std::string(err).find("checksum mismatch"), std::string::npos) << err;
    EXPECT_EQ(engine, nullptr);
    EXPECT_EQ(mq_engine_create_from_memory(nullptr, 0, simple.data(), simple.size(), sums.data(), sums.size(),
                                           &engine, err, sizeof err),
              MQ_ERR_ARGUMENT);
}

TEST(Api, AyahCountsAndItemTextComeFromTheVerifiedText) {
    mq_engine* engine = nullptr;
    char err[256];
    if (mq_engine_create(kDataDir.c_str(), &engine, err, sizeof err) != MQ_OK)
        GTEST_SKIP() << "data/quran missing: run tools/quran_text/download.py";
    EXPECT_EQ(mq_engine_ayah_count(engine, 1), 7);
    EXPECT_EQ(mq_engine_ayah_count(engine, 2), 286);
    EXPECT_EQ(mq_engine_ayah_count(engine, 114), 6);
    EXPECT_EQ(mq_engine_ayah_count(engine, 0), -1);
    EXPECT_EQ(mq_engine_ayah_count(engine, 115), -1);
    EXPECT_EQ(mq_engine_ayah_count(nullptr, 1), -1);

    mq_session* session = nullptr;
    ASSERT_EQ(mq_session_create(engine, 1, 2, 2, &session, err, sizeof err), MQ_OK) << err;
    // the text of an item is the loader's text of its word (this test types no Quran text)
    mutqin::Quran quran;
    std::string error;
    ASSERT_TRUE(mutqin::Quran::Load(kDataDir, &quran, &error)) << error;
    const mutqin::Ayah* ayah = quran.FindAyah(1, 2);
    ASSERT_NE(ayah, nullptr);
    const int need = mq_session_item_text(session, 0, nullptr, 0);
    ASSERT_GT(need, 0);
    std::string text(static_cast<size_t>(need) + 1, '\0');
    EXPECT_EQ(mq_session_item_text(session, 0, text.data(), text.size()), need);
    text.resize(static_cast<size_t>(need));
    std::string expected = ayah->words[0].text;
    for (const std::string& mark : ayah->words[0].marks)
        expected += " " + mark;
    EXPECT_EQ(text, expected);

    char tiny[4];  // cut, still NUL-terminated, and the full length is reported
    EXPECT_EQ(mq_session_item_text(session, 0, tiny, sizeof tiny), need);
    EXPECT_EQ(std::strlen(tiny), 3u);
    EXPECT_EQ(mq_session_item_text(session, 9999, nullptr, 0), MQ_ERR_ARGUMENT);
    EXPECT_EQ(mq_session_item_text(nullptr, 0, nullptr, 0), MQ_ERR_ARGUMENT);
    mq_session_destroy(session);
    mq_engine_destroy(engine);
}
