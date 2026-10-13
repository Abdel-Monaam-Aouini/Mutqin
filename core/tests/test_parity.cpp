// Parity test: the C++ loader must produce exactly the same dump as the Python reference
// (tools/quran_text/dump.py). CTest generates the Python dump first and passes its path in
// MUTQIN_PY_DUMP.
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "text/quran.h"

namespace {

std::vector<std::string> Lines(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string line; std::getline(in, line);) out.push_back(line);
    return out;
}

}  // namespace

TEST(QuranParity, CppDumpEqualsPythonDump) {
    const char* py_path = std::getenv("MUTQIN_PY_DUMP");
    if (!py_path) GTEST_SKIP() << "MUTQIN_PY_DUMP not set (run through ctest)";

    std::ifstream in(py_path, std::ios::binary);
    ASSERT_TRUE(in) << "cannot read " << py_path;
    std::string py((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    mutqin::Quran quran;
    std::string error;
    ASSERT_TRUE(mutqin::Quran::Load(MUTQIN_DATA_DIR, &quran, &error)) << error;
    const std::string cpp = quran.DumpTsv();

    if (cpp != py) {
        auto a = Lines(cpp), b = Lines(py);
        size_t i = 0;
        while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
        ADD_FAILURE() << "first difference at line " << (i + 1) << " (C++ " << a.size()
                      << " lines, Python " << b.size() << " lines)";
    }
    EXPECT_GT(cpp.size(), 1000000u);
}
