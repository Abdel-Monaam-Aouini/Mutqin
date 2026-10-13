"""Tasks 2.4 and 2.5: align what was heard to the expected words and report word-level events.

Method (decode, then align):
  1. The model output is greedy-CTC decoded into recognised tokens with confidences (decode.py).
  2. The tokens are aligned to the expected phonemes of the range with an edit-distance dynamic
     programme over (heard tokens, expected tokens). Besides match / substitute / insert / delete it
     has word-level jump arcs at word boundaries: skip the next 1..3 words, or repeat the last 1..3
     words, each for a small fixed cost. Whole-word skips and repeats are therefore cheap, as they are
     for a real reciter, instead of costing one edit per phoneme. The start (at a word boundary) and
     the end of the expected side are free: the reciter may begin later or stop earlier.
  3. Words between the first and the last aligned word are judged: ok, wrong or skipped. Runs of
     unexplained tokens longer than a few phonemes are added words.
Doubtful tokens (low confidence) are cheap to explain away and match anything when a word is scored,
so a missed mistake is preferred to a false alarm (AGENTS.md rule 4).
"""

from __future__ import annotations

from dataclasses import dataclass, field

from tools.prototype.decode import Token, greedy_decode
from tools.prototype.phoneme_data import WordPhonemes


@dataclass(frozen=True)
class Params:
    wrong_below: float = 0.5  # word match ratio under which an aligned word is "wrong"
    skip_aligned_max: float = 0.25  # aligned tokens / expected tokens at or under which it is "skipped"
    low_conf: float = 0.55  # tokens under this confidence are cheap to substitute / insert, and wildcards
    min_mismatch: int = 2  # mismatched tokens needed before a word can be called wrong
    repeat_min_ratio: float = 0.75  # a repeat jump must reproduce the repeated words this well
    skip_cost: float = 1.5  # per word skipped by a jump arc
    repeat_cost: float = 1.0  # a repeat jump arc (+0.5 for each extra word repeated)
    add_min_tokens: int = 4  # unexplained run length (between words) to call an added word
    add_inside_min: int = 3  # same, for a run in the middle of a word (noise rarely does this)
    max_jump_words: int = 3
    pause_frames: int = 8  # a gap this long between tokens is a pause (a stop is possible there)


@dataclass(frozen=True)
class Event:
    kind: str  # "skipped" | "wrong" | "repeated" | "added"
    index: int  # expected word index (for "added": the word it follows; for "repeated": the last repeated)
    confidence: float


@dataclass
class AlignResult:
    statuses: dict[int, str] = field(default_factory=dict)  # judged word index -> ok | wrong | skipped
    events: list[Event] = field(default_factory=list)
    span: tuple[int, int] | None = None  # first and last judged word index
    ops: list = field(default_factory=list)  # the alignment, for debugging


def edit_distance_wild(heard: list[Token], expected: tuple[str, ...], low_conf: float) -> int:
    """Edit distance between the tokens heard for a word and its expected phonemes, where a
    low-confidence heard token matches anything (it may just be noise)."""
    prev = list(range(len(expected) + 1))
    for i, tok in enumerate(heard, 1):
        wild = tok.confidence < low_conf
        cur = [i]
        for j, sym in enumerate(expected, 1):
            same = wild or tok.symbol == sym
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (0 if same else 1)))
        prev = cur
    return prev[-1]


def lcs_wild(heard: list[Token], expected: tuple[str, ...], low_conf: float) -> int:
    """Longest common subsequence where a low-confidence heard token matches anything."""
    prev = [0] * (len(expected) + 1)
    for tok in heard:
        wild = tok.confidence < low_conf
        cur = [0]
        for j, sym in enumerate(expected):
            if wild or tok.symbol == sym:
                cur.append(prev[j] + 1)
            else:
                cur.append(max(prev[j + 1], cur[j]))
        prev = cur
    return prev[-1]


_DIAG, _DEL, _INS, _JUMP = 1, 2, 3, 4


def align_tokens(heard: list[Token], words: list[WordPhonemes], params: Params | None = None) -> AlignResult:
    """Align recognised tokens to the expected words (their continuing reading) and judge them."""
    params = params or Params()
    flat: list[tuple[int, str]] = []
    bounds: list[int] = []
    for w, wp in enumerate(words):
        bounds.append(len(flat))
        flat += [(w, sym) for sym in wp.cont]
    bounds.append(len(flat))
    n, m, n_words = len(heard), len(flat), len(words)
    if n == 0 or m == 0:
        return AlignResult()

    # jump arcs: (from position, to position, cost, kind, words)
    jumps: list[tuple[int, int, float, str, int]] = []
    for b in range(1, n_words):
        for k in range(1, min(params.max_jump_words, n_words - 1 - b) + 1):
            jumps.append((bounds[b], bounds[b + k], params.skip_cost * k, "skip", b))
        for r in range(1, min(params.max_jump_words, b) + 1):
            jumps.append((bounds[b], bounds[b - r], params.repeat_cost + 0.5 * (r - 1), "repeat", b - 1))

    inf = float("inf")
    sub_low, ins_low = 0.4, 0.5
    cost = [[inf] * (m + 1) for _ in range(n + 1)]
    bt = [[0] * (m + 1) for _ in range(n + 1)]
    src = [[0] * (m + 1) for _ in range(n + 1)]
    for b in bounds:
        cost[0][b] = 0.0  # free start at a word boundary

    for i in range(1, n + 1):
        tok = heard[i - 1]
        low = tok.confidence < params.low_conf
        sub = sub_low if low else 1.0
        ins = ins_low if low else 1.0
        prev, row, brow = cost[i - 1], cost[i], bt[i]
        for j in range(m + 1):
            best, kind = prev[j] + ins, _INS
            if j > 0:
                d = prev[j - 1] + (0.0 if tok.symbol == flat[j - 1][1] else sub)
                if d <= best:
                    best, kind = d, _DIAG
            row[j], brow[j] = best, kind
        for _ in range(4):  # relax deletions and jump arcs within the row until nothing improves
            changed = False
            for j in range(1, m + 1):
                if row[j - 1] + 1.0 < row[j] - 1e-9:
                    row[j], brow[j], changed = row[j - 1] + 1.0, _DEL, True
            for frm, to, c, _kind, _w in jumps:
                if row[frm] + c < row[to] - 1e-9:
                    row[to], brow[to], src[i][to], changed = row[frm] + c, _JUMP, frm, True
            if not changed:
                break

    j = min(range(m + 1), key=lambda jj: (cost[n][jj], -jj))  # free end on the expected side
    ops: list[tuple] = []  # built backwards
    i = n
    for _ in range((n + m) * 6 + 10):
        if i == 0:
            break
        kind = bt[i][j]
        if kind == _DIAG:
            ops.append(("diag", i - 1, j - 1))
            i, j = i - 1, j - 1
        elif kind == _DEL:
            ops.append(("del", j - 1))
            j -= 1
        elif kind == _INS:
            ops.append(("ins", i - 1))
            i -= 1
        else:
            frm = src[i][j]
            ops.append(("jump", frm, j))
            j = frm
    ops.reverse()

    diag_ops = [o for o in ops if o[0] == "diag"]
    if not diag_ops:
        return AlignResult()
    first_w, last_w = flat[diag_ops[0][2]][0], flat[diag_ops[-1][2]][0]
    result = AlignResult(span=(first_w, last_w), ops=ops)
    jump_of = {(frm, to): (kind, w) for frm, to, _c, kind, w in jumps}

    aligned: dict[int, list[Token]] = {w: [] for w in range(first_w, last_w + 1)}
    aligned_at: dict[int, list[int]] = {w: [] for w in range(first_w, last_w + 1)}
    skipped: set[int] = set()
    cur_word: int | None = None
    run: list[Token] = []

    def flush_run(next_word: int | None = None) -> None:
        if cur_word is not None:
            if next_word == cur_word and len(run) >= params.add_inside_min:
                # a run in the middle of one word: extra speech overlapping it, just before it
                result.events.append(Event("added", max(cur_word - 1, 0), 1.0))
            elif len(run) >= params.add_min_tokens:
                result.events.append(Event("added", cur_word, 1.0))
        run.clear()

    k = 0
    while k < len(ops):
        op = ops[k]
        k += 1
        if op[0] == "ins":
            run.append(heard[op[1]])
            continue
        flush_run(flat[op[2]][0] if op[0] == "diag" else None)
        if op[0] == "diag":
            w = flat[op[2]][0]
            cur_word = w
            if first_w <= w <= last_w:
                aligned[w].append(heard[op[1]])
                aligned_at[w].append(op[1])
        elif op[0] == "jump":
            kind, w = jump_of[(op[1], op[2])]
            if kind == "skip":
                for sw in range(bounds.index(op[1]), bounds.index(op[2])):
                    if first_w <= sw <= last_w:
                        skipped.add(sw)
                        result.events.append(Event("skipped", sw, 1.0))
            else:
                # tokens consumed while the path re-walks the repeated words
                end_pos, pos, taken = op[1], op[2], []
                while k < len(ops) and pos < end_pos:
                    nxt = ops[k]
                    k += 1
                    if nxt[0] == "diag":
                        taken.append(heard[nxt[1]])
                        pos = nxt[2] + 1
                    elif nxt[0] == "ins":
                        taken.append(heard[nxt[1]])
                    elif nxt[0] == "del":
                        pos = nxt[1] + 1
                target = tuple(sym for _w, sym in flat[op[2] : end_pos])
                ratio = lcs_wild(taken, target, params.low_conf) / max(1, len(target))
                if ratio >= params.repeat_min_ratio:
                    result.events.append(Event("repeated", w, ratio))
                elif len(taken) >= params.add_min_tokens:
                    result.events.append(Event("added", w, 1.0))
                cur_word = w
    flush_run()

    for w in range(first_w, last_w + 1):
        wp = words[w]
        if w in skipped:
            result.statuses[w] = "skipped"
            continue
        length = len(wp.cont)
        pause_before, pause_after = _pauses(heard, aligned_at[w], params.pause_frames)
        distance, expected_len = min(
            (edit_distance_wild(aligned[w], alt, params.low_conf), len(alt))
            for alt in wp.readings(pause_before, pause_after)
            if alt
        )
        ratio = max(0.0, 1.0 - distance / expected_len)
        if len(aligned[w]) <= params.skip_aligned_max * length:
            status = "skipped"
        elif ratio < params.wrong_below and distance >= params.min_mismatch:
            status = "wrong"
        else:
            status = "ok"
        result.statuses[w] = status
        if status != "ok":
            result.events.append(Event(status, w, 1.0 - ratio))
    result.events.sort(key=lambda e: (e.index, e.kind))
    return result


def _pauses(heard: list[Token], at: list[int], gap: int) -> tuple[bool, bool]:
    """Is there a pause before the first and after the last token aligned to a word?"""
    if not at:
        return False, False
    first, last = at[0], at[-1]
    before = first == 0 or heard[first].start - heard[first - 1].end >= gap
    after = last == len(heard) - 1 or heard[last + 1].start - heard[last].end >= gap
    return before, after


def bisect_word(bounds: list[int], position: int) -> int:
    """Index of the word that starts at flat position `position` (a boundary)."""
    return bounds.index(position)


class StreamingAligner:
    """Feeds frames in chunks and tracks the position. The prototype re-aligns the whole heard
    sequence on every update; the C++ version will align a sliding window incrementally."""

    def __init__(self, words: list[WordPhonemes], params: Params | None = None) -> None:
        self.words = words
        self.params = params or Params()
        self.frames: list = []

    def push(self, frames: list) -> AlignResult:
        self.frames += frames
        return align_tokens(greedy_decode(self.frames), self.words, self.params)
