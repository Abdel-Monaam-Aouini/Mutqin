// Parity test: the C++ phonemizer must reproduce tools/phonemizer/dump.py exactly over the whole
// Quran (every item, tag, duration, variant and diagnostic, three readings, and a hash of every
// ayah phonemized on its own). CTest generates the Python dump first (MUTQIN_PY_PHONEMIZER_DUMP).
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "tajweed/phonemizer.h"
#include "text/quran.h"

namespace {

std::vector<std::string> Lines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string line; std::getline(in, line);)
        out.push_back(line);
    return out;
}

}  // namespace

TEST(PhonemizerParity, CppEqualsPythonOverTheWholeQuran) {
    const char* path = std::getenv("MUTQIN_PY_PHONEMIZER_DUMP");
    if (!path)
        GTEST_SKIP() << "MUTQIN_PY_PHONEMIZER_DUMP not set (run through ctest)";
    std::ifstream in(path, std::ios::binary);
    ASSERT_TRUE(in) << "cannot read " << path;
    const std::string py((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    mutqin::Quran quran;
    std::string error;
    ASSERT_TRUE(mutqin::Quran::Load(MUTQIN_DATA_DIR, &quran, &error)) << error;
    const std::string cpp = mutqin::DumpParity(quran);

    if (cpp != py) {
        const auto a = Lines(cpp), b = Lines(py);
        size_t i = 0;
        while (i < a.size() && i < b.size() && a[i] == b[i])
            ++i;
        ADD_FAILURE() << "first difference at line " << (i + 1) << " (C++ " << a.size() << " lines, Python "
                      << b.size() << " lines)\n  C++:    " << (i < a.size() ? a[i].substr(0, 300) : "<end>")
                      << "\n  Python: " << (i < b.size() ? b[i].substr(0, 300) : "<end>");
    }
    EXPECT_GT(cpp.size(), 20000000u);
}
