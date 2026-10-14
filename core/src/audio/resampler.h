// Streaming resampler to 16 kHz mono (the rate the core works at). Windowed-sinc low-pass, so audio
// from 44.1 / 48 kHz microphones is filtered before it is decimated; same-rate input is copied through.
#ifndef MUTQIN_AUDIO_RESAMPLER_H
#define MUTQIN_AUDIO_RESAMPLER_H

#include <cstddef>
#include <vector>

namespace mutqin {

class Resampler {
public:
    // input_rate in Hz (8000..192000); output is always 16000 Hz.
    explicit Resampler(int input_rate);

    int input_rate() const { return input_rate_; }
    // Appends audio of any length; the resampled samples that are complete are appended to *out.
    // The output does not depend on how the input is cut into chunks.
    void Accept(const float* samples, size_t count, std::vector<float>* out);
    void Reset();

private:
    int input_rate_;
    double ratio_;               // input samples per output sample
    int half_width_;             // taps on each side of the centre, in input samples
    std::vector<float> buffer_;  // input not yet consumed
    long long base_ = 0;         // absolute index (padding included) of buffer_[0]
    long long out_index_ = 0;    // number of output samples produced so far
    bool started_ = false;
};

}  // namespace mutqin

#endif  // MUTQIN_AUDIO_RESAMPLER_H
