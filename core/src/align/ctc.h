// CTC greedy decoding (C++ port of tools/prototype/decode.py) and the per-word expected readings.
#ifndef MUTQIN_ALIGN_CTC_H
#define MUTQIN_ALIGN_CTC_H

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "align/aligner.h"
#include "tajweed/phonemizer.h"

namespace mutqin {

// Token ids: 0 = blank, 1..kPhonemeCount = inventory symbols in contract order.
constexpr size_t kVocabSize = 1 + 31;

// One frame after argmax: the label and its probability.
struct Frame {
    int label = 0;
    float prob = 0.0f;
};

// Argmax over a row of kVocabSize probabilities.
Frame ArgmaxFrame(const float* probs);

// Collapses runs of the same label and drops blanks; confidence is the mean probability of the run.
std::vector<HeardToken> GreedyDecode(const std::vector<Frame>& frames);

// Per-word expected readings of a phonemizer result, in order of first appearance. `ayah_words`
// maps (surah, ayah) to its word count: word 1 is the first word, word `count` the last.
struct WordRef {
    WordKey key;
    WordReading reading;
};
std::vector<WordRef> BuildWordReadings(const std::vector<PhonemeItem>& items,
                                       const std::map<std::pair<int, int>, int>& ayah_words);

}  // namespace mutqin

#endif  // MUTQIN_ALIGN_CTC_H
