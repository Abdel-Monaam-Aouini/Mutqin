#include "features/logmel.h"

#include <algorithm>
#include <cmath>

namespace mutqin {

namespace {

const double kPi = 3.14159265358979323846;

double HzToMel(double hz) {
    return 1127.0 * std::log(1.0 + hz / 700.0);
}

// In-place radix-2 FFT, n a power of two.
void Fft(std::vector<float>& re, std::vector<float>& im) {
    const size_t n = re.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * kPi / static_cast<double>(len);
        const float wr = static_cast<float>(std::cos(ang)), wi = static_cast<float>(std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (size_t k = 0; k < len / 2; ++k) {
                const size_t a = i + k, b = i + k + len / 2;
                const float tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
                const float nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}

}  // namespace

LogMel::LogMel(const LogMelConfig& config) : config_(config) {
    const int bins = config_.fft_size / 2 + 1;
    window_.resize(config_.frame_length);
    for (int i = 0; i < config_.frame_length; ++i)
        window_[i] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * kPi * i / (config_.frame_length - 1)));
    re_.resize(config_.fft_size);
    im_.resize(config_.fft_size);

    filters_.assign(static_cast<size_t>(config_.num_mels) * bins, 0.0f);
    const double mel_lo = HzToMel(config_.low_freq), mel_hi = HzToMel(config_.high_freq);
    const double step = (mel_hi - mel_lo) / (config_.num_mels + 1);
    const double bin_hz = static_cast<double>(config_.sample_rate) / config_.fft_size;
    for (int m = 0; m < config_.num_mels; ++m) {
        const double left = mel_lo + m * step, center = left + step, right = center + step;
        for (int b = 0; b < bins; ++b) {
            const double mel = HzToMel(b * bin_hz);
            double w = 0.0;
            if (mel > left && mel <= center)
                w = (mel - left) / (center - left);
            else if (mel > center && mel < right)
                w = (right - mel) / (right - center);
            filters_[static_cast<size_t>(m) * bins + b] = static_cast<float>(w);
        }
    }
}

void LogMel::Frame(const float* in, float* out) {
    const int n = config_.frame_length, bins = config_.fft_size / 2 + 1;
    double mean = 0.0;
    for (int i = 0; i < n; ++i)
        mean += in[i];
    mean /= n;
    std::fill(re_.begin(), re_.end(), 0.0f);
    std::fill(im_.begin(), im_.end(), 0.0f);
    float prev = static_cast<float>(in[0] - mean);
    for (int i = 0; i < n; ++i) {
        const float x = static_cast<float>(in[i] - mean);
        re_[i] = (x - config_.preemphasis * (i == 0 ? x : prev)) * window_[i];
        prev = x;
    }
    Fft(re_, im_);
    for (int m = 0; m < config_.num_mels; ++m) {
        double sum = 0.0;
        const float* f = &filters_[static_cast<size_t>(m) * bins];
        for (int b = 0; b < bins; ++b)
            sum += f[b] * (re_[b] * re_[b] + im_[b] * im_[b]);
        out[m] = static_cast<float>(std::log(std::max(sum, 1.1920929e-7)));
    }
}

std::vector<float> LogMel::Accept(const float* samples, size_t count) {
    pending_.insert(pending_.end(), samples, samples + count);
    std::vector<float> out;
    const size_t len = static_cast<size_t>(config_.frame_length),
                 shift = static_cast<size_t>(config_.frame_shift);
    size_t pos = 0;
    while (pending_.size() - pos >= len) {
        const size_t at = out.size();
        out.resize(at + config_.num_mels);
        Frame(&pending_[pos], &out[at]);
        pos += shift;
        ++frames_;
    }
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(pos));
    return out;
}

}  // namespace mutqin
