// Word-level aligner and detector: C++ port of tools/prototype/aligner.py (the Python prototype
// is the reference; a parity test compares both on generated cases). Doubles are used on purpose
// so the results match the Python floats exactly.
#ifndef MUTQIN_ALIGN_ALIGNER_H
#define MUTQIN_ALIGN_ALIGNER_H

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mutqin {

struct HeardToken {
    std::string symbol;
    double confidence = 0.0;  // mean probability over the token's frames
    int start = 0;            // first frame
    int end = 0;              // one past the last frame
};

// Expected phonemes of one word. contexts[2 * pause_before + pause_after] is the reading for that
// combination; `cont` is the reading when the reciter stops only at ayah ends.
struct WordReading {
    std::vector<std::string> cont;
    std::array<std::vector<std::string>, 4> contexts;
};

struct AlignParams {
    double wrong_below = 0.5;
    double skip_aligned_max = 0.25;
    double low_conf = 0.55;
    int min_mismatch = 2;
    double repeat_min_ratio = 0.75;
    double skip_cost = 1.5;
    double repeat_cost = 1.0;
    int add_min_tokens = 4;
    int add_inside_min = 3;
    int max_jump_words = 3;
    int pause_frames = 8;
    int edge_free =
        0;  // leading / trailing expected units (at most 1/3 of a word) that may be missing at no cost
};

struct AlignEvent {
    std::string kind;  // "skipped" | "wrong" | "repeated" | "added"
    int index = 0;
    double confidence = 0.0;
};

struct AlignResult {
    std::map<int, std::string> statuses;  // judged word index -> ok | wrong | skipped
    std::vector<AlignEvent> events;
    bool has_span = false;
    int span_first = 0, span_last = 0;
};

AlignResult AlignTokens(const std::vector<HeardToken>& heard, const std::vector<WordReading>& words,
                        const AlignParams& params = AlignParams());

// Same result as AlignTokens on the full heard list, but keeps the DP rows between calls: each
// Update() recomputes only the rows from the first heard token that changed (normally the last
// one), so a long recitation costs one new row per new token instead of a full re-alignment.
class StreamingAligner {
public:
    StreamingAligner(std::vector<WordReading> words, const AlignParams& params = AlignParams());
    ~StreamingAligner();
    StreamingAligner(const StreamingAligner&) = delete;
    StreamingAligner& operator=(const StreamingAligner&) = delete;

    // `heard` is the complete decoded list so far (it only grows or changes at its end).
    const AlignResult& Update(const std::vector<HeardToken>& heard);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace mutqin

#endif  // MUTQIN_ALIGN_ALIGNER_H
