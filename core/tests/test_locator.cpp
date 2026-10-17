// Locating a recitation without knowing the surah: synthetic recitations (units of the expected reading with
// substitutions, deletions and insertions) must be placed at the right ayah.
#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "align/ctc.h"
#include "align/locator.h"
#include "api/session_internal.h"
#include "inference/units.h"
#include "tajweed/phonemizer.h"
#include "text/quran.h"

using namespace mutqin;

namespace {

const std::string kDataDir = MUTQIN_DATA_DIR;

// The groups of the whole Quran from the phonemizer: one group per word.
bool PhonemizerGroups(std::vector<UnitGroup>* groups) {
    Quran quran;
    std::string error;
    if (!Quran::Load(kDataDir, &quran, &error))
        return false;
    for (const Surah& surah : quran.surahs()) {
        std::vector<const Ayah*> ayahs;
        std::map<std::pair<int, int>, int> lens;
        for (const Ayah& a : surah.ayahs) {
            ayahs.push_back(&a);
            lens[{a.surah, a.ayah}] = static_cast<int>(a.words.size());
        }
        PhonemizeResult result;
        if (!PhonemizeRange(ayahs, &result, &error))
            return false;
        for (const WordRef& w : BuildWordReadings(result.items, lens)) {
            UnitGroup g;
            g.surah = std::get<0>(w.key);
            g.ayah = std::get<1>(w.key);
            g.first_word = g.last_word = std::get<2>(w.key);
            g.units = w.reading.cont;
            groups->push_back(std::move(g));
        }
    }
    return true;
}

// `length` heard units starting `offset` units into ayah (surah, ayah) of the groups, with mistakes.
std::vector<HeardToken> Recite(const std::vector<UnitGroup>& groups, size_t first_group, size_t length,
                               double error_rate, std::mt19937& rng,
                               const std::vector<std::string>& alphabet) {
    std::vector<HeardToken> heard;
    std::uniform_real_distribution<double> coin(0, 1);
    std::uniform_int_distribution<size_t> any(0, alphabet.size() - 1);
    int frame = 0;
    for (size_t g = first_group; g < groups.size() && heard.size() < length; ++g) {
        for (const std::string& unit : groups[g].units) {
            const double r = coin(rng);
            std::string symbol = unit;
            if (r < error_rate / 3)
                continue;  // deleted
            if (r < 2 * error_rate / 3)
                symbol = alphabet[any(rng)];  // wrong
            HeardToken t;
            t.symbol = symbol;
            t.confidence = 1.0;
            t.start = frame;
            t.end = frame + 4;
            frame += 4;
            heard.push_back(t);
            if (r >= 2 * error_rate / 3 && r < error_rate) {  // an extra unit after it
                t.symbol = alphabet[any(rng)];
                t.start = frame;
                t.end = frame + 4;
                frame += 4;
                heard.push_back(t);
            }
        }
    }
    return heard;
}

struct Rates {
    double right, wrong;
};

Rates SuccessRate(const std::vector<UnitGroup>& groups, const Locator& locator, size_t length,
                  double error_rate, int trials, unsigned seed) {
    std::vector<std::string> alphabet;
    {
        std::map<std::string, int> seen;
        for (const UnitGroup& g : groups)
            for (const std::string& u : g.units)
                if (seen.emplace(u, 1).second)
                    alphabet.push_back(u);
    }
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> pick(0, groups.size() - 40);
    int right = 0, wrong = 0;
    for (int t = 0; t < trials; ++t) {
        size_t g = pick(rng);
        // start at the first word of an ayah, as a recitation of an ayah does
        while (g > 0 && groups[g - 1].ayah == groups[g].ayah && groups[g - 1].surah == groups[g].surah)
            --g;
        const LocateResult r = locator.Locate(Recite(groups, g, length, error_rate, rng, alphabet));
        if (!r.found)
            continue;
        (r.surah == groups[g].surah && r.ayah == groups[g].ayah ? right : wrong)++;
    }
    std::printf("  heard %zu units, %.0f%% mistakes: right %d, wrong %d, not yet %d of %d\n", length,
                error_rate * 100, right, wrong, trials - right - wrong, trials);
    return {static_cast<double>(right) / trials, static_cast<double>(wrong) / trials};
}

}  // namespace

TEST(Locator, FindsTheAyahOfASyntheticRecitationFromThePhonemizerUnits) {
    std::vector<UnitGroup> groups;
    if (!PhonemizerGroups(&groups))
        GTEST_SKIP() << "data/quran missing";
    LocatorParams params;
    params.gram = 7;  // the phonemizer has only about 30 symbols, so longer runs are needed
    const Locator locator(groups, 0, params);
    const Rates a = SuccessRate(groups, locator, 40, 0.10, 200, 1);
    EXPECT_GT(a.right, 0.55);
    EXPECT_LT(a.wrong, 0.03);
    const Rates b = SuccessRate(groups, locator, 80, 0.10, 200, 2);
    EXPECT_GT(b.right, 0.8);
    EXPECT_LT(b.wrong, 0.03);
}

TEST(Locator, NeverPlacesTheBasmalaAlone) {
    std::vector<UnitGroup> groups;
    if (!PhonemizerGroups(&groups))
        GTEST_SKIP() << "data/quran missing";
    LocatorParams params;
    params.gram = 7;
    const Locator locator(groups, 0, params);
    std::vector<HeardToken> heard;
    for (const UnitGroup& g : groups) {
        if (g.surah != 1 || g.ayah != 1)
            break;
        for (const std::string& u : g.units) {
            HeardToken t;
            t.symbol = u;
            heard.push_back(t);
        }
    }
    ASSERT_GT(heard.size(), 14u);
    EXPECT_FALSE(locator.Locate(heard).found);
}

// Needs the model's table (not in the repository):
// MUTQIN_UNITS_TABLE=models/zipformer_p-quran/expected_units.tsv
TEST(Locator, FindsTheAyahFromTheModelsUnits) {
    const char* path = std::getenv("MUTQIN_UNITS_TABLE");
    if (path == nullptr)
        GTEST_SKIP() << "MUTQIN_UNITS_TABLE not set";
    std::vector<UnitGroup> groups;
    std::string error;
    ASSERT_TRUE(LoadAllUnitGroups(path, &groups, &error)) << error;
    // the heard units are cut like the table's (runs of one unit at most 2)
    std::vector<UnitGroup> cut = groups;
    for (UnitGroup& g : cut)
        for (std::string& u : g.units)
            u = CollapseRuns(u, 2);
    const Locator locator(groups, 2);
    const Rates a = SuccessRate(cut, locator, 30, 0.10, 400, 7);
    EXPECT_GT(a.right, 0.65);
    EXPECT_LT(a.wrong, 0.03);
    const Rates b = SuccessRate(cut, locator, 50, 0.10, 400, 7);
    EXPECT_GT(b.right, 0.8);
    EXPECT_LT(b.wrong, 0.03);
}

namespace {

// Returns more of a scripted hypothesis with every call, as a recognizer does while someone recites.
class GrowingRecognizer : public TokenRecognizer {
public:
    GrowingRecognizer(std::vector<HeardToken> all, size_t step) : all_(std::move(all)), step_(step) {}
    void Accept(const float*, size_t, std::vector<HeardToken>* tokens, size_t* from) override {
        shown_ = std::min(all_.size(), shown_ + step_);
        tokens->assign(all_.begin(), all_.begin() + static_cast<std::ptrdiff_t>(shown_));
        *from = 0;
    }
    void Finish(std::vector<HeardToken>* tokens, size_t* from) override {
        shown_ = all_.size();
        *tokens = all_;
        *from = 0;
    }
    void Reset() override { shown_ = 0; }

private:
    std::vector<HeardToken> all_;
    size_t step_, shown_ = 0;
};

}  // namespace

TEST(AutoSession, FindsTheAyahThenFollowsTheRecitationToTheEndOfTheSurah) {
    std::vector<UnitGroup> groups;
    if (!PhonemizerGroups(&groups))
        GTEST_SKIP() << "data/quran missing";
    // the table the session reads its range from: the phonemizer's readings, in the model's table format
    const std::string table = ::testing::TempDir() + "/auto_units.tsv";
    {
        std::ofstream out(table);
        for (const UnitGroup& g : groups) {
            out << g.surah << ":" << g.ayah << "\t" << g.first_word << "-" << g.last_word << "\t";
            for (size_t i = 0; i < g.units.size(); ++i)
                out << (i ? " " : "") << g.units[i];
            out << "\n";
        }
    }
    // a recitation of 105:2 to the end of the surah
    std::vector<HeardToken> heard;
    for (const UnitGroup& g : groups) {
        if (g.surah != 105 || g.ayah < 2)
            continue;
        for (const std::string& unit : g.units) {
            HeardToken t;
            t.symbol = unit;
            t.confidence = 1.0;
            t.start = static_cast<int>(heard.size()) * 4;
            t.end = t.start + 4;
            heard.push_back(t);
        }
    }
    ASSERT_GT(heard.size(), 30u);

    char err[256];
    mq_engine* engine = nullptr;
    ASSERT_EQ(mq_engine_create(kDataDir.c_str(), &engine, err, sizeof err), MQ_OK) << err;
    mq_session* session = nullptr;
    ASSERT_EQ(mq_session_create_auto(engine, &session, err, sizeof err), MQ_OK) << err;
    EXPECT_EQ(mq_session_word_count(session), 0u);
    EXPECT_EQ(mq_session_located(session, nullptr, nullptr), 0);

    SetSessionRecognizer(session, std::make_unique<GrowingRecognizer>(heard, 5));
    SetSessionAutoUnits(session, groups, table, 0);
    const float silence[160] = {};
    int surah = 0, ayah = 0;
    for (int i = 0; i < 200 && mq_session_located(session, &surah, &ayah) == 0; ++i)
        ASSERT_EQ(mq_session_push_audio(session, silence, 160), MQ_OK);
    ASSERT_EQ(mq_session_located(session, &surah, &ayah), 1) << "never located";
    EXPECT_EQ(surah, 105);
    EXPECT_EQ(ayah, 2);
    EXPECT_GT(mq_session_word_count(session), 0u);

    ASSERT_EQ(mq_session_finish_audio(session), MQ_OK);
    EXPECT_EQ(mq_session_event_count(session), 0u);
    for (size_t w = 0; w < mq_session_word_count(session); ++w)
        EXPECT_EQ(mq_session_word_status(session, w), MQ_WORD_OK) << w;

    // a reset forgets the place
    ASSERT_EQ(mq_session_reset(session), MQ_OK);
    EXPECT_EQ(mq_session_located(session, nullptr, nullptr), 0);
    EXPECT_EQ(mq_session_word_count(session), 0u);
    mq_session_destroy(session);
    mq_engine_destroy(engine);
}
