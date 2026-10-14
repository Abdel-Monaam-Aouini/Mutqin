// Micro-benchmark of the audio front end (task 3.7). Prints real-time factors (processing time /
// audio time; below 1 is faster than real time). Build with -DMUTQIN_BUILD_BENCH=ON, Release mode.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "audio/resampler.h"
#include "features/logmel.h"
#include "vad/energy_vad.h"

namespace {

double Seconds(std::chrono::steady_clock::time_point since) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
}

}  // namespace

int main(int argc, char** argv) {
    const double audio_seconds = argc > 1 ? std::atof(argv[1]) : 120.0;
    std::mt19937 rng(1);
    std::normal_distribution<float> noise(0.0f, 0.1f);
    std::vector<float> audio(static_cast<size_t>(audio_seconds * 16000));
    for (float& s : audio)
        s = noise(rng);

    {
        mutqin::LogMel fe;
        const auto t = std::chrono::steady_clock::now();
        size_t frames = 0;
        for (size_t at = 0; at < audio.size(); at += 1600)
            frames += fe.Accept(audio.data() + at, std::min<size_t>(1600, audio.size() - at)).size() / 80;
        const double s = Seconds(t);
        std::printf("log-mel features: %.1f s of audio -> %zu frames in %.3f s, real-time factor %.4f\n",
                    audio_seconds, frames, s, s / audio_seconds);
    }
    {
        mutqin::EnergyVad vad;
        const auto t = std::chrono::steady_clock::now();
        size_t active = 0;
        for (size_t at = 0; at + 160 <= audio.size(); at += 160)
            active += vad.ProcessBlock(audio.data() + at) ? 1 : 0;
        const double s = Seconds(t);
        std::printf("energy VAD:       %.1f s of audio, %zu active blocks in %.4f s, real-time factor %.5f\n",
                    audio_seconds, active, s, s / audio_seconds);
    }
    for (const int rate : {44100, 48000}) {
        mutqin::Resampler resampler(rate);
        std::vector<float> in(static_cast<size_t>(audio_seconds * rate)), out;
        for (float& s : in)
            s = noise(rng);
        const auto t = std::chrono::steady_clock::now();
        for (size_t at = 0; at < in.size(); at += 4410)
            resampler.Accept(in.data() + at, std::min<size_t>(4410, in.size() - at), &out);
        const double s = Seconds(t);
        std::printf("resampler %5d Hz: %.1f s of audio -> %zu samples in %.3f s, real-time factor %.4f\n",
                    rate, audio_seconds, out.size(), s, s / audio_seconds);
    }
    return 0;
}
