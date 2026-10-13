// C++ log-mel against tools/train/features.py (numpy), used to train the model, on a test signal.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "features/logmel.h"

TEST(FeaturesParity, CppEqualsNumpy) {
    const char* path = std::getenv("MUTQIN_PY_FEATURES_DUMP");
    ASSERT_NE(path, nullptr);
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in.good());
    int32_t n = 0, frames = 0;
    in.read(reinterpret_cast<char*>(&n), 4);
    in.read(reinterpret_cast<char*>(&frames), 4);
    std::vector<float> samples(static_cast<size_t>(n)), want(static_cast<size_t>(frames) * 80);
    in.read(reinterpret_cast<char*>(samples.data()), static_cast<std::streamsize>(samples.size() * 4));
    in.read(reinterpret_cast<char*>(want.data()), static_cast<std::streamsize>(want.size() * 4));
    ASSERT_TRUE(in.good());

    mutqin::LogMel fe;
    const std::vector<float> got = fe.Accept(samples.data(), samples.size());
    ASSERT_EQ(got.size(), want.size());
    double worst = 0.0;
    for (size_t i = 0; i < got.size(); ++i)
        worst = std::fmax(worst, std::fabs(got[i] - want[i]));
    EXPECT_LT(worst, 2e-3) << "largest difference " << worst;
}
