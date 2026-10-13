#include <gtest/gtest.h>

#include <cstring>

#include "mutqin.h"

TEST(Version, ReturnsNonEmptyString) {
    const char* v = mq_version();
    ASSERT_NE(v, nullptr);
    EXPECT_GT(std::strlen(v), 0u);
}

TEST(Version, MatchesProjectVersion) {
    EXPECT_STREQ(mq_version(), "0.1.0");
}
