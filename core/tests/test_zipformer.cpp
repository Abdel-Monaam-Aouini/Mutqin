// End-to-end test with the real Quran phoneme Zipformer (Muno459/zipformer_p-quran, gated, not in the
// repository). Skipped unless these are set:
//   MUTQIN_ZIPFORMER_MANIFEST  path of mutqin_model.txt (model=, tokens=, units=, collapse_runs=)
//   MUTQIN_ZIPFORMER_WAV       100 s of a correct recitation of 5:1-8 (starting with the basmala)
//   MUTQIN_ZIPFORMER_WAV_SKIP  the same with the word 5:1:6 cut out
#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "mutqin.h"
#include "wav.h"

namespace {

struct Recitation {
    std::vector<mq_event> events;
    bool ok = false;
};

Recitation Recite(const char* manifest, const char* wav_path) {
    Recitation run;
    mutqin_cli::Wav wav;
    std::string error;
    if (!mutqin_cli::ReadWavFile(wav_path, &wav, &error))
        return run;
    mq_engine* engine = nullptr;
    char err[256];
    if (mq_engine_create(MUTQIN_DATA_DIR, &engine, err, sizeof err) != MQ_OK)
        return run;
    mq_session* session = nullptr;
    if (mq_session_create(engine, 5, 1, 8, &session, err, sizeof err) != MQ_OK ||
        mq_session_load_model(session, manifest, err, sizeof err) != MQ_OK) {
        mq_engine_destroy(engine);
        return run;
    }
    for (size_t at = 0; at < wav.samples.size(); at += 1600)
        mq_session_push_audio(session, wav.samples.data() + at,
                              std::min<size_t>(1600, wav.samples.size() - at));
    mq_session_finish_audio(session);
    for (size_t i = 0; i < mq_session_event_count(session); ++i) {
        mq_event e{};
        mq_session_event(session, i, &e);
        run.events.push_back(e);
    }
    run.ok = true;
    mq_session_destroy(session);
    mq_engine_destroy(engine);
    return run;
}

}  // namespace

TEST(Zipformer, CorrectRecitationHasFewEventsAndACutWordIsFound) {
    const char* manifest = std::getenv("MUTQIN_ZIPFORMER_MANIFEST");
    const char* clean = std::getenv("MUTQIN_ZIPFORMER_WAV");
    const char* skipped = std::getenv("MUTQIN_ZIPFORMER_WAV_SKIP");
    if (manifest == nullptr || clean == nullptr || skipped == nullptr)
        GTEST_SKIP() << "set MUTQIN_ZIPFORMER_MANIFEST, MUTQIN_ZIPFORMER_WAV and MUTQIN_ZIPFORMER_WAV_SKIP";

    const Recitation good = Recite(manifest, clean);
    ASSERT_TRUE(good.ok);
    // Measured on this recording: 2 events (a repeated and a wrong word) in 100 s of correct recitation.
    EXPECT_LE(good.events.size(), 4u);
    for (const mq_event& e : good.events)
        EXPECT_NE(e.ayah, 0) << "the basmala must not raise an event";

    const Recitation cut = Recite(manifest, skipped);
    ASSERT_TRUE(cut.ok);
    bool found = false;
    for (const mq_event& e : cut.events)
        found = found || (e.kind == MQ_EVENT_SKIPPED && e.surah == 5 && e.ayah == 1 && e.word == 6);
    EXPECT_TRUE(found) << "the removed word 5:1:6 was not reported as skipped";
}
