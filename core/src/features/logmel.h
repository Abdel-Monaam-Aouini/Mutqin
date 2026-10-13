// Log-mel filterbank features (Kaldi-style fbank: povey-free, Hann window, pre-emphasis 0.97).
// Audio is 16 kHz mono float32 in [-1, 1]. The model task fixes the final settings; the defaults
// are the usual 25 ms window, 10 ms shift, 80 mel bins.
#ifndef MUTQIN_FEATURES_LOGMEL_H
#define MUTQIN_FEATURES_LOGMEL_H

#include <cstddef>
#include <vector>

namespace mutqin {

struct LogMelConfig {
    int sample_rate = 16000;
    int frame_length = 400;  // samples (25 ms)
    int frame_shift = 160;   // samples (10 ms)
    int fft_size = 512;      // power of two >= frame_length
    int num_mels = 80;
    float low_freq = 20.0f;
    float high_freq = 8000.0f;
    float preemphasis = 0.97f;
};

// Streaming extractor: Accept() any amount of audio, frames come out as soon as enough samples
// are available. Not thread safe.
class LogMel {
public:
    explicit LogMel(const LogMelConfig& config = LogMelConfig());

    const LogMelConfig& config() const { return config_; }
    // Appends audio and returns the new frames (num_mels floats each, concatenated).
    std::vector<float> Accept(const float* samples, size_t count);
    size_t frames_emitted() const { return frames_; }

private:
    void Frame(const float* in, float* out);

    LogMelConfig config_;
    std::vector<float> pending_;
    std::vector<float> window_;
    std::vector<float> filters_;  // num_mels x (fft_size/2+1)
    std::vector<float> re_, im_;
    size_t frames_ = 0;
};

}  // namespace mutqin

#endif  // MUTQIN_FEATURES_LOGMEL_H
