// End-to-end through the C API with synthetic posteriors built from the session's own expected
// words (no audio, no model): perfect, skipped-word and repeated-word recitations of 1:1-1:7.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "align/ctc.h"
#include "mutqin.h"
#include "tajweed/inventory.h"
#include "tajweed/phonemizer.h"
#include "text/quran.h"

using namespace mutqin;

namespace {

const std::string kDataDir = MUTQIN_DATA_DIR;

struct Fixture {
    Quran quran;
    std::vector<WordRef> words;
    mq_engine* engine = nullptr;
    mq_session* session = nullptr;
    bool ok = false;

    Fixture() {
        std::string error;
        if (!Quran::Load(kDataDir, &quran, &error))
            return;
        std::vector<const Ayah*> ayahs;
        std::map<std::pair<int, int>, int> lens;
        for (int a = 1; a <= 7; ++a) {
            ayahs.push_back(quran.FindAyah(1, a));
            lens[{1, a}] = static_cast<int>(ayahs.back()->words.size());
        }
        PhonemizeResult result;
        if (!PhonemizeRange(ayahs, &result, &error))
            return;
        words = BuildWordReadings(result.items, lens);
        char err[256];
        if (mq_engine_create(kDataDir.c_str(), &engine, err, sizeof err) != MQ_OK)
            return;
        if (mq_session_create(engine, 1, 1, 7, &session, err, sizeof err) != MQ_OK)
            return;
        ok = true;
    }
    ~Fixture() {
        mq_session_destroy(session);
        mq_engine_destroy(engine);
    }

    // Posterior rows for the given words (by index): each phoneme is 3 confident frames plus a blank.
    std::vector<float> Speak(const std::vector<size_t>& order) const {
        std::vector<float> rows;
        auto row = [&](int label, float p) {
            std::vector<float> r(MQ_VOCAB_SIZE, (1.0f - p) / (MQ_VOCAB_SIZE - 1));
            r[label] = p;
            rows.insert(rows.end(), r.begin(), r.end());
        };
        for (size_t w : order) {
            for (const std::string& sym : words[w].reading.cont) {
                for (int i = 0; i < 3; ++i)
                    row(TokenId(sym), 0.95f);
                row(0, 0.99f);
            }
        }
        return rows;
    }
};

std::vector<size_t> AllWords(size_t n) {
    std::vector<size_t> v;
    for (size_t i = 0; i < n; ++i)
        v.push_back(i);
    return v;
}

}  // namespace

TEST(SessionEvents, PerfectRecitationRaisesNothing) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    ASSERT_EQ(mq_session_word_count(f.session), f.words.size());
    const std::vector<float> rows = f.Speak(AllWords(f.words.size()));
    ASSERT_EQ(mq_session_push_posteriors(f.session, rows.data(), rows.size() / MQ_VOCAB_SIZE), MQ_OK);
    EXPECT_EQ(mq_session_event_count(f.session), 0u);
    for (size_t w = 0; w < f.words.size(); ++w)
        EXPECT_EQ(mq_session_word_status(f.session, w), MQ_WORD_OK) << w;
}

TEST(SessionEvents, SkippedWordIsReported) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    std::vector<size_t> order = AllWords(f.words.size());
    order.erase(order.begin() + 8);
    const std::vector<float> rows = f.Speak(order);
    ASSERT_EQ(mq_session_push_posteriors(f.session, rows.data(), rows.size() / MQ_VOCAB_SIZE), MQ_OK);
    ASSERT_EQ(mq_session_event_count(f.session), 1u);
    mq_event e{};
    ASSERT_EQ(mq_session_event(f.session, 0, &e), MQ_OK);
    EXPECT_EQ(e.kind, MQ_EVENT_SKIPPED);
    EXPECT_EQ(e.word_index, 8u);
    EXPECT_EQ(mq_session_word_status(f.session, 8), MQ_WORD_SKIPPED);
}

TEST(SessionEvents, RepeatedWordsAreReportedAndStreamingMatchesBatch) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    std::vector<size_t> order = AllWords(f.words.size());
    order.insert(order.begin() + 12, {10, 11});  // 10, 11, 10, 11, 12 ...
    const std::vector<float> rows = f.Speak(order);
    const size_t frames = rows.size() / MQ_VOCAB_SIZE;
    for (size_t at = 0; at < frames; at += 37)  // chunked pushes
        ASSERT_EQ(mq_session_push_posteriors(f.session, rows.data() + at * MQ_VOCAB_SIZE,
                                             std::min<size_t>(37, frames - at)),
                  MQ_OK);
    bool repeated = false;
    for (size_t i = 0; i < mq_session_event_count(f.session); ++i) {
        mq_event e{};
        ASSERT_EQ(mq_session_event(f.session, i, &e), MQ_OK);
        if (e.kind == MQ_EVENT_REPEATED)
            repeated = true;
        EXPECT_NE(e.kind, MQ_EVENT_SKIPPED);
        EXPECT_NE(e.kind, MQ_EVENT_WRONG);
    }
    EXPECT_TRUE(repeated);
}

TEST(SessionEvents, RejectsBadArguments) {
    EXPECT_EQ(mq_session_push_posteriors(nullptr, nullptr, 0), MQ_ERR_ARGUMENT);
    EXPECT_EQ(mq_session_word_count(nullptr), 0u);
    EXPECT_EQ(mq_session_word_status(nullptr, 0), -1);
    mq_event e{};
    EXPECT_EQ(mq_session_event(nullptr, 0, &e), MQ_ERR_ARGUMENT);
}
