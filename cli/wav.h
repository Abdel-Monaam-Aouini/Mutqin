// Minimal WAV reader for the CLI: 16-bit PCM or 32-bit float, mono or stereo (downmixed), 8-192 kHz.
#ifndef MUTQIN_CLI_WAV_H
#define MUTQIN_CLI_WAV_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mutqin_cli {

struct Wav {
    int sample_rate = 0;         // the core resamples to 16 kHz (mq_session_set_input_rate)
    std::vector<float> samples;  // mono, [-1, 1]
};

// Parses a WAV file image. On failure returns false and sets *error.
bool ParseWav(const std::vector<uint8_t>& bytes, Wav* out, std::string* error);
bool ReadWavFile(const std::string& path, Wav* out, std::string* error);

}  // namespace mutqin_cli

#endif  // MUTQIN_CLI_WAV_H
