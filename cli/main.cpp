// mutqin-cli: thin client of the C API (mutqin.h). Only the public header is used.
// Modes: print the expected phonemes of an ayah range (default), replay recorded model output
// (--posteriors), or read a WAV (--wav; analysing audio needs a recognition model, not bundled yet).
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "mutqin.h"
#include "wav.h"
#ifdef MUTQIN_HAVE_MIC
#include <csignal>
#include <thread>

#include "mic.h"
#endif

namespace {

void Usage() {
    std::fputs(
        "usage: mutqin-cli --surah N --ayah A[-B] [--data DIR] [--posteriors FILE | --wav FILE]\n"
        "  without --posteriors / --wav: prints the expected phonemes (provisional tajweed rules).\n"
        "  --data DIR         verified Quran text (default: data/quran)\n"
        "  --posteriors FILE  replay model output: raw little-endian float32, 32 values per frame\n"
        "                     (blank + 31 phonemes); prints word statuses and events\n"
        "  --chunk N          with --posteriors: push N frames at a time (streaming), prints the time\n"
        "  --model FILE       recognition model manifest (mutqin_model.txt), needed by --wav and --mic\n"
        "  --strictness LEVEL relaxed | standard (default) | strict: how many differences are reported\n"
        "  --timeline         with --wav: print each event with the audio time at which it first appeared\n"
        "  --mic [--seconds N]  live microphone (stop with Ctrl-C or after N seconds); needs a model\n"
        "  --wav FILE         16 kHz WAV (mono or stereo, 16-bit or float)\n"
        "  --version    print the library version\n",
        stderr);
}

bool ParseRange(const char* text, int* first, int* last) {
    char* end = nullptr;
    *first = static_cast<int>(std::strtol(text, &end, 10));
    if (end == text)
        return false;
    *last = *first;
    if (*end == '-') {
        const char* rest = end + 1;
        *last = static_cast<int>(std::strtol(rest, &end, 10));
        if (end == rest)
            return false;
    }
    return *end == '\0';
}

const char* KindName(int kind) {
    switch (kind) {
        case MQ_EVENT_SKIPPED:
            return "skipped";
        case MQ_EVENT_WRONG:
            return "wrong";
        case MQ_EVENT_REPEATED:
            return "repeated";
        default:
            return "added";
    }
}

void PrintEvents(const mq_session* session) {
    const size_t events = mq_session_event_count(session);
    for (size_t i = 0; i < events; ++i) {
        mq_event e{};
        mq_session_event(session, i, &e);
        std::printf("%s\t%d:%d:%d\tword %zu\tconfidence %.2f\n", KindName(e.kind), e.surah, e.ayah, e.word,
                    e.word_index, e.confidence);
    }
    if (events == 0)
        std::printf("no mistakes found\n");
}

// Prints how the set of events changed since the last call, with the audio time: new ones, and the ones
// that later audio took back ("cleared"), so a word whose verdict changes is not shown twice.
void ReportChanges(const mq_session* session, std::set<std::string>* shown, double audio_seconds) {
    std::set<std::string> now;
    const size_t events = mq_session_event_count(session);
    for (size_t i = 0; i < events; ++i) {
        mq_event e{};
        mq_session_event(session, i, &e);
        now.insert(std::string(KindName(e.kind)) + " " + std::to_string(e.surah) + ":" +
                   std::to_string(e.ayah) + ":" + std::to_string(e.word));
    }
    for (const std::string& id : *shown)
        if (now.count(id) == 0)
            std::printf("[%6.1f s] cleared %s\n", audio_seconds, id.c_str());
    for (const std::string& id : now)
        if (shown->count(id) == 0)
            std::printf("[%6.1f s] %s\n", audio_seconds, id.c_str());
    *shown = now;
}

#ifdef MUTQIN_HAVE_MIC
volatile std::sig_atomic_t g_stop = 0;

// Captures from the microphone, feeds the core in 100 ms chunks and prints the events when they change.
int RunMic(mq_session* session, int seconds) {
    mutqin_cli::Microphone mic;
    std::string error;
    if (!mic.Start(&error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        return 1;
    }
    std::signal(SIGINT, [](int) { g_stop = 1; });
    std::fprintf(stderr, "listening%s... (Ctrl-C to stop)\n", seconds > 0 ? " for a limited time" : "");
    const auto started = std::chrono::steady_clock::now();
    std::vector<float> samples;
    std::set<std::string> shown;
    size_t pushed = 0;
    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        samples.clear();
        mic.Drain(&samples);
        if (!samples.empty()) {
            const int rc = mq_session_push_audio(session, samples.data(), samples.size());
            if (rc == MQ_ERR_NO_MODEL) {
                std::fprintf(stderr,
                             "no recognition model is bundled yet, so the audio cannot be analysed\n");
                return 3;
            }
            if (rc != MQ_OK) {
                std::fprintf(stderr, "error: could not process the audio\n");
                return 1;
            }
            pushed += samples.size();
            ReportChanges(session, &shown, static_cast<double>(pushed) / 16000.0);
            std::fflush(stdout);
        }
        if (seconds > 0 && std::chrono::steady_clock::now() - started >= std::chrono::seconds(seconds))
            break;
    }
    if (mq_session_finish_audio(session) == MQ_OK) {  // judge the last word too
        std::printf("final result:\n");
        PrintEvents(session);
    }
    return 0;
}
#endif

// Feeds recorded posteriors to the session and prints what the core found.
int Replay(mq_session* session, const std::string& path, size_t chunk_frames) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "error: cannot open %s\n", path.c_str());
        return 1;
    }
    std::vector<float> data;
    float value;
    while (in.read(reinterpret_cast<char*>(&value), sizeof value))
        data.push_back(value);
    if (data.empty() || data.size() % MQ_VOCAB_SIZE != 0) {
        std::fprintf(stderr, "error: %s must hold rows of %d float32 values\n", path.c_str(), MQ_VOCAB_SIZE);
        return 1;
    }
    const size_t frames = data.size() / MQ_VOCAB_SIZE;
    const size_t step = chunk_frames == 0 ? frames : chunk_frames;
    const auto started = std::chrono::steady_clock::now();
    for (size_t at = 0; at < frames; at += step) {
        if (mq_session_push_posteriors(session, data.data() + at * MQ_VOCAB_SIZE,
                                       std::min(step, frames - at)) != MQ_OK) {
            std::fprintf(stderr, "error: could not process the posteriors\n");
            return 1;
        }
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::fprintf(stderr, "%zu frames in %zu pushes: %.1f ms\n", frames, (frames + step - 1) / step, ms);
    PrintEvents(session);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data_dir = "data/quran";
    std::string posteriors_path, wav_path, model_path;
    int strictness = MQ_STRICTNESS_STANDARD;
    bool timeline = false;
    bool use_mic = false;
    [[maybe_unused]] int mic_seconds = 0;  // unused when built without microphone support
    size_t chunk_frames = 0;
    int surah = 0, first = 0, last = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--version") {
            std::printf("%s\n", mq_version());
            return 0;
        } else if (arg == "--data" && has_value) {
            data_dir = argv[++i];
        } else if (arg == "--posteriors" && has_value) {
            posteriors_path = argv[++i];
        } else if (arg == "--model" && has_value) {
            model_path = argv[++i];
        } else if (arg == "--strictness" && has_value) {
            const std::string level = argv[++i];
            if (level == "relaxed")
                strictness = MQ_STRICTNESS_RELAXED;
            else if (level == "standard")
                strictness = MQ_STRICTNESS_STANDARD;
            else if (level == "strict")
                strictness = MQ_STRICTNESS_STRICT;
            else {
                Usage();
                return 2;
            }
        } else if (arg == "--timeline") {
            timeline = true;
        } else if (arg == "--mic") {
            use_mic = true;
        } else if (arg == "--seconds" && has_value) {
            mic_seconds = std::atoi(argv[++i]);
        } else if (arg == "--chunk" && has_value) {
            chunk_frames = static_cast<size_t>(std::atoi(argv[++i]));
        } else if (arg == "--wav" && has_value) {
            wav_path = argv[++i];
        } else if (arg == "--surah" && has_value) {
            surah = std::atoi(argv[++i]);
        } else if (arg == "--ayah" && has_value) {
            if (!ParseRange(argv[++i], &first, &last)) {
                Usage();
                return 2;
            }
        } else {
            Usage();
            return 2;
        }
    }
    if (surah < 1 || first < 1) {
        Usage();
        return 2;
    }

    char err[256];
    mq_engine* engine = nullptr;
    if (mq_engine_create(data_dir.c_str(), &engine, err, sizeof err) != MQ_OK) {
        std::fprintf(stderr, "error: %s\n(run tools/quran_text/download.py to fetch the verified text)\n",
                     err);
        return 1;
    }
    mq_session* session = nullptr;
    if (mq_session_create(engine, surah, first, last, &session, err, sizeof err) != MQ_OK) {
        std::fprintf(stderr, "error: %s\n", err);
        mq_engine_destroy(engine);
        return 1;
    }
    int status = 0;
    mq_session_set_strictness(session, strictness);
    if (!model_path.empty() && mq_session_load_model(session, model_path.c_str(), err, sizeof err) != MQ_OK) {
        std::fprintf(stderr, "error: %s\n", err);
        mq_session_destroy(session);
        mq_engine_destroy(engine);
        return 1;
    }
    if (!posteriors_path.empty()) {
        status = Replay(session, posteriors_path, chunk_frames);
        mq_session_destroy(session);
        mq_engine_destroy(engine);
        return status;
    }
    if (use_mic) {
#ifdef MUTQIN_HAVE_MIC
        status = RunMic(session, mic_seconds);
#else
        std::fprintf(stderr, "this build of mutqin-cli has no microphone support (see MUTQIN_CLI_MIC)\n");
        status = 1;
#endif
        mq_session_destroy(session);
        mq_engine_destroy(engine);
        return status;
    }
    if (!wav_path.empty()) {
        mutqin_cli::Wav wav;
        std::string wav_error;
        if (!mutqin_cli::ReadWavFile(wav_path, &wav, &wav_error)) {
            std::fprintf(stderr, "error: %s\n", wav_error.c_str());
            status = 1;
        } else {
            std::printf("%s: %.2f s of %d Hz audio\n", wav_path.c_str(),
                        static_cast<double>(wav.samples.size()) / wav.sample_rate, wav.sample_rate);
            if (mq_session_set_input_rate(session, wav.sample_rate) != MQ_OK)
                std::fprintf(stderr, "warning: unsupported sample rate %d Hz\n", wav.sample_rate);
            std::set<std::string> shown;
            const size_t chunk = 1600;  // 100 ms, as a microphone would deliver it
            for (size_t at = 0; at < wav.samples.size() && status == 0; at += chunk) {
                const size_t n = std::min(chunk, wav.samples.size() - at);
                const int rc = mq_session_push_audio(session, wav.samples.data() + at, n);
                if (rc == MQ_ERR_NO_MODEL) {
                    std::fprintf(stderr,
                                 "no recognition model is bundled yet, so the audio cannot be analysed\n");
                    status = 3;
                } else if (rc != MQ_OK) {
                    std::fprintf(stderr, "error: could not process the audio\n");
                    status = 1;
                } else if (timeline) {
                    ReportChanges(session, &shown, static_cast<double>(at + n) / wav.sample_rate);
                }
            }
            if (status == 0 && mq_session_finish_audio(session) == MQ_OK) {
                if (timeline)
                    ReportChanges(session, &shown, static_cast<double>(wav.samples.size()) / wav.sample_rate);
                std::printf("final result:\n");
                PrintEvents(session);
            }
        }
        mq_session_destroy(session);
        mq_engine_destroy(engine);
        return status;
    }
    const size_t count = mq_session_phoneme_count(session);
    for (size_t i = 0; i < count; ++i) {
        mq_phoneme p{};
        mq_session_phoneme(session, i, &p);
        std::printf("%d:%d:%d\t%s\tvariant=%d\t%s", p.surah, p.ayah, p.word, p.symbol, p.variant, p.rules);
        if (p.dur_max > 0)
            std::printf("\tcounts=%d-%d", p.dur_min, p.dur_max);
        std::printf("\n");
    }
    mq_session_destroy(session);
    mq_engine_destroy(engine);
    return 0;
}
