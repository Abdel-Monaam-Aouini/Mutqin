#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "wav.h"

using namespace mutqin_cli;

namespace {

void Put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void Put16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v));
    b.push_back(static_cast<uint8_t>(v >> 8));
}

std::vector<uint8_t> MakeWav(int rate, int channels, const std::vector<int16_t>& pcm) {
    std::vector<uint8_t> b = {'R', 'I', 'F', 'F'};
    Put32(b, static_cast<uint32_t>(36 + pcm.size() * 2));
    for (char c : {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '})
        b.push_back(static_cast<uint8_t>(c));
    Put32(b, 16);
    Put16(b, 1);
    Put16(b, static_cast<uint16_t>(channels));
    Put32(b, static_cast<uint32_t>(rate));
    Put32(b, static_cast<uint32_t>(rate * channels * 2));
    Put16(b, static_cast<uint16_t>(channels * 2));
    Put16(b, 16);
    for (char c : {'d', 'a', 't', 'a'})
        b.push_back(static_cast<uint8_t>(c));
    Put32(b, static_cast<uint32_t>(pcm.size() * 2));
    for (int16_t s : pcm)
        Put16(b, static_cast<uint16_t>(s));
    return b;
}

}  // namespace

TEST(Wav, ReadsMono16BitPcm) {
    Wav w;
    std::string err;
    ASSERT_TRUE(ParseWav(MakeWav(16000, 1, {0, 16384, -32768}), &w, &err)) << err;
    ASSERT_EQ(w.samples.size(), 3u);
    EXPECT_FLOAT_EQ(w.samples[1], 0.5f);
    EXPECT_FLOAT_EQ(w.samples[2], -1.0f);
}

TEST(Wav, DownmixesStereo) {
    Wav w;
    std::string err;
    ASSERT_TRUE(ParseWav(MakeWav(16000, 2, {16384, -16384, 16384, 16384}), &w, &err)) << err;
    ASSERT_EQ(w.samples.size(), 2u);
    EXPECT_FLOAT_EQ(w.samples[0], 0.0f);
    EXPECT_FLOAT_EQ(w.samples[1], 0.5f);
}

TEST(Wav, RejectsWrongRateAndGarbage) {
    Wav w;
    std::string err;
    EXPECT_FALSE(ParseWav(MakeWav(44100, 1, {0, 0}), &w, &err));
    EXPECT_NE(err.find("ffmpeg"), std::string::npos);
    EXPECT_FALSE(ParseWav({1, 2, 3}, &w, &err));
    EXPECT_FALSE(ReadWavFile("/does/not/exist.wav", &w, &err));
}
