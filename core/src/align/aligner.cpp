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

struct Jump {
    int from, to;
    double cost;
    bool skip;
    int word;
};

struct Interner {
    std::unordered_map<std::string, int> ids;
    int Id(const std::string& s) {
        auto it = ids.emplace(s, static_cast<int>(ids.size()));
        return it.first->second;
    }
};

// Edit distance where a low-confidence heard token matches anything.
int EditDistanceWild(const std::vector<int>& heard, const std::vector<char>& wild,
                     const std::vector<int>& expected) {
    std::vector<int> prev(expected.size() + 1), cur(expected.size() + 1);
    for (size_t j = 0; j <= expected.size(); ++j)
        prev[j] = static_cast<int>(j);
    for (size_t i = 1; i <= heard.size(); ++i) {
        cur[0] = static_cast<int>(i);
        for (size_t j = 1; j <= expected.size(); ++j) {
            const bool same = wild[i - 1] || heard[i - 1] == expected[j - 1];
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (same ? 0 : 1)});
        }
        prev = cur;
    }
    return prev.back();
}

int LcsWild(const std::vector<int>& heard, const std::vector<char>& wild, const std::vector<int>& expected) {
    std::vector<int> prev(expected.size() + 1, 0), cur(expected.size() + 1, 0);
    for (size_t i = 0; i < heard.size(); ++i) {
        cur[0] = 0;
        for (size_t j = 0; j < expected.size(); ++j) {
            if (wild[i] || heard[i] == expected[j])
                cur[j + 1] = prev[j] + 1;
            else
                cur[j + 1] = std::max(prev[j + 1], cur[j]);
        }
        prev = cur;
    }
    return prev.back();
}

}  // namespace

AlignResult AlignTokens(const std::vector<HeardToken>& heard, const std::vector<WordReading>& words,
                        const AlignParams& params) {
    AlignResult result;
    Interner interner;
    std::vector<int> flat_word, flat_sym, bounds;
    for (size_t w = 0; w < words.size(); ++w) {
        bounds.push_back(static_cast<int>(flat_sym.size()));
        for (const std::string& s : words[w].cont) {
            flat_word.push_back(static_cast<int>(w));
            flat_sym.push_back(interner.Id(s));
        }
    }
    bounds.push_back(static_cast<int>(flat_sym.size()));
    const int n = static_cast<int>(heard.size()), m = static_cast<int>(flat_sym.size());
    const int n_words = static_cast<int>(words.size());
    if (n == 0 || m == 0)
        return result;

    std::vector<int> heard_sym(n);
    std::vector<char> low(n);
    for (int i = 0; i < n; ++i) {
        heard_sym[i] = interner.Id(heard[i].symbol);
        low[i] = heard[i].confidence < params.low_conf;
    }

    std::vector<Jump> jumps;
    for (int b = 1; b < n_words; ++b) {
        for (int k = 1; k <= std::min(params.max_jump_words, n_words - 1 - b); ++k)
            jumps.push_back({bounds[b], bounds[b + k], params.skip_cost * k, true, b});
        for (int r = 1; r <= std::min(params.max_jump_words, b); ++r)
            jumps.push_back({bounds[b], bounds[b - r], params.repeat_cost + 0.5 * (r - 1), false, b - 1});
    }

    const double inf = std::numeric_limits<double>::infinity();
    const double sub_low = 0.4, ins_low = 0.5;
    const size_t stride = static_cast<size_t>(m) + 1;
    std::vector<double> cost(static_cast<size_t>(n + 1) * stride, inf);
    std::vector<unsigned char> bt(static_cast<size_t>(n + 1) * stride, 0);
    std::vector<int> src(static_cast<size_t>(n + 1) * stride, 0);
    for (int b : bounds)
        cost[static_cast<size_t>(b)] = 0.0;

    for (int i = 1; i <= n; ++i) {
        const double sub = low[i - 1] ? sub_low : 1.0;
        const double ins = low[i - 1] ? ins_low : 1.0;
        const double* prev = &cost[static_cast<size_t>(i - 1) * stride];
        double* row = &cost[static_cast<size_t>(i) * stride];
        unsigned char* brow = &bt[static_cast<size_t>(i) * stride];
        int* srow = &src[static_cast<size_t>(i) * stride];
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
                    srow[jp.to] = jp.from;
                    changed = true;
                }
            }
            if (!changed)
                break;
        }
    }

    int j = 0;
    {
        const double* last = &cost[static_cast<size_t>(n) * stride];
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
            const int frm = src[static_cast<size_t>(i) * stride + j];
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
    std::map<std::pair<int, int>, std::pair<bool, int>> jump_of;
    for (const Jump& jp : jumps)
        jump_of[{jp.from, jp.to}] = {jp.skip, jp.word};
    auto word_at_bound = [&](int pos) {
        return static_cast<int>(std::find(bounds.begin(), bounds.end(), pos) - bounds.begin());
    };

    const int span = last_w - first_w + 1;
    std::vector<std::vector<int>> aligned_at(span);  // heard indices aligned to each word
    std::vector<char> skipped(span, 0);
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
            if (in_span(w))
                aligned_at[w - first_w].push_back(op.a);
        } else if (op.type == Step::kJumpStep) {
            const auto info = jump_of[{op.a, op.b}];
            const int w = info.second;
            if (info.first) {
                for (int sw = word_at_bound(op.a); sw < word_at_bound(op.b); ++sw) {
                    if (in_span(sw)) {
                        skipped[sw - first_w] = 1;
                        result.events.push_back({"skipped", sw, 1.0});
                    }
                }
            } else {
                const int end_pos = op.a;
                int pos = op.b;
                std::vector<int> taken;
                while (k < ops.size() && pos < end_pos) {
                    const Step nxt = ops[k++];
                    if (nxt.type == Step::kDiagStep) {
                        taken.push_back(nxt.a);
                        pos = nxt.b + 1;
                    } else if (nxt.type == Step::kInsStep) {
                        taken.push_back(nxt.a);
                    } else if (nxt.type == Step::kDelStep) {
                        pos = nxt.a + 1;
                    }
                }
                std::vector<int> target(flat_sym.begin() + op.b, flat_sym.begin() + end_pos);
                std::vector<int> taken_sym;
                std::vector<char> taken_low;
                for (int t : taken) {
                    taken_sym.push_back(heard_sym[t]);
                    taken_low.push_back(low[t]);
                }
                const double ratio = static_cast<double>(LcsWild(taken_sym, taken_low, target)) /
                                     static_cast<double>(std::max<size_t>(1, target.size()));
                if (ratio >= params.repeat_min_ratio)
                    result.events.push_back({"repeated", w, ratio});
                else if (static_cast<int>(taken.size()) >= params.add_min_tokens)
                    result.events.push_back({"added", w, 1.0});
                cur_word = w;
                has_cur = true;
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
            const int d = EditDistanceWild(word_sym, word_low, alt_sym);
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
        if (status != "ok")
            result.events.push_back({status, w, 1.0 - ratio});
    }
    std::stable_sort(result.events.begin(), result.events.end(),
                     [](const AlignEvent& x, const AlignEvent& y) {
                         return x.index != y.index ? x.index < y.index : x.kind < y.kind;
                     });
    return result;
}

}  // namespace mutqin
