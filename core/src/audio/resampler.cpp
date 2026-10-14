#include "audio/resampler.h"

#include <algorithm>
#include <cmath>

namespace mutqin {

namespace {
const double kPi = 3.14159265358979323846;
constexpr int kZeroCrossings = 16;     // sinc lobes on each side of the centre, at the output rate
constexpr int kTableResolution = 256;  // filter samples per input sample; linear interpolation between them
}  // namespace

Resampler::Resampler(int input_rate)
    : input_rate_(input_rate),
      ratio_(static_cast<double>(input_rate) / 16000.0),
      half_width_(static_cast<int>(
          std::ceil(kZeroCrossings * std::max(1.0, static_cast<double>(input_rate) / 16000.0)))) {
    if (input_rate_ == 16000)
        return;
    // Low-pass at the lower of the two Nyquist rates, windowed by a Hann window that is zero at +-half_width.
    // Tabulating it once avoids a sine and a cosine per tap and per output sample (the cost that mattered on
    // phones).
    const double cutoff = std::min(1.0, 1.0 / ratio_);
    const int count = 2 * half_width_ * kTableResolution + 2;
    table_.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const double x = static_cast<double>(i) / kTableResolution - half_width_;
        const double arg = cutoff * x;
        const double sinc = std::fabs(arg) < 1e-9 ? 1.0 : std::sin(kPi * arg) / (kPi * arg);
        const double window = 0.5 + 0.5 * std::cos(kPi * x / half_width_);
        table_[static_cast<size_t>(i)] = static_cast<float>(cutoff * sinc * window);
    }
}

void Resampler::Reset() {
    buffer_.clear();
    base_ = 0;
    out_index_ = 0;
    started_ = false;
}

void Resampler::Accept(const float* samples, size_t count, std::vector<float>* out) {
    if (input_rate_ == 16000) {
        out->insert(out->end(), samples, samples + count);
        return;
    }
    if (!started_) {
        // Pretend there is silence before the first sample so the first output is centred on sample 0.
        buffer_.assign(static_cast<size_t>(half_width_), 0.0f);
        base_ = 0;
        started_ = true;
    }
    buffer_.insert(buffer_.end(), samples, samples + count);
    const long long width = half_width_;
    // Output n is centred on absolute input index n * ratio + width (the padding counts). Computing it
    // from n, not by adding up, makes the result independent of how the input is chunked.
    for (;;) {
        const double position = static_cast<double>(out_index_) * ratio_ + static_cast<double>(width);
        const long long centre = static_cast<long long>(std::floor(position));
        if (centre + width >= base_ + static_cast<long long>(buffer_.size()))
            break;
        const double frac = position - static_cast<double>(centre);
        double sum = 0.0, weight_sum = 0.0;
        const float* input = buffer_.data() + (centre - base_);
        for (long long k = -width + 1; k <= width; ++k) {
            const double at = (static_cast<double>(k) - frac + static_cast<double>(width)) * kTableResolution;
            const size_t i = static_cast<size_t>(at);
            const double t = at - static_cast<double>(i);
            const double w = table_[i] + t * (table_[i + 1] - table_[i]);  // distance from the output instant
            sum += w * input[k];
            weight_sum += w;
        }
        out->push_back(static_cast<float>(sum / weight_sum));  // unity gain at DC
        ++out_index_;
    }
    // drop the input that no later output can use
    const long long next_centre = static_cast<long long>(
        std::floor(static_cast<double>(out_index_) * ratio_ + static_cast<double>(width)));
    const long long drop = next_centre - width - base_;
    if (drop > 0) {
        buffer_.erase(buffer_.begin(), buffer_.begin() + drop);
        base_ += drop;
    }
}

}  // namespace mutqin
