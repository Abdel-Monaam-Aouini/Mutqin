#include "wav.h"

#include <cstring>
#include <fstream>
#include <iterator>

namespace mutqin_cli {

namespace {

uint32_t U32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
           static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}
uint16_t U16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | p[1] << 8);
}

}  // namespace

bool ParseWav(const std::vector<uint8_t>& bytes, Wav* out, std::string* error) {
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        *error = "not a WAV file";
        return false;
    }
    int format = 0, channels = 0, bits = 0, rate = 0;
    const uint8_t* data = nullptr;
    size_t data_size = 0;
    size_t pos = 12;
    while (pos + 8 <= bytes.size()) {
        const uint8_t* chunk = bytes.data() + pos;
        size_t size = U32(chunk + 4);
        const size_t body = pos + 8;
        if (body + size > bytes.size())
            size = bytes.size() - body;  // tolerate a truncated last chunk
        if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16) {
            format = U16(chunk + 8);
            channels = U16(chunk + 10);
            rate = static_cast<int>(U32(chunk + 12));
            bits = U16(chunk + 22);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            data = chunk + 8;
            data_size = size;
            break;
        }
        pos = body + size + (size & 1);
    }
    if (data == nullptr || channels == 0) {
        *error = "WAV has no fmt or data chunk";
        return false;
    }
    const bool pcm16 = format == 1 && bits == 16, float32 = format == 3 && bits == 32;
    if (!pcm16 && !float32) {
        *error = "unsupported WAV encoding (need 16-bit PCM or 32-bit float)";
        return false;
    }
    if (channels > 2) {
        *error = "need mono or stereo audio";
        return false;
    }
    if (rate != 16000) {
        *error = "need 16 kHz audio, got " + std::to_string(rate) +
                 " Hz (convert with: ffmpeg -i in.wav -ac 1 -ar 16000 out.wav)";
        return false;
    }
    const size_t bytes_per_sample = bits / 8;
    const size_t frames = data_size / (bytes_per_sample * channels);
    out->sample_rate = rate;
    out->samples.assign(frames, 0.0f);
    for (size_t i = 0; i < frames; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            const uint8_t* p = data + (i * channels + c) * bytes_per_sample;
            if (pcm16) {
                sum += static_cast<float>(static_cast<int16_t>(U16(p))) / 32768.0f;
            } else {
                const uint32_t raw = U32(p);
                float v;
                std::memcpy(&v, &raw, sizeof v);
                sum += v;
            }
        }
        out->samples[i] = sum / static_cast<float>(channels);
    }
    return true;
}

bool ReadWavFile(const std::string& path, Wav* out, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        *error = "cannot open " + path;
        return false;
    }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return ParseWav(bytes, out, error);
}

}  // namespace mutqin_cli
