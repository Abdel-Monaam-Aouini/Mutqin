#include "tajweed/phonemizer.h"

#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <utility>

#include "tajweed/uthmani.h"
#include "text/sha256.h"
#include "text/utf8.h"

namespace mutqin {
namespace {

constexpr uint32_t kAlef = 0x0627, kAlefWasla = 0x0671, kAlefMaksura = 0x0649, kTehMarbuta = 0x0629;
constexpr uint32_t kWaw = 0x0648, kYeh = 0x064A, kTatweel = 0x0640;
constexpr uint32_t kSmallWaw = 0x06E5, kSmallYeh = 0x06E6, kSmallHighYeh = 0x06E7;
constexpr uint32_t kRoundedZero = 0x06DF, kRectangularZero = 0x06E0;
constexpr uint32_t kLam = 0x0644, kNoon = 0x0646, kMeem = 0x0645, kHeh = 0x0647;

const char* ShortVowel(uint32_t cp) {
    if (cp == kFatha)
        return "a";
    if (cp == kDamma)
        return "u";
    if (cp == kKasra)
        return "i";
    return nullptr;
}
const char* TanweenVowel(uint32_t cp) {
    if (cp == kFathatan)
        return "a";
    if (cp == kDammatan)
        return "u";
    if (cp == kKasratan)
        return "i";
    return nullptr;
}
bool IsSignMark(uint32_t cp) {
    return cp == 0x06E2 || cp == 0x06ED || cp == 0x06DC || cp == 0x06E3 || cp == 0x06E8 || cp == 0x06EA ||
           cp == 0x06EB || cp == 0x06EC;
}
bool IsHamzaSeat(uint32_t cp) {
    return cp >= 0x0623 && cp <= 0x0626;
}

std::string Hex4(uint32_t cp) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04X", cp);
    return buf;
}

bool IsDur(const std::vector<int>& d, std::initializer_list<int> v) {
    return d.size() == v.size() && std::equal(d.begin(), d.end(), v.begin());
}

bool HasRule(const PhonemeItem& it, const char* rule) {
    return std::find(it.rules.begin(), it.rules.end(), rule) != it.rules.end();
}

bool IsDisconnectedLetters(const std::vector<LetterUnit>& units) {
    if (units.empty())
        return false;
    for (const LetterUnit& u : units) {
        for (uint32_t m : u.marks) {
            if (ShortVowel(m) || TanweenVowel(m) || m == kSukun || m == kShadda)
                return false;
        }
    }
    return true;
}

// Marks that belong to the base letter: those before a hamza above.
std::set<uint32_t> MarksOfHost(const LetterUnit& unit) {
    std::set<uint32_t> out;
    for (uint32_t m : unit.marks) {
        if (m == kHamzaAbove)
            break;
        out.insert(m);
    }
    return out;
}

PhonemeItem Clone(const PhonemeItem& item, Variant variant, std::vector<std::string> rules,
                  std::vector<int> dur) {
    PhonemeItem c;
    c.ph = item.ph;
    c.word = item.word;
    c.span_start = item.span_start;
    c.span_end = item.span_end;
    c.role = item.role;
    c.dur = std::move(dur);
    c.rules = std::move(rules);
    c.variant = variant;
    c.surah = item.surah;
    c.ayah = item.ayah;
    c.host_marks = item.host_marks;
    c.letter = item.letter;
    c.maddah = item.maddah;
    c.long_by = item.long_by;
    return c;
}

// Python's Item(...) without maddah / long_by: used by the noon and qalqalah passes.
PhonemeItem Copy(const PhonemeItem& item, Variant variant, std::vector<std::string> rules) {
    PhonemeItem c;
    c.ph = item.ph;
    c.word = item.word;
    c.span_start = item.span_start;
    c.span_end = item.span_end;
    c.role = item.role;
    c.dur = item.dur;
    c.rules = std::move(rules);
    c.variant = variant;
    c.surah = item.surah;
    c.ayah = item.ayah;
    c.host_marks = item.host_marks;
    c.letter = item.letter;
    return c;
}

// Gap buffer of items. The rule passes walk the list from the end to the start and insert next to
// the current position, so keeping the free space at the cursor makes every insert O(distance moved)
// instead of O(n). Indexing is O(1); references are invalidated by insert / erase.
class ItemList {
public:
    size_t size() const { return buf_.size() - (gap_end_ - gap_start_); }
    bool empty() const { return size() == 0; }
    PhonemeItem& operator[](size_t i) { return i < gap_start_ ? buf_[i] : buf_[i + (gap_end_ - gap_start_)]; }
    const PhonemeItem& operator[](size_t i) const {
        return i < gap_start_ ? buf_[i] : buf_[i + (gap_end_ - gap_start_)];
    }
    PhonemeItem& back() { return (*this)[size() - 1]; }
    const PhonemeItem& back() const { return (*this)[size() - 1]; }
    void push_back(PhonemeItem item) { insert_at(size(), std::move(item)); }
    void insert_at(size_t pos, PhonemeItem item) {
        MoveGap(pos);
        if (gap_start_ == gap_end_)
            Grow();
        buf_[gap_start_++] = std::move(item);
    }
    void erase_at(size_t pos) {
        MoveGap(pos);
        ++gap_end_;
    }
    std::vector<PhonemeItem> ToVector() {
        MoveGap(size());
        buf_.resize(gap_start_);
        gap_start_ = gap_end_ = 0;
        return std::move(buf_);
    }

private:
    void MoveGap(size_t pos) {
        if (gap_start_ == gap_end_) {  // empty gap: nothing to move (and no self-move-assignment)
            gap_start_ = gap_end_ = pos;
            return;
        }
        while (gap_start_ > pos)
            buf_[--gap_end_] = std::move(buf_[--gap_start_]);
        while (gap_start_ < pos)
            buf_[gap_start_++] = std::move(buf_[gap_end_++]);
    }
    void Grow() {
        const size_t cap = std::max<size_t>(64, buf_.size() * 2);
        std::vector<PhonemeItem> next(cap);
        for (size_t i = 0; i < gap_start_; ++i)
            next[i] = std::move(buf_[i]);
        const size_t after = buf_.size() - gap_end_;
        for (size_t i = 0; i < after; ++i)
            next[cap - after + i] = std::move(buf_[gap_end_ + i]);
        buf_ = std::move(next);
        gap_end_ = cap - after;
    }

    std::vector<PhonemeItem> buf_;
    size_t gap_start_ = 0;
    size_t gap_end_ = 0;
};

// ---------------------------------------------------------------------------------------------
class Builder {
public:
    explicit Builder(const std::vector<const Ayah*>& ayahs) : ayahs_(ayahs) {}

    void Run(PhonemizeResult* out);

private:
    void Diag(const std::string& kind, int offset) { diags_.push_back({word_->Id(), kind, offset}); }
    PhonemeItem& Add(const std::string& ph, int s, int e, Role role, std::vector<int> dur = {},
                     uint32_t letter = 0) {
        PhonemeItem it;
        it.ph = ph;
        it.word = word_->index;
        it.span_start = s;
        it.span_end = e;
        it.role = role;
        it.dur = std::move(dur);
        it.letter = letter;
        it.surah = word_->surah;
        it.ayah = word_->ayah;
        items_.push_back(std::move(it));
        return items_.back();
    }
    // Index of the previous item if it belongs to the word being processed, else -1.
    int Last() const {
        if (items_.empty())
            return -1;
        const PhonemeItem& l = items_.back();
        if (l.surah == word_->surah && l.ayah == word_->ayah && l.word == word_->index) {
            return static_cast<int>(items_.size()) - 1;
        }
        return -1;
    }
    bool Lengthen(const char* vowel, int end, uint32_t by) {
        const int l = Last();
        if (l >= 0) {
            PhonemeItem& it = items_[l];
            if (it.role == Role::kVowel && it.ph == vowel && IsDur(it.dur, {1}) &&
                it.variant == Variant::kBoth) {
                it.dur = {2};
                it.span_end = end;
                it.long_by = by;
                return true;
            }
        }
        return false;
    }
    void SignOnLastVowel(const LetterUnit& unit, uint32_t cp) {
        const int l = Last();
        if (l < 0) {
            Diag("mark_" + Hex4(cp) + "_without_host", unit.start);
        } else if (cp == kMaddahAbove) {
            items_[l].maddah = true;
        } else {
            items_[l].signs.insert(cp);
        }
    }
    void Marks(const LetterUnit& unit, int host, bool host_is_tatweel);
    void Consonant(const LetterUnit& unit, const std::string& ph, std::vector<std::string> tags);
    void Alef(const LetterUnit& unit);
    void Wasla(const LetterUnit& unit);
    void LamUnit(const LetterUnit& unit);
    void Small(const LetterUnit& unit, const char* vowel);
    void Weak(const LetterUnit& unit);
    bool NextUnitHas(const LetterUnit& unit, uint32_t mark) const;
    void Unit(const LetterUnit& unit);
    void RunAyah(const Ayah& ayah);

    const std::vector<const Ayah*>& ayahs_;
    ItemList items_;
    std::vector<Diagnostic> diags_;
    const Word* word_ = nullptr;
    std::vector<LetterUnit> units_;
    size_t pos_ = 0;
    std::vector<std::string> pending_tags_;
};

void Builder::Marks(const LetterUnit& unit, int host, bool host_is_tatweel) {
    std::vector<std::pair<size_t, uint32_t>> order;
    for (size_t k = 0; k < unit.marks.size(); ++k)
        order.emplace_back(k, unit.marks[k]);
    if (host_is_tatweel) {  // hamza first, then its vowel
        std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) {
            return (a.second != kHamzaAbove) < (b.second != kHamzaAbove);
        });
    }
    for (const auto& km : order) {
        const size_t k = km.first;
        const uint32_t cp = km.second;
        const int s0 = unit.start + 1 + static_cast<int>(k);
        const int s1 = s0 + 1;
        if (ShortVowel(cp)) {
            Add(ShortVowel(cp), s0, s1, Role::kVowel, {1});
        } else if (TanweenVowel(cp)) {
            Add(TanweenVowel(cp), s0, s1, Role::kVowel, {1});
            Add("n", s0, s1, Role::kTanweenN).variant = Variant::kContinueOnly;  // no n at waqf
        } else if (cp == kSuperscriptAlef) {
            if (!Lengthen("a", s1, cp)) {
                Add("a", s0, s1, Role::kVowel, {2}).long_by = cp;
                Diag("long_a_without_fatha", s0);
            }
        } else if (cp == kHamzaAbove) {
            Add("hz", s0, s1, Role::kCons, {}, kHamzaAbove);
            for (size_t m = k + 1; m < unit.marks.size(); ++m)
                items_.back().host_marks.insert(unit.marks[m]);
        } else if (cp == kMaddahAbove) {
            SignOnLastVowel(unit, cp);
        } else if (IsSignMark(cp)) {
            const int target = host >= 0 ? host : Last();
            if (target >= 0) {
                items_[target].signs.insert(cp);
            } else {
                Diag("mark_" + Hex4(cp) + "_without_host", s0);
            }
        } else if (cp == kShadda || cp == kSukun || cp == kRoundedZero || cp == kRectangularZero) {
            // shaddah handled at emission; sukun = nothing; zeros handled per letter
        } else if (cp == kSmallHighYeh) {
            bool own = false;
            for (uint32_t m : unit.marks)
                own = own || ShortVowel(m) || TanweenVowel(m);
            if (own) {
                Add("y", s0, s1, Role::kCons, {}, kSmallHighYeh);  // yeh with its own vowel
            } else if (!Lengthen("i", s1, cp)) {
                Diag("small_high_yeh_without_kasra", s0);
            }
        } else {
            Diag("unhandled_mark_" + Hex4(cp), s0);
        }
    }
}

void Builder::Consonant(const LetterUnit& unit, const std::string& ph, std::vector<std::string> tags) {
    const int times = unit.Has(kShadda) ? 2 : 1;
    tags.insert(tags.end(), pending_tags_.begin(), pending_tags_.end());
    pending_tags_.clear();
    int host = -1;
    const std::set<uint32_t> host_marks = MarksOfHost(unit);
    for (int n = 0; n < times; ++n) {
        PhonemeItem& h = Add(ph, unit.start, unit.start + 1, Role::kCons, {}, unit.ch);
        host = static_cast<int>(items_.size()) - 1;
        h.rules.insert(h.rules.end(), tags.begin(), tags.end());
        h.host_marks = host_marks;
        if (ph == "hz" && pos_ > 0 && units_[pos_ - 1].ch == kAlefWasla) {
            h.rules.push_back("hamza_after_wasla");  // no madd before it (the text has no maddah)
        }
        if (times == 2 && n == 1 && (ph == "n" || ph == "m")) {  // mushaddad noon / meem (spec 3.3)
            h.rules.push_back("ghunnah");
            h.dur = {2};
        }
    }
    Marks(unit, host, false);
}

void Builder::Alef(const LetterUnit& unit) {
    if (unit.Has(kRoundedZero))
        return;                        // written, not pronounced
    if (unit.Has(kRectangularZero)) {  // DRAFT: silent when continuing, pronounced at waqf
        const int l = Last();
        if (l >= 0 && items_[l].role == Role::kVowel && items_[l].ph == "a") {
            items_[l].flags.insert("alef_waqf");
        } else {
            Diag("rectangular_zero_alef_without_fatha", unit.start);
        }
        return;
    }
    const int l = Last();
    if (l >= 0 && items_[l].role == Role::kVowel && items_[l].ph == "a" && IsDur(items_[l].dur, {1})) {
        Lengthen("a", unit.end, kAlef);
    } else if (l >= 0 && items_[l].role == Role::kTanweenN && items_.size() >= 2 &&
               items_[items_.size() - 2].ph == "a") {
        items_[items_.size() - 2].flags.insert("iwad");  // tanween alef
    } else {
        Diag("alef_without_fatha", unit.start);
    }
    for (uint32_t m : unit.marks) {
        if (m == kMaddahAbove || IsSignMark(m))
            SignOnLastVowel(unit, m);
    }
}

void Builder::Wasla(const LetterUnit& unit) {
    const LetterUnit* nxt = pos_ + 1 < units_.size() ? &units_[pos_ + 1] : nullptr;
    if (nxt != nullptr && nxt->ch == kLam) {
        const size_t i1 = items_.size();
        Add("hz", unit.start, unit.end, Role::kCons, {}, kAlefWasla);
        const size_t i2 = items_.size();
        Add("a", unit.start, unit.end, Role::kVowel, {1});
        for (size_t idx : {i1, i2}) {
            items_[idx].variant = Variant::kStartOnly;
            items_[idx].rules.push_back("hamzat_wasl");
        }
    } else {
        Diag("wasla_start_vowel_unknown", unit.start);
    }
}

void Builder::LamUnit(const LetterUnit& unit) {
    const LetterUnit* prev = pos_ > 0 ? &units_[pos_ - 1] : nullptr;
    const LetterUnit* nxt = pos_ + 1 < units_.size() ? &units_[pos_ + 1] : nullptr;
    const bool article = prev != nullptr && prev->ch == kAlefWasla;
    if (unit.marks.empty() && nxt != nullptr && nxt->Has(kShadda)) {
        pending_tags_ = {article ? "lam_shamsiyyah" : "lam_assimilated"};
        return;  // not pronounced; the next consonant is doubled by its shaddah
    }
    std::vector<std::string> tags;
    if (article && unit.Has(kSukun))
        tags.push_back("lam_qamariyyah");
    Consonant(unit, "l", tags);
}

void Builder::Small(const LetterUnit& unit, const char* vowel) {
    bool own = false;
    for (uint32_t m : unit.marks)
        own = own || ShortVowel(m) || TanweenVowel(m);
    if (own) {
        Consonant(unit, std::string(vowel) == "u" ? "w" : "y", {});  // small letter with own vowel
        return;
    }
    if (!Lengthen(vowel, unit.end, unit.ch))
        Diag(std::string("small_letter_without_") + vowel, unit.start);
    for (uint32_t m : unit.marks) {
        if (m == kMaddahAbove) {
            SignOnLastVowel(unit, kMaddahAbove);
        } else {
            Diag("unhandled_mark_" + Hex4(m) + "_on_small_letter", unit.start);
        }
    }
}

bool Builder::NextUnitHas(const LetterUnit& unit, uint32_t mark) const {
    for (const LetterUnit& x : units_) {
        if (x.start == unit.end)
            return x.Has(mark);
    }
    return false;
}

void Builder::Weak(const LetterUnit& unit) {
    const uint32_t cp = unit.ch;
    if (unit.Has(kRoundedZero))
        return;
    bool own = false;
    for (uint32_t m : unit.marks)
        own = own || ShortVowel(m) || TanweenVowel(m) || m == kShadda || m == kSukun;
    if (own) {
        Consonant(unit, (cp == kYeh || cp == kAlefMaksura) ? "y" : "w", {});  // yeh-shaped: y
        return;
    }
    if (unit.Has(kSuperscriptAlef)) {  // carrier for the superscript alef
        Marks(unit, -1, false);
        return;
    }
    const int l = Last();
    const bool has_pv = l >= 0 && items_[l].role == Role::kVowel;
    const std::string prev_vowel = has_pv ? items_[l].ph : "";
    if (cp == kAlefMaksura && l >= 0 && items_[l].role == Role::kTanweenN && items_.size() >= 2 &&
        items_[items_.size() - 2].ph == "a") {
        items_[items_.size() - 2].flags.insert("iwad");  // tanween alef
    } else if (cp == kWaw && has_pv && prev_vowel == "a" && NextUnitHas(unit, kRoundedZero)) {
        // bare waw after fatha before a silent alef: consonant w, sukun implied by the encoding
        Add("w", unit.start, unit.start + 1, Role::kCons, {}, cp).rules.push_back("implicit_sukun");
    } else if (cp == kWaw && has_pv && prev_vowel == "u") {
        Lengthen("u", unit.end, cp);
    } else if (cp == kYeh && has_pv && prev_vowel == "i") {
        Lengthen("i", unit.end, cp);
    } else if (cp == kAlefMaksura && has_pv && (prev_vowel == "a" || prev_vowel == "i")) {
        Lengthen(prev_vowel.c_str(), unit.end, cp);
    } else {
        const char* name = cp == kWaw ? "waw" : cp == kYeh ? "yeh" : "maksura";
        Diag(std::string("bare_") + name + "_after_" + (has_pv ? prev_vowel : "nothing"), unit.start);
    }
    Marks(unit, -1, false);
}

void Builder::Unit(const LetterUnit& unit) {
    const uint32_t cp = unit.ch;
    if (cp == kAlef) {
        Alef(unit);
    } else if (cp == kAlefWasla) {
        Wasla(unit);
    } else if (cp == kTatweel) {
        Marks(unit, -1, true);
    } else if (cp == kSmallWaw) {
        Small(unit, "u");
    } else if (cp == kSmallYeh) {
        Small(unit, "i");
    } else if (cp == kWaw || cp == kYeh || cp == kAlefMaksura) {
        Weak(unit);
    } else if (cp == kLam) {
        LamUnit(unit);
    } else if (cp == kTehMarbuta) {
        Consonant(unit, "t", {"teh_marbuta"});
    } else if (IsHamzaSeat(cp)) {
        Consonant(unit, "hz", {});
    } else if (SymbolForLetter(cp) != nullptr) {
        Consonant(unit, SymbolForLetter(cp), {});
    } else {
        Diag("unhandled_letter_" + Hex4(cp), unit.start);
    }
}

void Builder::RunAyah(const Ayah& ayah) {
    for (const Word& word : ayah.words) {
        word_ = &word;
        std::vector<uint32_t> cps;
        if (!DecodeUtf8(word.text, &cps))
            throw UthmaniParseError("invalid UTF-8 in " + word.Id());
        std::vector<LetterUnit> units = ParseWord(cps);
        if (IsDisconnectedLetters(units)) {
            Diag("disconnected_letters", 0);  // letter names: task 1.9 data, not orthography
            continue;
        }
        units_ = std::move(units);
        for (pos_ = 0; pos_ < units_.size(); ++pos_)
            Unit(units_[pos_]);
    }
}

// -- passes ----------------------------------------------------------------------------------------

const std::set<std::string> kIzharHalqi = {"hz", "h", "E", "H", "gh", "kh"};
const std::set<std::string> kIdghamGhunnah = {"y", "n", "m", "w"};
const std::set<std::string> kIdghamNoGhunnah = {"l", "r"};
const std::set<std::string> kIkhfa = {"t", "th", "j", "d", "dh", "z", "s", "sh",
                                      "S", "D",  "T", "Z", "f",  "q", "k"};
const std::set<std::string> kQalqalah = {"q", "T", "b", "j", "d"};

bool HasAnyOf(const std::set<uint32_t>& marks, std::initializer_list<uint32_t> any) {
    for (uint32_t m : any) {
        if (marks.count(m))
            return true;
    }
    return false;
}

bool IsNoonSakin(const PhonemeItem& it) {
    if (it.role == Role::kTanweenN)
        return true;
    return it.role == Role::kCons && it.ph == "n" && it.letter == kNoon &&
           !HasAnyOf(it.host_marks, {kFatha, kDamma, kKasra, kShadda});
}

// Index of the first item after i that is pronounced when continuing, if it is a consonant in the
// same surah (a surah end is always a stop). -1 otherwise.
long NextConsonant(const ItemList& items, size_t i) {
    size_t j = i + 1;
    while (j < items.size() && items[j].variant == Variant::kStartOnly)
        ++j;
    if (j < items.size() && items[j].role == Role::kCons && items[j].surah == items[i].surah) {
        return static_cast<long>(j);
    }
    return -1;
}

bool WordEndsAfter(const ItemList& items, size_t i) {
    size_t k = i + 1;
    while (k < items.size() && items[k].variant == Variant::kStartOnly)
        ++k;
    return k >= items.size() || KeyOf(items[k]) != KeyOf(items[i]);
}

std::string WordIdOf(const PhonemeItem& it) {
    return std::to_string(it.surah) + ":" + std::to_string(it.ayah) + ":" + std::to_string(it.word);
}

void Hide(ItemList& items, size_t i, const char* rule, const char* ph) {
    const bool boundary = items[i].role == Role::kTanweenN || WordEndsAfter(items, i);
    if (boundary && items[i].role != Role::kTanweenN) {
        PhonemeItem plain = Copy(items[i], Variant::kWaqfOnly, items[i].rules);
        items[i].variant = Variant::kContinueOnly;
        items.insert_at(i + 1, plain);
    }
    if (ph != nullptr)
        items[i].ph = ph;
    items[i].rules.push_back(rule);
    items[i].dur = {2};
}

void Idgham(ItemList& items, size_t i, size_t j, const char* tag, bool ghunnah) {
    if (items[i].role == Role::kTanweenN) {
        items.erase_at(i);  // tanween never has an n at a stop either
        --j;
    } else {
        items[i].variant = Variant::kWaqfOnly;  // only a stop keeps the plain noon
    }
    std::vector<size_t> group;
    for (size_t k = j; k < std::min(j + 2, items.size()); ++k) {
        if (items[k].role == Role::kCons && items[k].ph == items[j].ph &&
            KeyOf(items[k]) == KeyOf(items[j])) {
            group.push_back(k);
        }
    }
    const size_t last = group.back();
    std::vector<std::string> rules = items[last].rules;
    rules.push_back(tag);
    PhonemeItem fused =
        Clone(items[last], Variant::kAfterContinue, rules, ghunnah ? std::vector<int>{2} : items[last].dur);
    if (group.size() == 2)
        items[last].variant = Variant::kStartOnly;  // keep a plain restart form
    items.insert_at(last + 1, fused);
}

void ApplyNoonRules(ItemList& items, std::vector<Diagnostic>& diags) {
    std::vector<size_t> idx;
    for (size_t k = 0; k < items.size(); ++k) {
        if (IsNoonSakin(items[k]))
            idx.push_back(k);
    }
    for (auto it = idx.rbegin(); it != idx.rend(); ++it) {
        const size_t i = *it;
        const long jl = NextConsonant(items, i);
        if (jl < 0)
            continue;  // end of the range, or not followed by a consonant
        const size_t j = static_cast<size_t>(jl);
        const bool same_word = KeyOf(items[i]) == KeyOf(items[j]);
        const std::string letter = items[j].ph;
        const bool izhar_like = kIzharHalqi.count(letter) || (same_word && (letter == "y" || letter == "w"));
        if (items[i].role == Role::kCons && items[i].host_marks.count(kSukun) && !izhar_like) {
            diags.push_back({WordIdOf(items[i]), "noon_sukun_before_non_izhar_letter", items[i].span_start});
            continue;
        }
        if (kIzharHalqi.count(letter)) {
            items[i].rules.push_back("izhar_halqi");
        } else if (kIdghamGhunnah.count(letter) && same_word && (letter == "y" || letter == "w")) {
            items[i].rules.push_back("izhar_mutlaq");
        } else if (kIdghamGhunnah.count(letter) || kIdghamNoGhunnah.count(letter)) {
            if (same_word) {
                diags.push_back({WordIdOf(items[i]), "noon_idgham_inside_word", items[i].span_start});
                continue;
            }
            const bool g = kIdghamGhunnah.count(letter) != 0;
            Idgham(items, i, j, g ? "idgham_ghunnah" : "idgham_no_ghunnah", g);
        } else if (letter == "b") {
            Hide(items, i, "iqlab", "m");
        } else if (kIkhfa.count(letter)) {
            Hide(items, i, "ikhfa", nullptr);
        } else {
            diags.push_back({WordIdOf(items[i]), "noon_before_unmapped_" + letter, items[i].span_start});
        }
    }
}

void ApplyMeemRules(ItemList& items, std::vector<Diagnostic>& diags) {
    std::vector<size_t> idx;
    for (size_t k = 0; k < items.size(); ++k) {
        const PhonemeItem& it = items[k];
        if (it.role == Role::kCons && it.ph == "m" && it.letter == kMeem &&
            !HasAnyOf(it.host_marks, {kFatha, kDamma, kKasra, kShadda})) {
            idx.push_back(k);
        }
    }
    for (auto it = idx.rbegin(); it != idx.rend(); ++it) {
        const size_t i = *it;
        const long jl = NextConsonant(items, i);
        if (jl < 0)
            continue;
        const size_t j = static_cast<size_t>(jl);
        if (items[j].ph == "b") {
            Hide(items, i, "ikhfa_shafawi", nullptr);
        } else if (items[j].ph == "m") {
            if (KeyOf(items[i]) == KeyOf(items[j])) {
                diags.push_back({WordIdOf(items[i]), "meem_idgham_inside_word", items[i].span_start});
                continue;
            }
            Idgham(items, i, j, "idgham_mithlayn_saghir", true);
        } else {
            items[i].rules.push_back("izhar_shafawi");
        }
    }
}

void ApplyQalqalah(ItemList& items) {
    std::vector<size_t> idx;
    for (size_t k = 0; k < items.size(); ++k) {
        const PhonemeItem& it = items[k];
        if (it.role == Role::kCons && kQalqalah.count(it.ph) && it.host_marks.count(kSukun) &&
            !it.host_marks.count(kShadda)) {
            idx.push_back(k);
        }
    }
    for (auto it = idx.rbegin(); it != idx.rend(); ++it) {
        const size_t i = *it;
        if (WordEndsAfter(items, i)) {
            std::vector<std::string> rules = items[i].rules;
            rules.push_back("qalqalah_kubra");
            PhonemeItem kubra = Copy(items[i], Variant::kWaqfOnly, rules);
            items[i].variant = Variant::kContinueOnly;
            items[i].rules.push_back("qalqalah_sughra");
            items.insert_at(i + 1, kubra);
        } else {
            items[i].rules.push_back("qalqalah_sughra");
        }
    }
}

// -- madd ----------------------------------------------------------------------------------------

long NextPronounced(const ItemList& items, size_t i) {
    size_t j = i + 1;
    while (j < items.size() && items[j].variant == Variant::kStartOnly)
        ++j;
    if (j < items.size() && items[j].surah == items[i].surah)
        return static_cast<long>(j);
    return -1;
}

bool IsMaddHamza(const PhonemeItem* item) {
    return item != nullptr && item->role == Role::kCons && item->ph == "hz" &&
           !HasRule(*item, "hamza_after_wasla");
}

bool IsSakinConsonant(const ItemList& items, size_t k) {
    const PhonemeItem& c = items[k];
    if (c.role != Role::kCons)
        return false;
    if (c.host_marks.count(kSukun))
        return true;
    return c.host_marks.count(kShadda) && k + 1 < items.size() && items[k + 1].ph == c.ph &&
           KeyOf(items[k + 1]) == KeyOf(c) && items[k + 1].span_start == c.span_start &&
           items[k + 1].span_end == c.span_end;
}

bool LastConsonantOfWord(const ItemList& items, size_t k) {
    const WordKey key = KeyOf(items[k]);
    for (size_t m = k + 1; m < items.size(); ++m) {
        if (KeyOf(items[m]) != key)
            return true;
        if (items[m].role == Role::kCons)
            return false;
    }
    return true;
}

bool HasFathatan(const ItemList& items, size_t k) {
    const WordKey key = KeyOf(items[k]);
    for (size_t m = k + 1; m < items.size(); ++m) {
        if (KeyOf(items[m]) != key)
            return false;
        if (items[m].role == Role::kTanweenN)
            return items[m - 1].ph == "a";
        if (items[m].role == Role::kCons)
            return false;
    }
    return false;
}

void ApplyMadd(ItemList& items) {
    std::vector<size_t> idx;
    for (size_t k = 0; k < items.size(); ++k) {
        const PhonemeItem& it = items[k];
        if (it.role == Role::kVowel && IsDur(it.dur, {2}) && it.long_by != 0 &&
            it.variant == Variant::kBoth) {
            idx.push_back(k);
        }
    }
    for (auto rit = idx.rbegin(); rit != idx.rend(); ++rit) {
        const size_t i = *rit;
        const PhonemeItem* prev = i > 0 ? &items[i - 1] : nullptr;
        const bool silah = (items[i].long_by == kSmallWaw || items[i].long_by == kSmallYeh) &&
                           prev != nullptr && prev->role == Role::kCons && prev->letter == kHeh;
        const bool badal = prev != nullptr && prev->role == Role::kCons && prev->ph == "hz" &&
                           prev->variant == Variant::kBoth && KeyOf(*prev) == KeyOf(items[i]);
        const long kl = NextPronounced(items, i);
        const PhonemeItem* nxt = kl >= 0 ? &items[static_cast<size_t>(kl)] : nullptr;
        const size_t k = kl >= 0 ? static_cast<size_t>(kl) : 0;
        const bool word_final = nxt == nullptr || KeyOf(*nxt) != KeyOf(items[i]);
        const bool hamza_next = IsMaddHamza(nxt);
        const bool nxt_cons = nxt != nullptr && nxt->role == Role::kCons;
        const char* base_rule = badal ? "madd_badal" : "madd_tabii";

        if (silah) {
            items[i].variant = Variant::kContinueOnly;  // the pronoun's silah only exists when continuing
            items[i].rules.push_back(hamza_next ? "madd_silah_kubra" : "madd_silah_sughra");
            items[i].dur = hamza_next ? std::vector<int>{4, 5} : std::vector<int>{2};
        } else if (hamza_next) {
            if (!word_final) {
                items[i].rules.push_back("madd_muttasil");
                items[i].dur = {4, 5};
            } else {  // the long vowel ends its word: munfasil only if the reading goes on
                PhonemeItem stop = Clone(items[i], Variant::kWaqfOnly, {base_rule}, {2});
                items[i].variant = Variant::kContinueOnly;
                items[i].rules.push_back("madd_munfasil");
                items[i].dur = {4, 5};
                items.insert_at(i + 1, stop);
            }
        } else if (nxt != nullptr && !word_final && IsSakinConsonant(items, k)) {
            items[i].rules.push_back("madd_lazim");
            items[i].dur = {6};
        } else if (nxt != nullptr && !word_final && nxt_cons && LastConsonantOfWord(items, k) &&
                   !HasFathatan(items, k)) {
            // sukun only appears at a stop ('arid lis-sukun)
            PhonemeItem stop = Clone(items[i], Variant::kWaqfOnly, {"madd_arid_lissukun"}, {2, 4, 6});
            items[i].variant = Variant::kContinueOnly;
            items[i].rules.push_back(base_rule);
            items.insert_at(i + 1, stop);
        } else {
            items[i].rules.push_back(base_rule);
        }
    }
}

void ApplyLeen(ItemList& items) {
    std::vector<size_t> idx;
    for (size_t k = 0; k < items.size(); ++k) {
        const PhonemeItem& it = items[k];
        if (it.role == Role::kCons && (it.ph == "w" || it.ph == "y") && it.host_marks.count(kSukun) &&
            it.variant == Variant::kBoth) {
            idx.push_back(k);
        }
    }
    for (auto rit = idx.rbegin(); rit != idx.rend(); ++rit) {
        const size_t i = *rit;
        const PhonemeItem* prev = i > 0 ? &items[i - 1] : nullptr;
        if (!(prev != nullptr && prev->role == Role::kVowel && prev->ph == "a" && IsDur(prev->dur, {1}) &&
              KeyOf(*prev) == KeyOf(items[i]))) {
            continue;
        }
        const long kl = NextPronounced(items, i);
        if (kl < 0)
            continue;
        const size_t k = static_cast<size_t>(kl);
        if (items[k].role != Role::kCons || KeyOf(items[k]) != KeyOf(items[i]))
            continue;
        if (!LastConsonantOfWord(items, k))
            continue;
        std::vector<std::string> rules = items[i].rules;
        rules.push_back("madd_leen");
        PhonemeItem stop = Clone(items[i], Variant::kWaqfOnly, rules, {2, 4, 6});
        items[i].variant = Variant::kContinueOnly;
        items.insert_at(i + 1, stop);
    }
}

// -- waqf ------------------------------------------------------------------------------------------

void ApplyWaqf(ItemList& items) {
    std::vector<std::vector<size_t>> groups;
    for (size_t k = 0; k < items.size(); ++k) {
        if (!groups.empty() && KeyOf(items[groups.back().front()]) == KeyOf(items[k])) {
            groups.back().push_back(k);
        } else {
            groups.push_back({k});
        }
    }
    for (auto git = groups.rbegin(); git != groups.rend(); ++git) {
        const std::vector<size_t>& idxs = *git;
        std::vector<size_t> cont;
        for (size_t k : idxs) {
            if (items[k].variant == Variant::kBoth || items[k].variant == Variant::kContinueOnly)
                cont.push_back(k);
        }
        size_t end = cont.size();
        while (end > 0 && items[cont[end - 1]].role == Role::kTanweenN)
            --end;
        if (end == 0)
            continue;
        const size_t v_idx = cont[end - 1];
        if (!(items[v_idx].role == Role::kVowel && IsDur(items[v_idx].dur, {1}) &&
              items[v_idx].variant == Variant::kBoth)) {
            continue;
        }
        bool stop_only_consonant_follows = false;  // e.g. the noon of an idgham: the vowel is not final
        for (size_t k : idxs) {
            if (k > v_idx && items[k].role == Role::kCons)
                stop_only_consonant_follows = true;
        }
        if (stop_only_consonant_follows)
            continue;
        long c_idx = -1;
        for (size_t m = 0; m + 1 < end; ++m) {
            if (items[cont[m]].role == Role::kCons)
                c_idx = static_cast<long>(cont[m]);
        }

        bool has_extra = false;
        PhonemeItem extra;
        if (items[v_idx].flags.count("iwad")) {
            extra = Clone(items[v_idx], Variant::kWaqfOnly, {"madd_iwad"}, {2});
            has_extra = true;
        } else if (items[v_idx].flags.count("alef_waqf")) {
            extra = Clone(items[v_idx], Variant::kWaqfOnly, {"alef_waqf"}, {2});
            has_extra = true;
        }
        items[v_idx].variant = Variant::kContinueOnly;  // a stop drops the final vowel
        if (has_extra)
            items.insert_at(v_idx + 1, extra);
        if (c_idx < 0)
            continue;
        const size_t c = static_cast<size_t>(c_idx);
        if (items[c].variant != Variant::kBoth)
            continue;
        PhonemeItem stop;
        if (HasRule(items[c], "teh_marbuta")) {
            stop = Clone(items[c], Variant::kWaqfOnly, {"teh_marbuta_waqf"}, {});
            stop.ph = "h";
        } else if (kQalqalah.count(items[c].ph)) {
            std::vector<std::string> rules = items[c].rules;
            rules.push_back("qalqalah_kubra");
            stop = Clone(items[c], Variant::kWaqfOnly, rules, items[c].dur);
        } else {
            continue;
        }
        items[c].variant = Variant::kContinueOnly;
        items.insert_at(c + 1, stop);
    }
}

void Builder::Run(PhonemizeResult* out) {
    for (const Ayah* ayah : ayahs_)
        RunAyah(*ayah);
    ApplyNoonRules(items_, diags_);
    ApplyMeemRules(items_, diags_);
    ApplyQalqalah(items_);
    ApplyMadd(items_);
    ApplyLeen(items_);
    ApplyWaqf(items_);
    out->items = items_.ToVector();
    out->diagnostics = std::move(diags_);
}

std::string Join(const std::vector<std::string>& parts, char sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i)
            out.push_back(sep);
        out += parts[i];
    }
    return out;
}

}  // namespace

const char* RoleName(Role r) {
    switch (r) {
        case Role::kCons:
            return "cons";
        case Role::kVowel:
            return "vowel";
        case Role::kTanweenN:
            return "tanween_n";
    }
    return "";
}

WordKey KeyOf(const PhonemeItem& item) {
    return WordKey(item.surah, item.ayah, item.word);
}

bool PhonemizeRange(const std::vector<const Ayah*>& ayahs, PhonemizeResult* out, std::string* error) {
    try {
        Builder(ayahs).Run(out);
        return true;
    } catch (const UthmaniParseError& e) {
        if (error)
            *error = e.what();
        return false;
    }
}

std::vector<size_t> SelectReading(const std::vector<PhonemeItem>& items, const std::set<WordKey>& stops,
                                  bool all_stops) {
    std::vector<size_t> out;
    bool have_prev = false;
    WordKey prev_key;
    bool prev_stopped = true;
    bool stopped_here = false;
    for (size_t i = 0; i < items.size(); ++i) {
        const WordKey key = KeyOf(items[i]);
        if (!have_prev || key != prev_key) {
            if (have_prev) {
                prev_stopped =
                    all_stops || stops.count(prev_key) || std::get<0>(prev_key) != std::get<0>(key);
            }
            stopped_here = all_stops || stops.count(key);
            prev_key = key;
            have_prev = true;
        }
        const Variant v = items[i].variant;
        const bool keep = v == Variant::kBoth || (v == Variant::kContinueOnly && !stopped_here) ||
                          (v == Variant::kWaqfOnly && stopped_here) ||
                          (v == Variant::kStartOnly && prev_stopped) ||
                          (v == Variant::kAfterContinue && !prev_stopped);
        if (keep)
            out.push_back(i);
    }
    return out;
}

std::string DumpResult(const PhonemizeResult& result) {
    std::string out;
    for (const PhonemeItem& it : result.items) {
        out += "I\t" + WordIdOf(it) + "\t" + it.ph + "\t" + std::to_string(it.span_start) + "," +
               std::to_string(it.span_end) + "\t" + RoleName(it.role) + "\t";
        for (size_t i = 0; i < it.dur.size(); ++i) {
            if (i)
                out.push_back('/');
            out += std::to_string(it.dur[i]);
        }
        out += std::string("\t") + VariantName(it.variant) + "\t" + Join(it.rules, ',') + "\n";
    }
    for (const Diagnostic& d : result.diagnostics) {
        out += "D\t" + d.word_id + "\t" + d.kind + "\t" + std::to_string(d.offset) + "\n";
    }
    return out;
}

std::string DumpParity(const Quran& quran) {
    std::vector<const Ayah*> all;
    std::set<WordKey> ayah_ends;
    for (const Surah& s : quran.surahs()) {
        for (const Ayah& a : s.ayahs) {
            all.push_back(&a);
            ayah_ends.insert(WordKey(a.surah, a.ayah, static_cast<int>(a.words.size())));
        }
    }
    PhonemizeResult range;
    std::string error;
    if (!PhonemizeRange(all, &range, &error))
        return "ERROR\t" + error + "\n";
    std::string out = DumpResult(range);
    const auto readings = {std::make_pair("continue", SelectReading(range.items, {}, false)),
                           std::make_pair("stops", SelectReading(range.items, {}, true)),
                           std::make_pair("ayah_end", SelectReading(range.items, ayah_ends, false))};
    for (const auto& r : readings) {
        out += std::string("R\t") + r.first + "\t";
        for (size_t i = 0; i < r.second.size(); ++i) {
            if (i)
                out.push_back(',');
            out += std::to_string(r.second[i]);
        }
        out += "\n";
    }
    for (const Ayah* a : all) {
        PhonemizeResult one;
        if (!PhonemizeRange({a}, &one, &error))
            return "ERROR\t" + error + "\n";
        out += "H\t" + std::to_string(a->surah) + ":" + std::to_string(a->ayah) + "\t" +
               Sha256Hex(DumpResult(one)) + "\n";
    }
    return out;
}

}  // namespace mutqin
