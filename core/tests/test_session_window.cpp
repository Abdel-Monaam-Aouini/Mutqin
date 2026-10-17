// A long range is aligned through a sliding window: the verdicts and events must be the same as those of
// one alignment over the whole range. Synthetic posteriors built from the expected words (no audio).
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "align/aligner.h"
#include "align/ctc.h"
#include "mutqin.h"
#include "tajweed/inventory.h"
#include "tajweed/phonemizer.h"
#include "text/quran.h"

using namespace mutqin;

namespace {

const std::string kDataDir = MUTQIN_DATA_DIR;
constexpr int kSurah = 2, kLastAyah = 60;  // several windows long (more than 3000 expected units)

struct LongFixture {
    Quran quran;
    std::vector<WordRef> words;
    mq_engine* engine = nullptr;
    mq_session* session = nullptr;
    bool ok = false;

    LongFixture() {
        std::string error;
        if (!Quran::Load(kDataDir, &quran, &error))
            return;
        std::vector<const Ayah*> ayahs;
        std::map<std::pair<int, int>, int> lens;
        for (int a = 1; a <= kLastAyah; ++a) {
            ayahs.push_back(quran.FindAyah(kSurah, a));
            lens[{kSurah, a}] = static_cast<int>(ayahs.back()->words.size());
        }
        PhonemizeResult result;
        if (!PhonemizeRange(ayahs, &result, &error))
            return;
        words = BuildWordReadings(result.items, lens);
        char err[256];
        if (mq_engine_create(kDataDir.c_str(), &engine, err, sizeof err) != MQ_OK)
            return;
        ok = mq_session_create(engine, kSurah, 1, kLastAyah, &session, err, sizeof err) == MQ_OK;
    }
    ~LongFixture() {
        mq_session_destroy(session);
        mq_engine_destroy(engine);
    }

    // Posterior rows for the words in `order`: each phoneme is 3 confident frames plus a blank.
    std::vector<float> Speak(const std::vector<size_t>& order) const {
        std::vector<float> rows;
        for (size_t w : order) {
            for (const std::string& sym : words[w].reading.cont) {
                for (int i = 0; i < 4; ++i) {
                    const int label = i < 3 ? TokenId(sym) : 0;
                    std::vector<float> r(MQ_VOCAB_SIZE, 0.05f / (MQ_VOCAB_SIZE - 1));
                    r[static_cast<size_t>(label)] = 0.95f;
                    rows.insert(rows.end(), r.begin(), r.end());
                }
            }
        }
        return rows;
    }

    // Pushes the rows in pieces of about 3 s, the way a microphone would deliver them.
    int Push(const std::vector<float>& rows) const {
        const size_t frames = rows.size() / MQ_VOCAB_SIZE, step = 300;
        for (size_t at = 0; at < frames; at += step) {
            const size_t n = std::min(step, frames - at);
            const int rc = mq_session_push_posteriors(session, rows.data() + at * MQ_VOCAB_SIZE, n);
            if (rc != MQ_OK)
                return rc;
        }
        return MQ_OK;
    }

    // What one alignment over the whole range says about the same rows.
    AlignResult WholeRange(const std::vector<float>& rows) const {
        std::vector<Frame> frames;
        for (size_t i = 0; i < rows.size() / MQ_VOCAB_SIZE; ++i)
            frames.push_back(ArgmaxFrame(rows.data() + i * MQ_VOCAB_SIZE));
        std::vector<WordReading> readings;
        for (const WordRef& w : words)
            readings.push_back(w.reading);
        return AlignTokens(GreedyDecode(frames), readings);
    }
};

std::vector<size_t> Words(size_t from, size_t to) {
    std::vector<size_t> v;
    for (size_t i = from; i < to; ++i)
        v.push_back(i);
    return v;
}

}  // namespace

TEST(SessionWindow, TheRangeIsLongEnoughToNeedSeveralWindows) {
    LongFixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    size_t units = 0;
    for (const WordRef& w : f.words)
        units += w.reading.cont.size();
    EXPECT_GT(units, 4000u);
}

TEST(SessionWindow, CleanRecitationIsAllOkWhereverTheWindowIs) {
    LongFixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    ASSERT_EQ(f.Push(f.Speak(Words(0, f.words.size()))), MQ_OK);
    EXPECT_EQ(mq_session_event_count(f.session), 0u);
    for (size_t w = 0; w < f.words.size(); ++w)
        ASSERT_EQ(mq_session_word_status(f.session, w), MQ_WORD_OK) << "word " << w;
    EXPECT_EQ(mq_session_position(f.session), static_cast<int>(f.words.size()) - 1);
}

TEST(SessionWindow, MistakesFarIntoTheRangeAreFoundLikeInOneAlignment) {
    LongFixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    const size_t n = f.words.size();
    std::vector<size_t> order = Words(0, n);
    // a word skipped early, one far in, and a word repeated near the end
    const size_t early = 40, late = n * 6 / 10, repeat = n - 30;
    order.insert(order.begin() + static_cast<std::ptrdiff_t>(repeat) + 1, repeat);
    order.erase(order.begin() + static_cast<std::ptrdiff_t>(late));
    order.erase(order.begin() + static_cast<std::ptrdiff_t>(early));
    const std::vector<float> rows = f.Speak(order);
    ASSERT_EQ(f.Push(rows), MQ_OK);

    const AlignResult whole = f.WholeRange(rows);
    ASSERT_EQ(mq_session_event_count(f.session), whole.events.size());
    for (size_t i = 0; i < whole.events.size(); ++i) {
        mq_event e{};
        ASSERT_EQ(mq_session_event(f.session, i, &e), MQ_OK);
        EXPECT_EQ(e.word_index, static_cast<size_t>(whole.events[i].index)) << "event " << i;
    }
    bool skipped_early = false, skipped_late = false;
    for (const AlignEvent& e : whole.events) {
        skipped_early |= e.kind == "skipped" && e.index == static_cast<int>(early);
        skipped_late |= e.kind == "skipped" && e.index == static_cast<int>(late);
    }
    EXPECT_TRUE(skipped_early && skipped_late) << "the single alignment must see both skips";
    for (const auto& [w, status] : whole.statuses) {
        const int got = mq_session_word_status(f.session, static_cast<size_t>(w));
        const int want = status == "ok" ? MQ_WORD_OK : status == "wrong" ? MQ_WORD_WRONG : MQ_WORD_SKIPPED;
        EXPECT_EQ(got, want) << "word " << w;
    }
}

TEST(SessionWindow, ALevelChangeJudgesTheWholeRecitationAgain) {
    LongFixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    ASSERT_EQ(f.Push(f.Speak(Words(0, f.words.size()))), MQ_OK);
    ASSERT_EQ(mq_session_set_strictness(f.session, MQ_STRICTNESS_STRICT), MQ_OK);
    EXPECT_EQ(mq_session_position(f.session), static_cast<int>(f.words.size()) - 1);
    EXPECT_EQ(mq_session_word_status(f.session, 0), MQ_WORD_OK);
    EXPECT_EQ(mq_session_word_status(f.session, f.words.size() - 2), MQ_WORD_OK);
}
