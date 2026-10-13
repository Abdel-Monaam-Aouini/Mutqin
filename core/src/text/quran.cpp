#include "text/quran.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "text/sha256.h"
#include "text/utf8.h"

namespace mutqin {
namespace {

constexpr int kExpectedSurahs = 114;
constexpr int kExpectedAyahs = 6236;
constexpr size_t kBasmalaTokens = 4;  // taken from 1:1 at load time; checked, not typed
constexpr const char* kUthmaniFile = "quran-uthmani.txt";
constexpr const char* kSimpleFile = "quran-simple-clean.txt";
constexpr const char* kChecksumsFile = "checksums.txt";
const char kShaddah[] = "\xD9\x91";  // U+0651
// Surahs whose first basmala token has one extra shaddah vs 1:1 in the Tanzil Uthmani text.
constexpr int kShaddahSurahs[] = {95, 97};

struct MarkRange { uint32_t lo, hi; };
constexpr MarkRange kMarkRanges[] = {{0x0610, 0x061A}, {0x064B, 0x065F}, {0x06D6, 0x06ED}};

struct DataError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

bool ReadFile(const std::string& path, std::string* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// Byte length of the first UTF-8 character of a non-empty string.
size_t FirstCharBytes(const std::string& s) {
    size_t n = 1;
    while (n < s.size() && (static_cast<uint8_t>(s[n]) & 0xC0) == 0x80) ++n;
    return n;
}

// Same semantics as Python's str.split(sep): "" -> [""], keeps empty tokens.
std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t pos = s.find(sep, start);
        if (pos == std::string::npos) { parts.push_back(s.substr(start)); break; }
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

using AyahKey = std::pair<int, int>;
using Lines = std::vector<std::pair<AyahKey, std::string>>;

bool ParseDigits(const std::string& line, size_t* pos, int* value) {
    size_t start = *pos;
    long v = 0;
    while (*pos < line.size() && line[*pos] >= '0' && line[*pos] <= '9') {
        v = v * 10 + (line[*pos] - '0');
        if (*pos - start > 6) return false;
        ++(*pos);
    }
    if (*pos == start) return false;
    *value = static_cast<int>(v);
    return true;
}

// Matches "<digits>|<digits>|<non-empty text>".
bool MatchDataLine(const std::string& line, AyahKey* key, std::string* text) {
    size_t pos = 0;
    int surah, ayah;
    if (!ParseDigits(line, &pos, &surah) || pos >= line.size() || line[pos] != '|') return false;
    ++pos;
    if (!ParseDigits(line, &pos, &ayah) || pos >= line.size() || line[pos] != '|') return false;
    ++pos;
    if (pos >= line.size()) return false;
    *key = {surah, ayah};
    *text = line.substr(pos);
    return true;
}

Lines ParseLines(const std::string& dir, const std::string& name) {
    std::string raw;
    if (!ReadFile(dir + "/" + name, &raw)) throw DataError(name + ": cannot read file");
    if (raw.compare(0, 3, "\xEF\xBB\xBF") == 0) throw DataError(name + ": unexpected BOM");
    std::vector<uint32_t> scratch;
    if (!DecodeUtf8(raw, &scratch)) throw DataError(name + ": invalid UTF-8");

    Lines result;
    std::set<AyahKey> seen;
    bool in_footer = false;
    int lineno = 0;
    for (const std::string& line : Split(raw, '\n')) {
        ++lineno;
        AyahKey key;
        std::string text;
        if (!in_footer && MatchDataLine(line, &key, &text)) {
            if (!seen.insert(key).second) {
                throw DataError(name + ":" + std::to_string(lineno) + ": duplicate ayah");
            }
            result.emplace_back(key, std::move(text));
        } else if (line.empty() || line[0] == '#') {
            in_footer = in_footer || !result.empty();
        } else {
            throw DataError(name + ":" + std::to_string(lineno) + ": unexpected line");
        }
    }
    return result;
}

std::map<std::string, std::string> ReadChecksums(const std::string& path) {
    std::string raw;
    std::map<std::string, std::string> result;
    if (!ReadFile(path, &raw)) return result;
    for (const std::string& line : Split(raw, '\n')) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        size_t sep = line.find("  ");
        std::string digest = line.substr(0, sep);
        std::string name = sep == std::string::npos ? "" : line.substr(sep + 2);
        auto trim = [](std::string s) {
            size_t b = s.find_first_not_of(" \t\r"), e = s.find_last_not_of(" \t\r");
            return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
        };
        result[trim(name)] = trim(digest);
    }
    return result;
}

void VerifyChecksums(const std::string& dir) {
    auto recorded = ReadChecksums(dir + "/" + kChecksumsFile);
    if (recorded.empty()) throw DataError(std::string(kChecksumsFile) + " missing or empty");
    for (const char* name : {kUthmaniFile, kSimpleFile}) {
        auto it = recorded.find(name);
        if (it == recorded.end()) throw DataError(std::string(name) + ": no checksum recorded");
        std::string data;
        if (!ReadFile(dir + "/" + name, &data)) throw DataError(std::string(name) + ": file missing");
        if (Sha256Hex(data) != it->second) {
            throw DataError(std::string(name) + ": checksum mismatch (file modified)");
        }
    }
}

std::map<int, int> CheckNumbering(const Lines& lines, const std::string& name) {
    std::map<int, int> counts;
    for (const auto& entry : lines) {
        const int surah = entry.first.first, ayah = entry.first.second;
        auto it = counts.find(surah);
        if (it == counts.end()) {
            if (surah != static_cast<int>(counts.size()) + 1) {
                throw DataError(name + ": surah " + std::to_string(surah) + " out of order");
            }
            it = counts.emplace(surah, 0).first;
        } else if (surah != static_cast<int>(counts.size())) {
            throw DataError(name + ": surah " + std::to_string(surah) + " not contiguous");
        }
        if (ayah != ++it->second) {
            throw DataError(name + ": ayah " + std::to_string(surah) + ":" + std::to_string(ayah) +
                            " out of order");
        }
    }
    return counts;
}

bool InShaddahList(int surah) {
    for (int s : kShaddahSurahs) if (s == surah) return true;
    return false;
}

// Splits the basmala prefix off ayah 1, checking it against the 1:1 tokens.
std::vector<std::string> SplitBasmala(int surah, std::vector<std::string>* tokens,
                                      const std::vector<std::string>& reference) {
    if (surah == 1 || surah == 9) return {};
    if (tokens->size() <= kBasmalaTokens) {
        throw DataError(std::to_string(surah) + ":1: basmala prefix does not match 1:1");
    }
    std::vector<std::string> prefix(tokens->begin(), tokens->begin() + kBasmalaTokens);
    std::vector<std::string> expected = reference;
    if (InShaddahList(surah) && !expected.empty() && !expected[0].empty()) {
        size_t n = FirstCharBytes(expected[0]);
        expected[0] = expected[0].substr(0, n) + kShaddah + expected[0].substr(n);
    }
    if (prefix != expected) {
        throw DataError(std::to_string(surah) + ":1: basmala prefix does not match 1:1");
    }
    tokens->erase(tokens->begin(), tokens->begin() + kBasmalaTokens);
    return prefix;
}

std::vector<Word> BuildWords(int surah, int ayah, const std::vector<std::string>& tokens) {
    std::vector<Word> words;
    for (const std::string& token : tokens) {
        if (IsMarkOnly(token)) {
            if (words.empty()) {
                throw DataError(std::to_string(surah) + ":" + std::to_string(ayah) +
                                ": mark token before the first word");
            }
            words.back().marks.push_back(token);
            continue;
        }
        Word w;
        w.surah = surah;
        w.ayah = ayah;
        w.index = static_cast<int>(words.size()) + 1;
        w.text = token;
        words.push_back(std::move(w));
    }
    return words;
}

std::string Join(const std::vector<std::string>& parts, char sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out.push_back(sep);
        out += parts[i];
    }
    return out;
}

}  // namespace

std::string Word::Id() const {
    return std::to_string(surah) + ":" + std::to_string(ayah) + ":" + std::to_string(index);
}

bool IsMarkOnly(const std::string& token) {
    if (token.empty()) return false;
    std::vector<uint32_t> cps;
    if (!DecodeUtf8(token, &cps)) return false;
    for (uint32_t cp : cps) {
        bool in_range = false;
        for (const MarkRange& r : kMarkRanges) in_range = in_range || (cp >= r.lo && cp <= r.hi);
        if (!in_range) return false;
    }
    return true;
}

bool Quran::Load(const std::string& data_dir, Quran* out, std::string* error) {
    try {
        VerifyChecksums(data_dir);
        Lines uthmani = ParseLines(data_dir, kUthmaniFile);
        Lines simple = ParseLines(data_dir, kSimpleFile);

        if (uthmani.size() != simple.size()) throw DataError("files list different ayahs");
        for (size_t i = 0; i < uthmani.size(); ++i) {
            if (uthmani[i].first != simple[i].first) throw DataError("files list different ayahs");
        }
        std::map<int, int> counts = CheckNumbering(uthmani, kUthmaniFile);
        if (static_cast<int>(counts.size()) != kExpectedSurahs ||
            static_cast<int>(uthmani.size()) != kExpectedAyahs) {
            throw DataError("expected 114 surahs / 6236 ayahs, got " + std::to_string(counts.size()) +
                            " / " + std::to_string(uthmani.size()));
        }

        std::vector<std::string> reference = Split(uthmani[0].second, ' ');
        if (reference.size() > kBasmalaTokens) reference.resize(kBasmalaTokens);

        Quran quran;
        size_t pos = 0;
        for (int number = 1; number <= kExpectedSurahs; ++number) {
            Surah surah;
            surah.number = number;
            for (int a = 1; a <= counts[number]; ++a, ++pos) {
                std::vector<std::string> tokens = Split(uthmani[pos].second, ' ');
                if (a == 1) surah.basmala = SplitBasmala(number, &tokens, reference);
                Ayah ayah;
                ayah.surah = number;
                ayah.ayah = a;
                ayah.words = BuildWords(number, a, tokens);
                ayah.simple_text = simple[pos].second;
                surah.ayahs.push_back(std::move(ayah));
            }
            quran.surahs_.push_back(std::move(surah));
        }
        *out = std::move(quran);
        return true;
    } catch (const DataError& e) {
        if (error) *error = e.what();
        return false;
    }
}

size_t Quran::ayah_count() const {
    size_t n = 0;
    for (const Surah& s : surahs_) n += s.ayahs.size();
    return n;
}

const Ayah* Quran::FindAyah(int surah, int ayah) const {
    if (surah < 1 || surah > static_cast<int>(surahs_.size())) return nullptr;
    const Surah& s = surahs_[surah - 1];
    if (ayah < 1 || ayah > static_cast<int>(s.ayahs.size())) return nullptr;
    return &s.ayahs[ayah - 1];
}

std::string Quran::DumpTsv() const {
    std::string out;
    for (const Surah& s : surahs_) {
        out += "S\t" + std::to_string(s.number) + "\t" + Join(s.basmala, ' ') + "\n";
        for (const Ayah& a : s.ayahs) {
            out += "A\t" + std::to_string(a.surah) + ":" + std::to_string(a.ayah) + "\t" +
                   a.simple_text + "\n";
            for (const Word& w : a.words) {
                out += "W\t" + w.Id() + "\t" + w.text + "\t" + Join(w.marks, ' ') + "\n";
            }
        }
    }
    return out;
}

}  // namespace mutqin
