// mutqin-cli: thin client of the C API (mutqin.h). Only the public header is used.
// Modes: print the expected phonemes of an ayah range (default), replay recorded model output
// (--posteriors), or read a WAV (--wav; analysing audio needs a recognition model, not bundled yet).
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "mutqin.h"
#include "wav.h"

namespace {

void Usage() {
    std::fputs(
        "usage: mutqin-cli --surah N --ayah A[-B] [--data DIR] [--posteriors FILE | --wav FILE]\n"
        "  without --posteriors / --wav: prints the expected phonemes (provisional tajweed rules).\n"
        "  --data DIR         verified Quran text (default: data/quran)\n"
        "  --posteriors FILE  replay model output: raw little-endian float32, 32 values per frame\n"
        "                     (blank + 31 phonemes); prints word statuses and events\n"
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

// Feeds recorded posteriors to the session and prints what the core found.
int Replay(mq_session* session, const std::string& path) {
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
    if (mq_session_push_posteriors(session, data.data(), data.size() / MQ_VOCAB_SIZE) != MQ_OK) {
        std::fprintf(stderr, "error: could not process the posteriors\n");
        return 1;
    }
    PrintEvents(session);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data_dir = "data/quran";
    std::string posteriors_path, wav_path;
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
    if (!posteriors_path.empty()) {
        status = Replay(session, posteriors_path);
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
            std::printf("%s: %.2f s of 16 kHz audio\n", wav_path.c_str(),
                        static_cast<double>(wav.samples.size()) / 16000.0);
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
                }
            }
            if (status == 0)
                PrintEvents(session);
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
