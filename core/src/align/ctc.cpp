#include "align/ctc.h"

#include <algorithm>
#include <map>

#include "tajweed/inventory.h"

namespace mutqin {

Frame ArgmaxFrame(const float* probs) {
    Frame f;
    f.prob = probs[0];
    for (size_t i = 1; i < kVocabSize; ++i) {
        if (probs[i] > f.prob) {
            f.prob = probs[i];
            f.label = static_cast<int>(i);
        }
    }
    return f;
}

std::vector<HeardToken> GreedyDecode(const std::vector<Frame>& frames) {
    std::vector<HeardToken> tokens;
    size_t i = 0;
    while (i < frames.size()) {
        const int label = frames[i].label;
        size_t j = i;
        double sum = 0.0;
        while (j < frames.size() && frames[j].label == label)
            sum += frames[j++].prob;
        if (label != 0) {
            HeardToken t;
            t.symbol = Phonemes()[label - 1].symbol;
            t.confidence = sum / static_cast<double>(j - i);
            t.start = static_cast<int>(i);
            t.end = static_cast<int>(j);
            tokens.push_back(t);
        }
        i = j;
    }
    return tokens;
}

namespace {

bool Keep(Variant v, bool before, bool after) {
    switch (v) {
        case Variant::kBoth:
            return true;
        case Variant::kContinueOnly:
            return !after;
        case Variant::kWaqfOnly:
            return after;
        case Variant::kStartOnly:
            return before;
        case Variant::kAfterContinue:
            return !before;
    }
    return false;
}

// Consecutive identical phonemes (a shaddah) are one token: CTC cannot emit a label twice in a row.
std::vector<std::string> Collapse(const std::vector<std::string>& in) {
    std::vector<std::string> out;
    for (const std::string& s : in)
        if (out.empty() || out.back() != s)
            out.push_back(s);
    return out;
}

}  // namespace

std::vector<WordRef> BuildWordReadings(const std::vector<PhonemeItem>& items,
                                       const std::map<std::pair<int, int>, int>& ayah_words) {
    std::vector<WordKey> order;
    std::map<WordKey, std::vector<const PhonemeItem*>> by_word;
    for (const PhonemeItem& item : items) {
        const WordKey key = KeyOf(item);
        auto it = by_word.find(key);
        if (it == by_word.end()) {
            order.push_back(key);
            it = by_word.emplace(key, std::vector<const PhonemeItem*>()).first;
        }
        it->second.push_back(&item);
    }
    std::vector<WordRef> out;
    for (const WordKey& key : order) {
        WordRef ref;
        ref.key = key;
        ref.last_word = std::get<2>(key);
        std::array<std::vector<std::string>, 4> combos;
        for (int ctx = 0; ctx < 4; ++ctx) {
            const bool before = ctx >= 2, after = ctx % 2 == 1;
            std::vector<std::string> syms;
            for (const PhonemeItem* item : by_word[key])
                if (Keep(item->variant, before, after))
                    syms.push_back(item->ph);
            combos[ctx] = Collapse(syms);
        }
        const bool first = std::get<2>(key) == 1;
        const bool last = std::get<2>(key) == ayah_words.at({std::get<0>(key), std::get<1>(key)});
        ref.reading.contexts = combos;
        ref.reading.ayah = std::get<1>(key);
        ref.reading.cont = combos[2 * static_cast<int>(first) + static_cast<int>(last)];
        out.push_back(std::move(ref));
    }
    return out;
}

}  // namespace mutqin
