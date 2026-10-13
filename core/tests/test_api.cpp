#include <gtest/gtest.h>

#include <string>

#include "mutqin.h"

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
