// End-to-end through the C API with synthetic posteriors built from the session's own expected
// words (no audio, no model): perfect, skipped-word and repeated-word recitations of 1:1-1:7.
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "align/ctc.h"
#include "api/session_internal.h"
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

namespace {

// Plays back a scripted list of posterior rows: one row per 4 feature frames until it runs out.
class ScriptedModel : public AcousticModel {
public:
    explicit ScriptedModel(std::vector<float> rows) : rows_(std::move(rows)) {}
    void Accept(const float*, size_t frames, std::vector<float>* posteriors) override {
        pending_ += frames;
        while (pending_ >= 4 && next_ + MQ_VOCAB_SIZE <= rows_.size()) {
            posteriors->insert(posteriors->end(), rows_.begin() + static_cast<std::ptrdiff_t>(next_),
                               rows_.begin() + static_cast<std::ptrdiff_t>(next_ + MQ_VOCAB_SIZE));
            next_ += MQ_VOCAB_SIZE;
            pending_ -= 4;
        }
    }
    void Reset() override {
        next_ = 0;
        pending_ = 0;
    }

private:
    std::vector<float> rows_;
    size_t next_ = 0, pending_ = 0;
};

}  // namespace

TEST(SessionAudio, WithoutAModelPushAudioIsRefused) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    const std::vector<float> silence(1600, 0.0f);
    EXPECT_EQ(mq_session_push_audio(f.session, silence.data(), silence.size()), MQ_ERR_NO_MODEL);
    EXPECT_EQ(mq_session_push_audio(nullptr, silence.data(), silence.size()), MQ_ERR_ARGUMENT);
}

TEST(SessionAudio, AudioFlowsThroughFeaturesModelAndAligner) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    std::vector<size_t> order = AllWords(f.words.size());
    order.erase(order.begin() + 8);  // the scripted "recitation" skips word 8
    const std::vector<float> rows = f.Speak(order);
    const size_t frames = rows.size() / MQ_VOCAB_SIZE;
    SetSessionModel(f.session, std::make_unique<ScriptedModel>(rows));

    // Audio content is irrelevant to the fake model; enough samples for 4 feature frames per row.
    const size_t total = (frames * 4 + 2) * 160 + 400;
    const std::vector<float> audio(total, 0.01f);
    for (size_t at = 0; at < audio.size(); at += 1600)
        ASSERT_EQ(
            mq_session_push_audio(f.session, audio.data() + at, std::min<size_t>(1600, audio.size() - at)),
            MQ_OK);
    ASSERT_EQ(mq_session_event_count(f.session), 1u);
    mq_event e{};
    ASSERT_EQ(mq_session_event(f.session, 0, &e), MQ_OK);
    EXPECT_EQ(e.kind, MQ_EVENT_SKIPPED);
    EXPECT_EQ(e.word_index, 8u);
}

#include <fstream>

#include "inference/sherpa_recognizer.h"

namespace {

// Returns a scripted token hypothesis: the first half after Accept, everything after Finish.
class ScriptedRecognizer : public TokenRecognizer {
public:
    explicit ScriptedRecognizer(std::vector<HeardToken> all) : all_(std::move(all)) {}
    void Accept(const float*, size_t, std::vector<HeardToken>* tokens, size_t* from) override {
        tokens->assign(all_.begin(), all_.begin() + static_cast<std::ptrdiff_t>(all_.size() / 2));
        *from = 0;
    }
    void Finish(std::vector<HeardToken>* tokens, size_t* from) override {
        *tokens = all_;
        *from = 0;
    }
    void Reset() override {}

private:
    std::vector<HeardToken> all_;
};

}  // namespace

TEST(SessionTokens, TokenRecognizerFeedsTheAlignerAndFinishFlushes) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    std::vector<HeardToken> heard;
    int frame = 0;
    for (size_t w = 0; w < f.words.size(); ++w) {
        if (w == 8)
            continue;  // the scripted recitation skips word 8
        for (const std::string& sym : f.words[w].reading.cont) {
            HeardToken t;
            t.symbol = sym;
            t.confidence = 1.0;
            t.start = frame;
            t.end = frame + 4;
            frame += 4;
            heard.push_back(t);
        }
    }
    SetSessionRecognizer(f.session, std::make_unique<ScriptedRecognizer>(heard));
    const std::vector<float> audio(1600, 0.0f);
    ASSERT_EQ(mq_session_push_audio(f.session, audio.data(), audio.size()), MQ_OK);
    ASSERT_EQ(mq_session_finish_audio(f.session), MQ_OK);
    ASSERT_EQ(mq_session_event_count(f.session), 1u);
    mq_event e{};
    ASSERT_EQ(mq_session_event(f.session, 0, &e), MQ_OK);
    EXPECT_EQ(e.kind, MQ_EVENT_SKIPPED);
    EXPECT_EQ(e.word_index, 8u);
}

TEST(TokenMap, ParsesAndValidates) {
    TokenMap map;
    std::string error;
    ASSERT_TRUE(ParseTokenMap("# comment\nAA\ta a\nB\tb\n\nSIL\t\n", &map, &error)) << error;
    EXPECT_EQ(map.at("AA"), (std::vector<std::string>{"a", "a"}));
    EXPECT_EQ(map.at("B"), (std::vector<std::string>{"b"}));
    EXPECT_TRUE(map.at("SIL").empty());
    EXPECT_FALSE(ParseTokenMap("X\tnotaphoneme\n", &map, &error));
    EXPECT_NE(error.find("notaphoneme"), std::string::npos);
    EXPECT_FALSE(ParseTokenMap("no tab here\n", &map, &error));
}

TEST(ModelManifest, ResolvesPathsRelativeToTheManifest) {
    const std::string dir = ::testing::TempDir();
    const std::string path = dir + "/mutqin_manifest_test.txt";
    {
        std::ofstream out(path);
        out << "# model\nmodel=encoder.onnx\ntokens=tokens.txt\ntoken_map=/abs/map.tsv\nthreads=3\n";
    }
    SherpaConfig config;
    std::string error;
    ASSERT_TRUE(ReadModelManifest(path, &config, &error)) << error;
    EXPECT_EQ(config.model, dir + "/encoder.onnx");
    EXPECT_EQ(config.tokens, dir + "/tokens.txt");
    EXPECT_EQ(config.token_map, "/abs/map.tsv");
    EXPECT_EQ(config.threads, 3);
    EXPECT_FALSE(ReadModelManifest(dir + "/does_not_exist.txt", &config, &error));
}

#ifndef MUTQIN_HAVE_SHERPA
TEST(ModelManifest, LoadModelExplainsWhenTheBackendIsNotBuilt) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    char err[256];
    EXPECT_EQ(mq_session_load_model(f.session, "whatever.txt", err, sizeof err), MQ_ERR_NO_MODEL);
    EXPECT_NE(std::string(err).find("MUTQIN_WITH_SHERPA"), std::string::npos);
}
#endif

TEST(SessionLimits, WholeSurahsAreAllowedAndTooMuchSpeechIsRefused) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    char err[256];
    mq_session* big = nullptr;
    // all of al-Baqarah, the longest surah: allowed since the aligner slides a window over the range
    ASSERT_EQ(mq_session_create(f.engine, 2, 1, 286, &big, err, sizeof err), MQ_OK) << err;
    EXPECT_GT(mq_session_word_count(big), 6000u);
    mq_session_destroy(big);

    // Far more heard phonemes than the range could ever need.
    size_t expected = 0;
    for (const WordRef& w : f.words)
        expected += w.reading.cont.size();
    const size_t frames = (2 * expected + 600) * 4;
    std::vector<float> rows(frames * MQ_VOCAB_SIZE, 0.0f);
    for (size_t i = 0; i < frames; ++i) {  // alternate two phonemes separated by blanks
        const size_t label = (i % 4 == 3) ? 0 : (i / 4 % 2 == 0 ? 1 : 2);
        rows[i * MQ_VOCAB_SIZE + label] = 1.0f;
    }
    EXPECT_EQ(mq_session_push_posteriors(f.session, rows.data(), frames), MQ_ERR_LIMIT);
}

TEST(SessionAudio, TheWordBeingSpokenIsJudgedOnlyAfterTheAudioIsFinished) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    const std::vector<float> rows = f.Speak(AllWords(f.words.size()));
    const size_t frames = rows.size() / MQ_VOCAB_SIZE;
    SetSessionModel(f.session, std::make_unique<ScriptedModel>(rows));
    const std::vector<float> audio((frames * 4 + 2) * 160 + 400, 0.01f);
    for (size_t at = 0; at < audio.size(); at += 1600)
        ASSERT_EQ(
            mq_session_push_audio(f.session, audio.data() + at, std::min<size_t>(1600, audio.size() - at)),
            MQ_OK);
    const size_t last = mq_session_word_count(f.session) - 1;
    EXPECT_EQ(mq_session_word_status(f.session, last), MQ_WORD_NOT_JUDGED);  // still streaming
    EXPECT_EQ(mq_session_word_status(f.session, last - 1), MQ_WORD_OK);
    ASSERT_EQ(mq_session_finish_audio(f.session), MQ_OK);
    EXPECT_EQ(mq_session_word_status(f.session, last), MQ_WORD_OK);
}

TEST(SessionAudio, AudioAtAnotherSampleRateIsResampled) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    std::vector<size_t> order = AllWords(f.words.size());
    order.erase(order.begin() + 8);
    const std::vector<float> rows = f.Speak(order);
    const size_t frames = rows.size() / MQ_VOCAB_SIZE;
    SetSessionModel(f.session, std::make_unique<ScriptedModel>(rows));
    EXPECT_EQ(mq_session_set_input_rate(f.session, 48000), MQ_OK);
    EXPECT_EQ(mq_session_set_input_rate(f.session, 100), MQ_ERR_ARGUMENT);
    EXPECT_EQ(mq_session_set_input_rate(nullptr, 48000), MQ_ERR_ARGUMENT);
    EXPECT_EQ(mq_session_set_input_rate(f.session, 48000), MQ_OK);
    // the same duration as in AudioFlowsThroughFeaturesModelAndAligner, but sampled at 48 kHz
    const std::vector<float> audio(((frames * 4 + 2) * 160 + 400) * 3 + 2000, 0.01f);
    for (size_t at = 0; at < audio.size(); at += 4800)
        ASSERT_EQ(
            mq_session_push_audio(f.session, audio.data() + at, std::min<size_t>(4800, audio.size() - at)),
            MQ_OK);
    ASSERT_EQ(mq_session_finish_audio(f.session), MQ_OK);
    ASSERT_EQ(mq_session_event_count(f.session), 1u);
    mq_event e{};
    ASSERT_EQ(mq_session_event(f.session, 0, &e), MQ_OK);
    EXPECT_EQ(e.kind, MQ_EVENT_SKIPPED);
    EXPECT_EQ(e.word_index, 8u);
}

TEST(SessionPosition, ItemsPositionAndReset) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    mq_item item{};
    ASSERT_EQ(mq_session_item(f.session, 0, &item), MQ_OK);
    EXPECT_EQ(item.surah, 1);
    EXPECT_EQ(item.ayah, 1);
    EXPECT_EQ(item.word, 1);
    EXPECT_EQ(mq_session_item(f.session, f.words.size(), &item), MQ_ERR_ARGUMENT);
    EXPECT_EQ(mq_session_position(f.session), -1);

    // the first 10 words heard: the position is the 10th item (index 9)
    const std::vector<float> rows = f.Speak(std::vector<size_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    ASSERT_EQ(mq_session_push_posteriors(f.session, rows.data(), rows.size() / MQ_VOCAB_SIZE), MQ_OK);
    EXPECT_EQ(mq_session_position(f.session), 9);

    ASSERT_EQ(mq_session_reset(f.session), MQ_OK);
    EXPECT_EQ(mq_session_position(f.session), -1);
    EXPECT_EQ(mq_session_event_count(f.session), 0u);
    ASSERT_EQ(mq_session_push_posteriors(f.session, rows.data(), rows.size() / MQ_VOCAB_SIZE), MQ_OK);
    EXPECT_EQ(mq_session_position(f.session), 9);  // a new attempt behaves like the first
    EXPECT_EQ(mq_session_reset(nullptr), MQ_ERR_ARGUMENT);
}

#include <atomic>
#include <thread>

TEST(SessionThreads, OnePushesWhileAnotherPollsAndTheResultIsTheSame) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    std::vector<size_t> order = AllWords(f.words.size());
    order.erase(order.begin() + 8);  // one skipped word
    const std::vector<float> rows = f.Speak(order);
    const size_t frames = rows.size() / MQ_VOCAB_SIZE;
    SetSessionModel(f.session, std::make_unique<ScriptedModel>(rows));
    const std::vector<float> audio((frames * 4 + 2) * 160 + 400, 0.01f);

    std::atomic<bool> done{false};
    std::atomic<long> polls{0};
    std::thread poller([&] {
        while (!done) {
            const size_t count = mq_session_event_count(f.session);
            for (size_t i = 0; i < count; ++i) {
                mq_event e{};
                (void)mq_session_event(f.session, i,
                                       &e);  // may race with a push: only the lock keeps it valid
            }
            (void)mq_session_position(f.session);
            (void)mq_session_word_status(f.session, 3);
            ++polls;
        }
    });
    for (size_t at = 0; at < audio.size(); at += 160)
        ASSERT_EQ(
            mq_session_push_audio(f.session, audio.data() + at, std::min<size_t>(160, audio.size() - at)),
            MQ_OK);
    ASSERT_EQ(mq_session_finish_audio(f.session), MQ_OK);
    done = true;
    poller.join();
    EXPECT_GT(polls.load(), 0);

    ASSERT_EQ(mq_session_event_count(f.session), 1u);
    mq_event e{};
    ASSERT_EQ(mq_session_event(f.session, 0, &e), MQ_OK);
    EXPECT_EQ(e.kind, MQ_EVENT_SKIPPED);
    EXPECT_EQ(e.word_index, 8u);
}

TEST(SessionThreads, TwoSessionsOfOneEngineRunOnTwoThreads) {
    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    char err[256];
    mq_session* other = nullptr;
    ASSERT_EQ(mq_session_create(f.engine, 1, 1, 7, &other, err, sizeof err), MQ_OK) << err;
    const std::vector<float> rows = f.Speak(AllWords(f.words.size()));
    const size_t n = rows.size() / MQ_VOCAB_SIZE;
    int rc_a = -1, rc_b = -1;
    std::thread a([&] { rc_a = mq_session_push_posteriors(f.session, rows.data(), n); });
    std::thread b([&] { rc_b = mq_session_push_posteriors(other, rows.data(), n); });
    a.join();
    b.join();
    EXPECT_EQ(rc_a, MQ_OK);
    EXPECT_EQ(rc_b, MQ_OK);
    EXPECT_EQ(mq_session_event_count(f.session), 0u);
    EXPECT_EQ(mq_session_event_count(other), 0u);
    mq_session_destroy(other);
}

TEST(Strictness, LevelsAreOrderedAndTheApiValidatesThem) {
    const AlignParams relaxed = StrictnessProfile(MQ_STRICTNESS_RELAXED);
    const AlignParams standard = StrictnessProfile(MQ_STRICTNESS_STANDARD);
    const AlignParams strict = StrictnessProfile(MQ_STRICTNESS_STRICT);
    // more edge tolerance, a lower "wrong" bar and later "added" mean fewer reports; strict is the opposite
    EXPECT_GT(relaxed.edge_free, standard.edge_free);
    EXPECT_GT(standard.edge_free, strict.edge_free);
    EXPECT_LT(relaxed.wrong_below, standard.wrong_below);
    EXPECT_LT(standard.wrong_below, strict.wrong_below);
    EXPECT_GT(relaxed.add_min_tokens, standard.add_min_tokens);
    EXPECT_GT(standard.add_min_tokens, strict.add_min_tokens);
    EXPECT_EQ(StrictnessProfile(7).edge_free, standard.edge_free);  // unknown level: standard

    Fixture f;
    if (!f.ok)
        GTEST_SKIP() << "data/quran missing";
    EXPECT_EQ(mq_session_set_strictness(f.session, MQ_STRICTNESS_STRICT), MQ_OK);
    EXPECT_EQ(mq_session_set_strictness(f.session, MQ_STRICTNESS_RELAXED), MQ_OK);
    EXPECT_EQ(mq_session_set_strictness(f.session, 3), MQ_ERR_ARGUMENT);
    EXPECT_EQ(mq_session_set_strictness(f.session, -1), MQ_ERR_ARGUMENT);
    EXPECT_EQ(mq_session_set_strictness(nullptr, MQ_STRICTNESS_STANDARD), MQ_ERR_ARGUMENT);
    // a level change on a session without model units is remembered and does not disturb the result
    const std::vector<float> rows = f.Speak(AllWords(f.words.size()));
    ASSERT_EQ(mq_session_push_posteriors(f.session, rows.data(), rows.size() / MQ_VOCAB_SIZE), MQ_OK);
    EXPECT_EQ(mq_session_set_strictness(f.session, MQ_STRICTNESS_STRICT), MQ_OK);
    EXPECT_EQ(mq_session_event_count(f.session), 0u);
}
