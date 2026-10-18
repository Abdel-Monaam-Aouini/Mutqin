#include "align/aligner.h"

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace mutqin {

namespace {

enum Op { kDiag = 1, kDel = 2, kIns = 3, kJump = 4 };

struct Step {
    enum Type { kDiagStep, kDelStep, kInsStep, kJumpStep } type;
    int a = 0, b = 0;  // diag: heard, flat; del: flat; ins: heard; jump: from, to
};

enum class JumpKind { kSkip, kRestart };

struct Jump {
    int from, to;
    double cost;
    JumpKind kind;
    int word;
};

struct Interner {
    std::unordered_map<std::string, int> ids;
    int Id(const std::string& s) {
        auto it = ids.emplace(s, static_cast<int>(ids.size()));
        return it.first->second;
    }
};

// Edit distance where a low-confidence heard token matches anything. Up to `edge_free` expected
// units may be missing at the start and at the end for free (the neighbouring word took them).
int EditDistanceWild(const std::vector<int>& heard, const std::vector<char>& wild,
                     const std::vector<int>& expected, int edge_free) {
    // at most a third of the word may be missing at its edges
    const int free_units = std::min(edge_free, static_cast<int>(expected.size()) / 3);
    std::vector<int> prev(expected.size() + 1), cur(expected.size() + 1);
    for (size_t j = 0; j <= expected.size(); ++j)
        prev[j] = static_cast<int>(j) <= free_units ? 0 : static_cast<int>(j);
    for (size_t i = 1; i <= heard.size(); ++i) {
        cur[0] = static_cast<int>(i);
        for (size_t j = 1; j <= expected.size(); ++j) {
            const bool same = wild[i - 1] || heard[i - 1] == expected[j - 1];
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (same ? 0 : 1)});
        }
        prev = cur;
    }
    int best = prev[expected.size()];
    for (int t = 1; t <= free_units; ++t)
        best = std::min(best, prev[expected.size() - static_cast<size_t>(t)]);
    return best;
}

}  // namespace

namespace {

// DP state kept between updates: rows only depend on earlier rows, so appended heard tokens
// only need new rows. A row is recomputed if its token changed (the last token of a growing
// decode can change confidence or symbol).
struct DpState {
    bool ready = false;
    Interner interner;
    std::vector<int> flat_word, flat_sym, bounds;
    std::vector<Jump> jumps;
    int m = 0, n_words = 0;
    size_t stride = 0;
    // Memory is bounded: the backtrace needs one byte per cell (bt) and the jump sources, but the
    // costs only matter for the last rows, so only kRing of them are kept (plus row 0).
    static constexpr size_t kRing = 8;
    std::vector<double> row0;
    std::vector<double> ring;  // row i (i >= 1) lives in slot i % kRing
    std::vector<unsigned char> bt;
    std::vector<std::vector<std::pair<int, int>>> jump_src;  // per row: (to, from) of the jump cells

    double* Cost(size_t i) { return i == 0 ? row0.data() : &ring[(i % kRing) * stride]; }
    std::vector<int> row_sym;  // symbol of the heard token behind DP row i + 1
    std::vector<char> row_low;
};

AlignResult AlignImpl(DpState& st, const std::vector<HeardToken>& heard,
                      const std::vector<WordReading>& words, const AlignParams& params) {
    AlignResult result;
    if (!st.ready) {
        for (size_t w = 0; w < words.size(); ++w) {
            st.bounds.push_back(static_cast<int>(st.flat_sym.size()));
            for (const std::string& s : words[w].cont) {
                st.flat_word.push_back(static_cast<int>(w));
                st.flat_sym.push_back(st.interner.Id(s));
            }
        }
        st.bounds.push_back(static_cast<int>(st.flat_sym.size()));
        st.m = static_cast<int>(st.flat_sym.size());
        st.n_words = static_cast<int>(words.size());
        for (int b = 1; b < st.n_words; ++b) {
            for (int k = 1; k <= std::min(params.max_jump_words, st.n_words - 1 - b); ++k)
                st.jumps.push_back(
                    {st.bounds[b], st.bounds[b + k], params.skip_cost * k, JumpKind::kSkip, b});
            for (int r = 1; r <= std::min(params.max_jump_words, b); ++r)
                st.jumps.push_back({st.bounds[b], st.bounds[b - r], params.repeat_cost + 0.5 * (r - 1),
                                    JumpKind::kRestart, b - 1});
        }
        // Going back and reading again is how one learns: from any word boundary of an ayah (its end
        // included) back to an earlier word of the same ayah, from its end back to the start of the last few
        // ayahs. None of these is reported, and the reading that comes last is the one that counts (see the
        // judging below).
        std::vector<int> starts;
        for (int w = 0; w < st.n_words; ++w)
            if (w == 0 || words[static_cast<size_t>(w)].ayah != words[static_cast<size_t>(w) - 1].ayah)
                starts.push_back(w);
        for (size_t i = 0; i < starts.size(); ++i) {
            const int first = starts[i];
            const int end = i + 1 < starts.size() ? starts[i + 1] : st.n_words;
            for (int b = first + 1; b <= end; ++b) {
                std::vector<int> targets;
                targets.push_back(first);
                for (int t = std::max(first + 1, b - params.max_restart_words); t < b; ++t)
                    targets.push_back(t);
                for (int t : targets)
                    st.jumps.push_back({st.bounds[static_cast<size_t>(b)], st.bounds[static_cast<size_t>(t)],
                                        params.restart_cost + 0.02 * (b - t - 1), JumpKind::kRestart, b - 1});
            }
            for (size_t k = 1; k < static_cast<size_t>(params.max_restart_ayahs) && k <= i; ++k)
                st.jumps.push_back(
                    {st.bounds[static_cast<size_t>(end)], st.bounds[static_cast<size_t>(starts[i - k])],
                     params.restart_cost + 0.5 * static_cast<double>(k), JumpKind::kRestart, end - 1});
        }
        st.stride = static_cast<size_t>(st.m) + 1;
        st.row0.assign(st.stride, std::numeric_limits<double>::infinity());
        st.ring.assign(DpState::kRing * st.stride, std::numeric_limits<double>::infinity());
        st.bt.assign(st.stride, 0);
        st.jump_src.assign(1, {});
        for (int b : st.bounds)
            st.row0[static_cast<size_t>(b)] = 0.0;  // free start at a word boundary
        st.ready = true;
    }
    auto& flat_word = st.flat_word;
    auto& flat_sym = st.flat_sym;
    auto& bounds = st.bounds;
    auto& jumps = st.jumps;
    auto& interner = st.interner;
    auto& bt = st.bt;
    const int m = st.m;
    const size_t stride = st.stride;
    const int n = static_cast<int>(heard.size());
    if (n == 0 || m == 0)
        return result;

    std::vector<int> heard_sym(n);
    std::vector<char> low(n);
    for (int i = 0; i < n; ++i) {
        heard_sym[i] = interner.Id(heard[i].symbol);
        low[i] = heard[i].confidence < params.low_conf;
    }

    // First row whose token differs from the stored one; everything before it is reused.
    size_t keep = 0;
    while (keep < st.row_sym.size() && keep < heard.size() && st.row_sym[keep] == heard_sym[keep] &&
           st.row_low[keep] == low[keep])
        ++keep;
    // Only the last kRing cost rows survive; if the first changed row is older than that, start over.
    const size_t rows_before = st.row_sym.size();
    if (keep > 0 && keep + DpState::kRing <= rows_before)
        keep = 0;
    st.row_sym.assign(heard_sym.begin(), heard_sym.end());
    st.row_low.assign(low.begin(), low.end());
    bt.resize(static_cast<size_t>(n + 1) * stride, 0);
    st.jump_src.resize(static_cast<size_t>(n) + 1);

    const double sub_low = 0.4, ins_low = 0.5;
    for (int i = static_cast<int>(keep) + 1; i <= n; ++i) {
        const double sub = low[i - 1] ? sub_low : 1.0;
        const double ins = low[i - 1] ? ins_low : 1.0;
        const double* prev = st.Cost(static_cast<size_t>(i - 1));
        double* row = st.Cost(static_cast<size_t>(i));
        unsigned char* brow = &bt[static_cast<size_t>(i) * stride];
        auto& jump_src = st.jump_src[static_cast<size_t>(i)];
        jump_src.clear();
        for (int j = 0; j <= m; ++j) {
            double best = prev[j] + ins;
            unsigned char kind = kIns;
            if (j > 0) {
                const double d = prev[j - 1] + (heard_sym[i - 1] == flat_sym[j - 1] ? 0.0 : sub);
                if (d <= best) {
                    best = d;
                    kind = kDiag;
                }
            }
            row[j] = best;
            brow[j] = kind;
        }
        for (int round = 0; round < 4; ++round) {
            bool changed = false;
            for (int j = 1; j <= m; ++j) {
                if (row[j - 1] + 1.0 < row[j] - 1e-9) {
                    row[j] = row[j - 1] + 1.0;
                    brow[j] = kDel;
                    changed = true;
                }
            }
            for (const Jump& jp : jumps) {
                if (row[jp.from] + jp.cost < row[jp.to] - 1e-9) {
                    row[jp.to] = row[jp.from] + jp.cost;
                    brow[jp.to] = kJump;
                    bool found = false;
                    for (auto& entry : jump_src) {
                        if (entry.first == jp.to) {
                            entry.second = jp.from;
                            found = true;
                        }
                    }
                    if (!found)
                        jump_src.push_back({jp.to, jp.from});
                    changed = true;
                }
            }
            if (!changed)
                break;
        }
    }

    int j = 0;
    {
        const double* last = st.Cost(static_cast<size_t>(n));
        for (int jj = 1; jj <= m; ++jj)
            if (last[jj] <= last[j])
                j = jj;  // lowest cost, ties to the larger index
    }
    std::vector<Step> ops;
    int i = n;
    for (long iter = 0; iter < static_cast<long>(n + m) * 6 + 10; ++iter) {
        if (i == 0)
            break;
        const unsigned char kind = bt[static_cast<size_t>(i) * stride + j];
        if (kind == kDiag) {
            ops.push_back({Step::kDiagStep, i - 1, j - 1});
            --i;
            --j;
        } else if (kind == kDel) {
            ops.push_back({Step::kDelStep, j - 1, 0});
            --j;
        } else if (kind == kIns) {
            ops.push_back({Step::kInsStep, i - 1, 0});
            --i;
        } else {
            int frm = 0;
            for (const auto& entry : st.jump_src[static_cast<size_t>(i)])
                if (entry.first == j)
                    frm = entry.second;
            ops.push_back({Step::kJumpStep, frm, j});
            j = frm;
        }
    }
    std::reverse(ops.begin(), ops.end());

    int first_w = -1, last_w = -1;
    for (const Step& s : ops) {
        if (s.type == Step::kDiagStep) {
            if (first_w < 0)
                first_w = flat_word[s.b];
            last_w = flat_word[s.b];
        }
    }
    if (first_w < 0)
        return result;
    result.has_span = true;
    result.span_first = first_w;
    result.span_last = last_w;

    // Later jumps overwrite earlier ones with the same (from, to), as in the Python dict.
    std::map<std::pair<int, int>, std::pair<JumpKind, int>> jump_of;
    for (const Jump& jp : jumps)
        jump_of[{jp.from, jp.to}] = {jp.kind, jp.word};
    auto word_at_bound = [&](int pos) {
        return static_cast<int>(std::find(bounds.begin(), bounds.end(), pos) - bounds.begin());
    };

    const int span = last_w - first_w + 1;
    std::vector<std::vector<int>> aligned_at(span);  // heard indices aligned to each word
    std::vector<char> skipped(span, 0);
    std::vector<int> seen_pass(static_cast<size_t>(span),
                               -1);  // the reading (0 = the first) that last gave a word tokens
    int pass_id = 0;
    bool has_cur = false;
    int cur_word = 0;
    std::vector<int> run;

    auto in_span = [&](int w) { return first_w <= w && w <= last_w; };
    auto flush_run = [&](bool has_next, int next_word) {
        if (has_cur) {
            if (has_next && next_word == cur_word && static_cast<int>(run.size()) >= params.add_inside_min)
                result.events.push_back({"added", std::max(cur_word - 1, 0), 1.0});
            else if (static_cast<int>(run.size()) >= params.add_min_tokens)
                result.events.push_back({"added", cur_word, 1.0});
        }
        run.clear();
    };

    size_t k = 0;
    while (k < ops.size()) {
        const Step op = ops[k++];
        if (op.type == Step::kInsStep) {
            run.push_back(op.a);
            continue;
        }
        if (op.type == Step::kDiagStep)
            flush_run(true, flat_word[op.b]);
        else
            flush_run(false, 0);
        if (op.type == Step::kDiagStep) {
            const int w = flat_word[op.b];
            cur_word = w;
            has_cur = true;
            if (in_span(w)) {
                const size_t slot = static_cast<size_t>(w - first_w);
                if ((seen_pass[slot] >= 0 && seen_pass[slot] != pass_id) || skipped[slot]) {
                    // this word was read (or skipped) before and is read again now: the new reading replaces
                    // it
                    aligned_at[slot].clear();
                    skipped[slot] = 0;
                    result.events.erase(std::remove_if(result.events.begin(), result.events.end(),
                                                       [w](const AlignEvent& e) { return e.index == w; }),
                                        result.events.end());
                }
                seen_pass[slot] = pass_id;
                aligned_at[slot].push_back(op.a);
            }
        } else if (op.type == Step::kJumpStep) {
            const auto info = jump_of[{op.a, op.b}];
            if (info.first == JumpKind::kSkip) {
                for (int sw = word_at_bound(op.a); sw < word_at_bound(op.b); ++sw) {
                    if (in_span(sw)) {
                        skipped[sw - first_w] = 1;
                        result.events.push_back({"skipped", sw, 1.0});
                    }
                }
            } else {  // going back to read again: what is read from here on replaces the earlier reading
                ++pass_id;
                has_cur = false;
            }
        }
    }
    flush_run(false, 0);

    for (int w = first_w; w <= last_w; ++w) {
        if (skipped[w - first_w]) {
            result.statuses[w] = "skipped";
            continue;
        }
        const WordReading& wp = words[w];
        const std::vector<int>& at = aligned_at[w - first_w];
        if (!at.empty())
            result.first_heard[w] = at.front();
        const size_t length = wp.cont.size();
        bool pause_before = false, pause_after = false;
        if (!at.empty()) {
            const int first = at.front(), last = at.back();
            pause_before = first == 0 || heard[first].start - heard[first - 1].end >= params.pause_frames;
            pause_after = last == n - 1 || heard[last + 1].start - heard[last].end >= params.pause_frames;
        }
        std::vector<int> word_sym;
        std::vector<char> word_low;
        for (int idx : at) {
            word_sym.push_back(heard_sym[idx]);
            word_low.push_back(low[idx]);
        }
        bool have = false;
        int best_dist = 0;
        size_t best_len = 0;
        for (int ctx = 0; ctx < 4; ++ctx) {
            const bool b = ctx >= 2, a = ctx % 2 == 1;
            if ((b && !pause_before) || (a && !pause_after))
                continue;
            const std::vector<std::string>& alt = wp.contexts[ctx];
            if (alt.empty())
                continue;
            std::vector<int> alt_sym;
            for (const std::string& s : alt)
                alt_sym.push_back(interner.Id(s));
            const int d = EditDistanceWild(word_sym, word_low, alt_sym, params.edge_free);
            if (!have || d < best_dist || (d == best_dist && alt.size() < best_len)) {
                have = true;
                best_dist = d;
                best_len = alt.size();
            }
        }
        if (!have) {  // Python raises here; a word with no reading cannot be judged
            result.statuses[w] = "ok";
            continue;
        }
        const double ratio =
            std::max(0.0, 1.0 - static_cast<double>(best_dist) / static_cast<double>(best_len));
        std::string status = "ok";
        if (static_cast<double>(at.size()) <= params.skip_aligned_max * static_cast<double>(length))
            status = "skipped";
        else if (ratio < params.wrong_below && best_dist >= params.min_mismatch)
            status = "wrong";
        result.statuses[w] = status;
        if (status == "wrong") {
            std::vector<std::string>& units = result.heard_units[w];
            for (int idx : at)
                units.push_back(heard[idx].symbol);
        }
        if (status != "ok")
            result.events.push_back({status, w, 1.0 - ratio});
    }
    std::stable_sort(result.events.begin(), result.events.end(),
                     [](const AlignEvent& x, const AlignEvent& y) {
                         return x.index != y.index ? x.index < y.index : x.kind < y.kind;
                     });
    return result;
}

}  // namespace

AlignParams StrictnessProfile(int level) {
    AlignParams p;
    if (level == 0) {  // relaxed
        p.edge_free = 3;
        p.add_min_tokens = 4;
        p.wrong_below = 0.4;
        p.min_mismatch = 3;
    } else if (level == 2) {  // strict
        p.edge_free = 1;
        p.add_min_tokens = 2;
        p.wrong_below = 0.6;
    } else {  // standard
        p.edge_free = 2;
        p.add_min_tokens = 3;
    }
    return p;
}

AlignResult AlignTokens(const std::vector<HeardToken>& heard, const std::vector<WordReading>& words,
                        const AlignParams& params) {
    DpState state;
    return AlignImpl(state, heard, words, params);
}

struct StreamingAligner::Impl {
    std::vector<WordReading> words;
    AlignParams params;
    DpState state;
    AlignResult result;
};

StreamingAligner::StreamingAligner(std::vector<WordReading> words, const AlignParams& params)
    : impl_(new Impl()) {
    impl_->words = std::move(words);
    impl_->params = params;
}

StreamingAligner::~StreamingAligner() = default;

const AlignResult& StreamingAligner::Update(const std::vector<HeardToken>& heard) {
    impl_->result = AlignImpl(impl_->state, heard, impl_->words, impl_->params);
    return impl_->result;
}

}  // namespace mutqin
