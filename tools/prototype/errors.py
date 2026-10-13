"""Task 2.2: synthetic recitation errors, text side.

Takes the expected words of an ayah range and produces what a reciter "said": the original words
with injected mistakes, plus ground-truth labels. Kinds:
  skipped   an expected word is not said
  wrong     an expected word is replaced by a different word of similar length
  repeated  an expected word is said twice
  added     an extra word is said after an expected word
"""

from __future__ import annotations

import random
from dataclasses import dataclass, field

from tools.prototype.phoneme_data import WordKey, WordPhonemes

KINDS = ("skipped", "wrong", "repeated", "added")


@dataclass(frozen=True)
class TruthEvent:
    kind: str  # one of KINDS
    index: int  # expected word index (for "added": the word it follows)


@dataclass(frozen=True)
class SpokenWord:
    tokens: tuple[str, ...]
    source: WordKey | None  # which Quran word these tokens come from


@dataclass
class Utterance:
    expected: list[WordKey]
    spoken: list[SpokenWord]
    truth: list[TruthEvent] = field(default_factory=list)
    # spoken index after which the reciter paused (stop reading), for tests of both readings
    pauses: set[int] = field(default_factory=set)


def pick_range(rng: random.Random, words: dict[WordKey, WordPhonemes], max_ayahs: int = 3) -> list[WordKey]:
    """A random run of consecutive ayahs (same surah) in which every word has phonemes."""
    by_ayah: dict[tuple[int, int], list[WordKey]] = {}
    for key in words:
        by_ayah.setdefault((key[0], key[1]), []).append(key)
    ayahs = sorted(by_ayah)
    while True:
        start = rng.choice(ayahs)
        n = rng.randint(1, max_ayahs)
        chosen = []
        for a in range(start[1], start[1] + n):
            keys = by_ayah.get((start[0], a))
            if keys is None:
                break
            chosen.append(sorted(keys))
        flat = [k for keys in chosen for k in keys]
        if flat and all(words[k].cont for k in flat) and len(flat) >= 4:
            return flat


def edit_distance(a: tuple[str, ...], b: tuple[str, ...]) -> int:
    prev = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        cur = [i]
        for j, y in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (x != y)))
        prev = cur
    return prev[-1]


def _different_word(
    rng: random.Random,
    words: dict[WordKey, WordPhonemes],
    like: WordKey,
    avoid_near: list[WordKey],
    min_distance: float = 0.5,
) -> WordKey:
    """A word of similar length that is clearly different (normalized edit distance of at least
    `min_distance`) from `like` and from the neighbouring words: a one-letter variant would be a
    letter error (Phase 4), not a wrong word."""
    target = words[like].cont
    near = [words[k].cont for k in avoid_near]
    pool = []
    for k, wp in words.items():
        if not wp.cont or abs(len(wp.cont) - len(target)) > 1:
            continue
        if all(
            edit_distance(wp.cont, o) / max(len(wp.cont), len(o)) >= min_distance for o in [target, *near]
        ):
            pool.append(k)
            if len(pool) >= 400:
                break
    return rng.choice(pool)


def make_utterance(
    rng: random.Random,
    words: dict[WordKey, WordPhonemes],
    expected: list[WordKey],
    errors: tuple[str, ...] = (),
    pause_prob: float = 0.15,
) -> Utterance:
    """Say `expected`, with one injected mistake of each kind in `errors` (on distinct, non-adjacent
    words, never at the first or last word so the recitation start and end stay clear)."""
    n = len(expected)
    candidates = list(range(1, n - 1)) if n > 2 else []
    chosen: dict[int, str] = {}
    for kind in errors:
        free = [i for i in candidates if i not in chosen and i - 1 not in chosen and i + 1 not in chosen]
        if not free:
            raise ValueError("range too short for the requested errors")
        chosen[rng.choice(free)] = kind

    spoken: list[SpokenWord] = []
    truth: list[TruthEvent] = []
    for i, key in enumerate(expected):
        kind = chosen.get(i)
        own = SpokenWord(words[key].cont, key)
        if kind == "skipped":
            truth.append(TruthEvent("skipped", i))
        elif kind == "wrong":
            other = _different_word(rng, words, key, [expected[x] for x in (i - 1, i + 1) if 0 <= x < n])
            spoken.append(SpokenWord(words[other].cont, other))
            truth.append(TruthEvent("wrong", i))
        else:
            spoken.append(own)
            if kind == "repeated":
                spoken.append(own)
                truth.append(TruthEvent("repeated", i))
            elif kind == "added":
                extra = _different_word(rng, words, key, [expected[x] for x in (i, i + 1) if 0 <= x < n])
                spoken.append(SpokenWord(words[extra].cont, extra))
                truth.append(TruthEvent("added", i))
    pauses = {j for j in range(len(spoken) - 1) if rng.random() < pause_prob}
    return Utterance(expected, spoken, truth, pauses)


def render(utt: Utterance, words: dict[WordKey, WordPhonemes]) -> tuple[list[str], set[int]]:
    """The phoneme tokens a reciter produces for the utterance, and the token indices after which
    there is a pause. Each word is said in its own context: after a pause (or at the start) it takes
    the starting form, before a pause (or at the end) the stopping form."""
    tokens: list[str] = []
    pause_after: set[int] = set()
    last = len(utt.spoken) - 1
    for j, word in enumerate(utt.spoken):
        after = j in utt.pauses or j == last
        before = j == 0 or (j - 1) in utt.pauses
        use = words[word.source].tokens_for(before, after) if word.source is not None else word.tokens
        tokens += use
        if j in utt.pauses and use:
            pause_after.add(len(tokens) - 1)
    return tokens, pause_after
