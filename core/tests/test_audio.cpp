#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "audio/ring_buffer.h"
#include "features/logmel.h"
#include "vad/energy_vad.h"

using namespace mutqin;

namespace {
std::vector<float> Tone(double hz, double seconds, double amp) {
    std::vector<float> x(static_cast<size_t>(16000 * seconds));
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = static_cast<float>(amp *
                                  std::sin(2.0 * 3.14159265358979 * hz * static_cast<double>(i) / 16000.0));
    return x;
}
}  // namespace

TEST(RingBuffer, KeepsOrderAcrossWrapAndRefusesOverflow) {
    RingBuffer rb(5);
    const float in[] = {1, 2, 3, 4, 5, 6, 7};
    EXPECT_EQ(rb.Push(in, 7), 5u);
    float out[8];
    EXPECT_EQ(rb.Pop(out, 3), 3u);
    EXPECT_EQ(out[0], 1.0f);
    EXPECT_EQ(rb.Push(in + 5, 2), 2u);
    EXPECT_EQ(rb.size(), 4u);
    EXPECT_EQ(rb.Pop(out, 8), 4u);
    EXPECT_EQ(out[0], 4.0f);
    EXPECT_EQ(out[2], 6.0f);
    EXPECT_EQ(out[3], 7.0f);
}

TEST(LogMel, FrameCountAndStreamingEqualsBatch) {
    const std::vector<float> x = Tone(440, 1.0, 0.5);
    LogMel whole;
    const std::vector<float> a = whole.Accept(x.data(), x.size());
    EXPECT_EQ(whole.frames_emitted(), 98u);  // 1 + (16000 - 400) / 160
    EXPECT_EQ(a.size(), 98u * 80u);

    LogMel chunked;
    std::vector<float> b;
    for (size_t i = 0; i < x.size(); i += 777) {
        const size_t n = std::min<size_t>(777, x.size() - i);
        const std::vector<float> part = chunked.Accept(x.data() + i, n);
        b.insert(b.end(), part.begin(), part.end());
    }
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i)
        ASSERT_FLOAT_EQ(a[i], b[i]);
}

TEST(LogMel, ToneEnergyPeaksInTheRightMelBin) {
    auto peak_bin = [](double hz) {
        const std::vector<float> x = Tone(hz, 0.2, 0.5);
        LogMel fe;
        const std::vector<float> f = fe.Accept(x.data(), x.size());
        int best = 0;
        for (int m = 1; m < 80; ++m)
            if (f[m] > f[best])
                best = m;
        return best;
    };
    const int low = peak_bin(300), mid = peak_bin(1000), high = peak_bin(4000);
    EXPECT_LT(low, mid);
    EXPECT_LT(mid, high);
}

TEST(EnergyVad, DetectsSpeechAndHoldsThroughShortPause) {
    EnergyVad vad;
    const std::vector<float> silence(160, 0.0f);
    const std::vector<float> speech = Tone(200, 0.01, 0.1);
    for (int i = 0; i < 20; ++i)
        EXPECT_FALSE(vad.ProcessBlock(silence.data()));
    for (int i = 0; i < 10; ++i)
        EXPECT_TRUE(vad.ProcessBlock(speech.data()));
    for (int i = 0; i < 20; ++i)
        EXPECT_TRUE(vad.ProcessBlock(silence.data()));  // inside hangover
    bool still = true;
    for (int i = 0; i < 30; ++i)
        still = vad.ProcessBlock(silence.data());
    EXPECT_FALSE(still);
}

#include "audio/resampler.h"

namespace {
std::vector<float> ToneAt(double hz, double rate, double seconds, double amp = 0.5) {
    std::vector<float> x(static_cast<size_t>(rate * seconds));
    for (size_t i = 0; i < x.size(); ++i)
        x[i] =
            static_cast<float>(amp * std::sin(2.0 * 3.14159265358979 * hz * static_cast<double>(i) / rate));
    return x;
}

double Rms(const std::vector<float>& x, size_t from, size_t to) {
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return std::sqrt(sum / static_cast<double>(to - from));
}
}  // namespace

TEST(Resampler, SameRateIsACopy) {
    Resampler r(16000);
    const std::vector<float> in = ToneAt(440, 16000, 0.1);
    std::vector<float> out;
    r.Accept(in.data(), in.size(), &out);
    EXPECT_EQ(out, in);
}

TEST(Resampler, KeepsALowToneAndTheLength) {
    for (const int rate : {8000, 44100, 48000}) {
        Resampler r(rate);
        const std::vector<float> in = ToneAt(440, rate, 1.0);
        std::vector<float> out;
        r.Accept(in.data(), in.size(), &out);
        EXPECT_NEAR(static_cast<double>(out.size()), 16000.0, 40.0) << rate;
        // after the start-up, the output is the same 440 Hz sine at 16 kHz (peak 0.5)
        double worst = 0.0;
        for (size_t i = 200; i + 200 < out.size(); ++i) {
            const double want =
                0.5 * std::sin(2.0 * 3.14159265358979 * 440.0 * static_cast<double>(i) / 16000.0);
            worst = std::fmax(worst, std::fabs(out[i] - want));
        }
        EXPECT_LT(worst, 0.02) << "rate " << rate;
    }
}

TEST(Resampler, RemovesToneAboveTheOutputNyquist) {
    Resampler r(48000);
    const std::vector<float> in = ToneAt(12000, 48000, 0.5);  // above 8 kHz: would alias without the filter
    std::vector<float> out;
    r.Accept(in.data(), in.size(), &out);
    EXPECT_LT(Rms(out, 200, out.size() - 200), 0.01);  // input RMS is 0.35
}

TEST(Resampler, OutputDoesNotDependOnChunking) {
    const std::vector<float> in = ToneAt(700, 44100, 0.5);
    Resampler whole(44100), chunked(44100);
    std::vector<float> a, b;
    whole.Accept(in.data(), in.size(), &a);
    for (size_t at = 0; at < in.size(); at += 313)
        chunked.Accept(in.data() + at, std::min<size_t>(313, in.size() - at), &b);
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i)
        ASSERT_FLOAT_EQ(a[i], b[i]);
}
