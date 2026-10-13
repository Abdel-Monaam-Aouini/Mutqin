#include <gtest/gtest.h>

#include <string>

#include "text/sha256.h"

// Standard FIPS 180-4 / NIST test vectors.
TEST(Sha256, EmptyString) {
    EXPECT_EQ(mutqin::Sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256, Abc) {
    EXPECT_EQ(mutqin::Sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256, TwoBlockMessage) {
    EXPECT_EQ(mutqin::Sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(Sha256, OneMillionA) {
    EXPECT_EQ(mutqin::Sha256Hex(std::string(1000000, 'a')),
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Sha256, PaddingBoundaries) {
    // 55, 56 and 64 bytes straddle the single-/double-block padding boundary; must not crash and
    // must be distinct.
    EXPECT_NE(mutqin::Sha256Hex(std::string(55, 'x')), mutqin::Sha256Hex(std::string(56, 'x')));
    EXPECT_NE(mutqin::Sha256Hex(std::string(56, 'x')), mutqin::Sha256Hex(std::string(64, 'x')));
}
