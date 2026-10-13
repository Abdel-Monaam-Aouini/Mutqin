// Compares the C++ aligner with the Python prototype on cases dumped by tools/prototype/dump_align.py.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "align/aligner.h"

using namespace mutqin;

namespace {

std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

std::vector<std::string> Words(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    for (std::string w; in >> w;)
        out.push_back(w);
    return out;
}

}  // namespace

TEST(AlignerParity, CppEqualsPython) {
    const char* path = std::getenv("MUTQIN_PY_ALIGN_DUMP");
    ASSERT_NE(path, nullptr) << "MUTQIN_PY_ALIGN_DUMP not set";
    std::ifstream in(path);
    ASSERT_TRUE(in.good()) << path;

    int cases = 0, checked_events = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("CASE ", 0) != 0)
            continue;
        const std::string id = line.substr(5);
        std::vector<WordReading> words;
        std::vector<HeardToken> heard;
        std::map<int, std::string> want_status;
        std::vector<AlignEvent> want_events;
        bool want_span = false;
        int want_first = 0, want_last = 0;
        while (std::getline(in, line) && line != "END") {
            if (line.rfind("W: ", 0) == 0) {
                const std::vector<std::string> parts = Split(line.substr(3), '|');
                ASSERT_EQ(parts.size(), 5u);
                WordReading w;
                w.cont = Words(parts[0]);
                for (int c = 0; c < 4; ++c)
                    w.contexts[c] = Words(parts[c + 1]);
                words.push_back(w);
            } else if (line.rfind("T: ", 0) == 0) {
                std::istringstream ls(line.substr(3));
                HeardToken t;
                ls >> t.symbol >> t.confidence >> t.start >> t.end;
                heard.push_back(t);
            } else if (line.rfind("SPAN ", 0) == 0) {
                std::istringstream ls(line.substr(5));
                std::string a;
                ls >> a;
                if (a != "none") {
                    want_span = true;
                    want_first = std::stoi(a);
                    ls >> want_last;
                }
            } else if (line.rfind("S ", 0) == 0) {
                std::istringstream ls(line.substr(2));
                int w;
                std::string s;
                ls >> w >> s;
                want_status[w] = s;
            } else if (line.rfind("E ", 0) == 0) {
                std::istringstream ls(line.substr(2));
                AlignEvent e;
                ls >> e.kind >> e.index >> e.confidence;
                want_events.push_back(e);
            }
        }
        const AlignResult got = AlignTokens(heard, words);
        ++cases;
        EXPECT_EQ(got.has_span, want_span) << "case " << id;
        if (want_span) {
            EXPECT_EQ(got.span_first, want_first) << "case " << id;
            EXPECT_EQ(got.span_last, want_last) << "case " << id;
        }
        EXPECT_EQ(got.statuses, want_status) << "case " << id;
        ASSERT_EQ(got.events.size(), want_events.size()) << "case " << id;
        for (size_t i = 0; i < want_events.size(); ++i) {
            EXPECT_EQ(got.events[i].kind, want_events[i].kind) << "case " << id;
            EXPECT_EQ(got.events[i].index, want_events[i].index) << "case " << id;
            EXPECT_NEAR(got.events[i].confidence, want_events[i].confidence, 1e-12) << "case " << id;
            ++checked_events;
        }
    }
    EXPECT_GE(cases, 100);
    EXPECT_GT(checked_events, 50);
}
