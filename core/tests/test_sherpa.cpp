// Integration test of the sherpa-onnx backend. It needs a streaming Zipformer2 CTC model folder in
// MUTQIN_SHERPA_TEST_MODEL_DIR (any language: the units are mapped to phonemes arbitrarily, so this
// checks the plumbing only, not recognition quality) and a 16 kHz WAV in MUTQIN_SHERPA_TEST_WAV.
#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "mutqin.h"
#include "tajweed/inventory.h"
#include "wav.h"

namespace fs = std::filesystem;

TEST(Sherpa, StreamingModelRunsThroughTheCApi) {
    const char* model_dir = std::getenv("MUTQIN_SHERPA_TEST_MODEL_DIR");
    const char* wav_path = std::getenv("MUTQIN_SHERPA_TEST_WAV");
    if (model_dir == nullptr || wav_path == nullptr)
        GTEST_SKIP() << "set MUTQIN_SHERPA_TEST_MODEL_DIR and MUTQIN_SHERPA_TEST_WAV";

    std::string onnx;
    for (const auto& entry : fs::directory_iterator(model_dir))
        if (entry.path().extension() == ".onnx" && entry.path().string().find("int8") != std::string::npos)
            onnx = entry.path().string();
    ASSERT_FALSE(onnx.empty()) << "no .int8.onnx in " << model_dir;

    // Arbitrary map: model unit i -> phoneme (i mod 31). Only the plumbing is checked.
    const fs::path tmp = fs::path(::testing::TempDir()) / "sherpa_test";
    fs::create_directories(tmp);
    {
        std::ifstream tokens(fs::path(model_dir) / "tokens.txt");
        std::ofstream map(tmp / "token_map.tsv");
        std::string unit;
        int id, n = 0;
        while (tokens >> unit >> id) {
            if (unit.front() == '<')
                continue;  // <blk>, <sos/eos>, <unk>, byte tokens
            map << unit << '\t' << mutqin::Phonemes()[n++ % mutqin::kPhonemeCount].symbol << '\n';
        }
    }
    {
        std::ofstream manifest(tmp / "mutqin_model.txt");
        manifest << "model=" << onnx << "\ntokens=" << (fs::path(model_dir) / "tokens.txt").string()
                 << "\ntoken_map=" << (tmp / "token_map.tsv").string() << "\nthreads=2\n";
    }

    mutqin_cli::Wav wav;
    std::string error;
    ASSERT_TRUE(mutqin_cli::ReadWavFile(wav_path, &wav, &error)) << error;

    mq_engine* engine = nullptr;
    char err[256];
    if (mq_engine_create(MUTQIN_DATA_DIR, &engine, err, sizeof err) != MQ_OK)
        GTEST_SKIP() << "data/quran missing";
    mq_session* session = nullptr;
    ASSERT_EQ(mq_session_create(engine, 1, 1, 7, &session, err, sizeof err), MQ_OK) << err;
    ASSERT_EQ(mq_session_load_model(session, (tmp / "mutqin_model.txt").string().c_str(), err, sizeof err),
              MQ_OK)
        << err;

    const auto started = std::chrono::steady_clock::now();
    for (size_t at = 0; at < wav.samples.size(); at += 1600)
        ASSERT_EQ(mq_session_push_audio(session, wav.samples.data() + at,
                                        std::min<size_t>(1600, wav.samples.size() - at)),
                  MQ_OK);
    ASSERT_EQ(mq_session_finish_audio(session), MQ_OK);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    const double audio_seconds = static_cast<double>(wav.samples.size()) / 16000.0;
    std::printf("[sherpa] %.1f s of audio in %.2f s (real-time factor %.2f), %zu events\n", audio_seconds,
                seconds, seconds / audio_seconds, mq_session_event_count(session));

    // Speech of another language against 1:1-7 must not look like a perfect recitation.
    EXPECT_GT(mq_session_event_count(session), 0u);
    mq_session_destroy(session);
    mq_engine_destroy(engine);
}
