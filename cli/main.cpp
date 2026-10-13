// mutqin-cli: thin client of the C API (mutqin.h). Only the public header is used.
// Current state: prints the expected phonemes of an ayah range. Audio input (WAV, microphone)
// arrives with the model and aligner tasks.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "mutqin.h"

namespace {

void Usage() {
    std::fputs(
        "usage: mutqin-cli --surah N --ayah A[-B] [--data DIR]\n"
        "  prints the expected phonemes (provisional tajweed rules) for the ayah range.\n"
        "  --data DIR   verified Quran text (default: data/quran)\n"
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

}  // namespace

int main(int argc, char** argv) {
    std::string data_dir = "data/quran";
    int surah = 0, first = 0, last = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool has_value = i + 1 < argc;
        if (arg == "--version") {
            std::printf("%s\n", mq_version());
            return 0;
        } else if (arg == "--data" && has_value) {
            data_dir = argv[++i];
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
