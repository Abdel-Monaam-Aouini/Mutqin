// Direct tests of the C++ phonemizer. Synthetic words use arbitrary code points that do not spell
// Quran words; Quran text is only read from data/quran/ (see the parity test).
#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "tajweed/inventory.h"
#include "tajweed/phonemizer.h"
#include "tajweed/uthmani.h"
#include "text/utf8.h"

using namespace mutqin;

namespace {

const std::string BA = "ب", TA = "ت", NOON = "ن", LAM = "ل", WAW = "و", YEH = "ي", ALEF = "ا", MAKSURA = "ى",
                  KAF = "ك", MEEM = "م", DAL = "د", HEH = "ه", HAMZA = "ء", WASLA = "ٱ", TEH_MARBUTA = "ة";
const std::string FATHA = "َ", DAMMA = "ُ", KASRA = "ِ", SHADDA = "ّ", SUKUN = "ْ", FATHATAN = "ً", DAMMATAN = "ٌ",
                  SUPERSCRIPT_ALEF = "ٰ", ROUNDED_ZERO = "۟";

Ayah MakeAyah(int surah, int ayah, std::initializer_list<std::string> words) {
    Ayah a;
    a.surah = surah;
    a.ayah = ayah;
    int i = 1;
    for (const std::string& w : words) {
        Word wd;
        wd.surah = surah;
        wd.ayah = ayah;
        wd.index = i++;
        wd.text = w;
        a.words.push_back(wd);
    }
    return a;
}

PhonemizeResult Phon(std::initializer_list<std::string> words) {
    const Ayah a = MakeAyah(1, 1, words);
    PhonemizeResult r;
    std::string error;
    EXPECT_TRUE(PhonemizeRange({&a}, &r, &error)) << error;
    return r;
}

// "ph" or "ph:dur" of the items of one reading.
std::vector<std::string> Reading(const PhonemizeResult& r, bool all_stops,
                                 const std::set<WordKey>& stops = {}) {
    std::vector<std::string> out;
    for (size_t i : SelectReading(r.items, stops, all_stops)) {
        std::string s = r.items[i].ph;
        for (size_t k = 0; k < r.items[i].dur.size(); ++k)
            s += (k ? "/" : ":") + std::to_string(r.items[i].dur[k]);
        out.push_back(s);
    }
    return out;
}
using V = std::vector<std::string>;

bool HasTag(const PhonemeItem& it, const char* tag) {
    for (const auto& r : it.rules) {
        if (r == tag)
            return true;
    }
    return false;
}

}  // namespace

TEST(Inventory, HasThirtyOneSymbolsAndStableTokenIds) {
    EXPECT_EQ(TokenId("<blank>"), 0);
    EXPECT_EQ(TokenId("hz"), 1);
    EXPECT_EQ(TokenId("u"), 31);
    EXPECT_EQ(TokenId("xx"), -1);
    EXPECT_STREQ(SymbolForLetter(0x0628), "b");
    EXPECT_EQ(SymbolForLetter(0x0627), nullptr);  // alef is not a consonant
}

TEST(Uthmani, SplitsUnitsAndSpans) {
    std::vector<uint32_t> cps;
    ASSERT_TRUE(DecodeUtf8(BA + FATHA + SHADDA + TA + SUKUN, &cps));
    const auto units = ParseWord(cps);
    ASSERT_EQ(units.size(), 2u);
    EXPECT_EQ(units[0].ch, 0x0628u);
    EXPECT_EQ(units[0].marks.size(), 2u);
    EXPECT_EQ(units[0].start, 0);
    EXPECT_EQ(units[0].end, 3);
    EXPECT_EQ(units[1].start, 3);
    EXPECT_TRUE(units[0].Has(kShadda));
    EXPECT_FALSE(units[0].Has(kSukun));
}

TEST(Uthmani, RejectsMarkWithoutBaseAndUnknownCharacters) {
    std::vector<uint32_t> cps;
    ASSERT_TRUE(DecodeUtf8(FATHA + BA, &cps));
    EXPECT_THROW(ParseWord(cps), UthmaniParseError);
    ASSERT_TRUE(DecodeUtf8("a", &cps));
    EXPECT_THROW(ParseWord(cps), UthmaniParseError);
    EXPECT_TRUE(ParseWord({}).empty());
}

TEST(Phonemizer, ConsonantsVowelsSukunShaddah) {
    EXPECT_EQ(Reading(Phon({BA + FATHA + TA + KASRA + LAM + DAMMA}), false),
              (V{"b", "a:1", "t", "i:1", "l", "u:1"}));
    EXPECT_EQ(Reading(Phon({TA + SUKUN}), false), (V{"t"}));
    EXPECT_EQ(Reading(Phon({TA + SHADDA + FATHA}), false), (V{"t", "t", "a:1"}));
}

TEST(Phonemizer, LongVowels) {
    EXPECT_EQ(Reading(Phon({BA + FATHA + ALEF + TA + FATHA + KAF + FATHA}), false),
              (V{"b", "a:2", "t", "a:1", "k", "a:1"}));
    EXPECT_EQ(Reading(Phon({BA + DAMMA + WAW + TA + FATHA}), false), (V{"b", "u:2", "t", "a:1"}));
    EXPECT_EQ(Reading(Phon({BA + KASRA + YEH + TA + FATHA}), false), (V{"b", "i:2", "t", "a:1"}));
    EXPECT_EQ(Reading(Phon({BA + FATHA + SUPERSCRIPT_ALEF + TA + FATHA}), false),
              (V{"b", "a:2", "t", "a:1"}));
    EXPECT_EQ(Reading(Phon({BA + FATHA + MAKSURA + TA + FATHA}), false), (V{"b", "a:2", "t", "a:1"}));
}

TEST(Phonemizer, SilentAlefAndTanween) {
    EXPECT_EQ(Reading(Phon({BA + DAMMA + WAW + ALEF + ROUNDED_ZERO}), false), (V{"b", "u:2"}));
    EXPECT_EQ(Reading(Phon({TA + FATHATAN}), false), (V{"t", "a:1", "n"}));
}

TEST(Phonemizer, UnexpectedPatternsAreDiagnosticsNotGuesses) {
    const auto r = Phon({BA + SUKUN + ALEF});  // alef after a consonant without fatha
    ASSERT_EQ(r.diagnostics.size(), 1u);
    EXPECT_EQ(r.diagnostics[0].kind, "alef_without_fatha");
    EXPECT_EQ(r.diagnostics[0].word_id, "1:1:1");
    const auto d = Phon({WAW + ALEF});  // no vowel anywhere: surah-opening letters, skipped
    EXPECT_EQ(d.diagnostics.at(0).kind, "disconnected_letters");
    EXPECT_TRUE(d.items.empty());
}

TEST(Phonemizer, ArticleLamAndHamzatWasl) {
    const auto r = Phon({WASLA + LAM + TA + SHADDA + FATHA});
    // the first word of a recitation is a start, so the article's hamzat al-wasl is pronounced
    EXPECT_EQ(Reading(r, false), (V{"hz", "a:1", "t", "t", "a:1"}));
    EXPECT_EQ(Reading(r, true), (V{"hz", "a:1", "t", "t"}));
    // after the first word, reading on drops it
    const auto two = Phon({BA + FATHA + TA + DAMMA, WASLA + LAM + TA + SHADDA + FATHA});
    EXPECT_EQ(Reading(two, false), (V{"b", "a:1", "t", "u:1", "t", "t", "a:1"}));
    EXPECT_TRUE(HasTag(r.items[0], "hamzat_wasl"));
}

TEST(Phonemizer, NoonRulesBetweenWords) {
    const auto ikhfa = Phon({BA + FATHA + NOON, KAF + FATHA});
    EXPECT_EQ(Reading(ikhfa, false), (V{"b", "a:1", "n:2", "k", "a:1"}));
    EXPECT_EQ(Reading(ikhfa, true), (V{"b", "a:1", "n", "k"}));  // a stop keeps the plain noon
    const auto iqlab = Phon({BA + FATHA + NOON, BA + FATHA});
    EXPECT_EQ(Reading(iqlab, false)[2], "m:2");
    const auto idgham = Phon({BA + FATHA + NOON, LAM + FATHA});
    EXPECT_EQ(Reading(idgham, false), (V{"b", "a:1", "l", "l", "a:1"}));
    EXPECT_EQ(Reading(idgham, false, {WordKey(1, 1, 1)}), (V{"b", "a:1", "n", "l", "a:1"}));
    const auto ghunnah = Phon({BA + FATHA + NOON, YEH + FATHA});
    EXPECT_EQ(Reading(ghunnah, false), (V{"b", "a:1", "y", "y:2", "a:1"}));
    const auto izhar = Phon({BA + FATHA + NOON + SUKUN, HEH + FATHA});
    EXPECT_EQ(Reading(izhar, false)[2], "n");
    EXPECT_TRUE(HasTag(izhar.items[2], "izhar_halqi"));
}

TEST(Phonemizer, MeemAndMushaddad) {
    EXPECT_EQ(Reading(Phon({BA + FATHA + MEEM, BA + FATHA}), false)[2], "m:2");
    const auto m = Phon({BA + FATHA + MEEM, MEEM + FATHA});
    EXPECT_EQ(Reading(m, false), (V{"b", "a:1", "m", "m:2", "a:1"}));
    EXPECT_EQ(Reading(Phon({BA + FATHA + NOON + SHADDA + FATHA}), false), (V{"b", "a:1", "n", "n:2", "a:1"}));
}

TEST(Phonemizer, QalqalahAtWordEndAndInside) {
    const auto inside = Phon({BA + FATHA + DAL + SUKUN + TA + FATHA});
    EXPECT_TRUE(HasTag(inside.items[2], "qalqalah_sughra"));
    const auto end = Phon({BA + FATHA + DAL + SUKUN, KAF + FATHA});
    EXPECT_EQ(end.items[2].variant, Variant::kContinueOnly);
    EXPECT_TRUE(HasTag(end.items[3], "qalqalah_kubra"));
    EXPECT_EQ(end.items[3].variant, Variant::kWaqfOnly);
}

TEST(Phonemizer, MaddTypes) {
    EXPECT_EQ(Reading(Phon({BA + FATHA + ALEF + HAMZA + FATHA}), false)[1], "a:4/5");  // muttasil
    const auto munfasil = Phon({BA + FATHA + ALEF, HAMZA + FATHA});
    EXPECT_EQ(Reading(munfasil, false)[1], "a:4/5");
    EXPECT_EQ(Reading(munfasil, true)[1], "a:2");
    EXPECT_EQ(Reading(Phon({BA + FATHA + ALEF + TA + SHADDA + FATHA}), false)[1], "a:6");  // lazim
    const auto arid = Phon({BA + FATHA + ALEF + TA + KASRA});
    EXPECT_EQ(Reading(arid, false)[1], "a:2");
    EXPECT_EQ(Reading(arid, true)[1], "a:2/4/6");
    const auto leen = Phon({BA + FATHA + WAW + SUKUN + TA + KASRA});
    EXPECT_EQ(Reading(leen, true)[2], "w:2/4/6");
}

TEST(Phonemizer, WaqfDropsFinalVowelsAndMakesTehMarbutaHeh) {
    EXPECT_EQ(Reading(Phon({BA + FATHA + TA + DAMMA}), true), (V{"b", "a:1", "t"}));
    EXPECT_EQ(Reading(Phon({BA + FATHA + TA + FATHATAN + ALEF}), true), (V{"b", "a:1", "t", "a:2"}));
    EXPECT_EQ(Reading(Phon({BA + FATHA + TEH_MARBUTA + KASRA}), true), (V{"b", "a:1", "h"}));
    EXPECT_EQ(Reading(Phon({BA + FATHA + TEH_MARBUTA + KASRA}), false), (V{"b", "a:1", "t", "i:1"}));
    EXPECT_EQ(Reading(Phon({BA + FATHA + ALEF}), true), (V{"b", "a:2"}));
}

TEST(Phonemizer, NothingContinuesAcrossASurahBoundary) {
    const Ayah a1 = MakeAyah(1, 1, {BA + FATHA + NOON});
    const Ayah a2 = MakeAyah(2, 1, {BA + FATHA});
    PhonemizeResult r;
    std::string error;
    ASSERT_TRUE(PhonemizeRange({&a1, &a2}, &r, &error));
    EXPECT_EQ(r.items[2].ph, "n");  // plain noon, no iqlab
    EXPECT_EQ(r.items[2].variant, Variant::kBoth);
}

TEST(Phonemizer, ReportsInvalidCharacters) {
    const Ayah a = MakeAyah(1, 1, {"a"});
    PhonemizeResult r;
    std::string error;
    EXPECT_FALSE(PhonemizeRange({&a}, &r, &error));
    EXPECT_NE(error.find("unexpected character"), std::string::npos);
}
