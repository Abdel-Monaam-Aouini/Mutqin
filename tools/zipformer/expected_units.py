"""Expected units per Tanzil word for the Quran phoneme Zipformer (Muno459/zipformer_p-quran).

The model ships `ordered_quran_phonemes.json`: for every ayah the canonical units of each "phoneme word".
Those phoneme words are NOT the Tanzil words: where a noon or tanween sound is assimilated into the next
word (ikhfa, idgham, iqlab) the model's phonetizer glues words together, so 4,115 of 6,236 ayahs have fewer
phoneme words than Tanzil words. This tool maps them back by aligning the consonant letters of both
sequences (ours come from the reference phonemizer, theirs from the unit letters) and then assigning
Tanzil words to groups monotonically. Nothing here interprets the units themselves.

Output (`expected_units.tsv`, derived from the model's files, NPL-1.1, never committed):
  surah:ayah <TAB> first_word-last_word <TAB> unit unit unit ...
one line per group, in recitation order. A group covers one or more consecutive Tanzil words; when one
Tanzil word owns several groups (spelled-out letters such as 2:1) each group is its own line with the
same word span.
Usage: python3 -I tools/zipformer/expected_units.py [--model-dir DIR] [--out FILE]
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.phonemizer import inventory as inv  # noqa: E402
from tools.phonemizer.phonemize import phonemize_range  # noqa: E402
from tools.quran_text.loader import load_quran  # noqa: E402
from tools.quran_text.model import Quran  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
Group = tuple[int, int, tuple[str, ...]]  # first word, last word (1-based, inclusive), units


def tokenize(text: str, units: set[str]) -> list[str]:
    """Greedy longest-match segmentation into model units (as the model card describes)."""
    longest = max(len(u) for u in units)
    out: list[str] = []
    i = 0
    while i < len(text):
        for length in range(min(longest, len(text) - i), 0, -1):
            if text[i : i + length] in units:
                out.append(text[i : i + length])
                i += length
                break
        else:
            raise ValueError(f"unit not in the model vocabulary: {text[i]!r}")
    return out


def _collapse(seq: list[tuple[str, int]]) -> list[tuple[str, int]]:
    out: list[tuple[str, int]] = []
    for item in seq:
        if not out or out[-1][0] != item[0]:
            out.append(item)
    return out


def _their_consonants(groups: list[str]) -> list[tuple[str, int]]:
    seq = []
    for gi, text in enumerate(groups):
        for ch in text:
            d = inv.BY_CODEPOINT.get(ord(ch))
            if d is not None and d.kind != inv.Kind.VOWEL:
                seq.append((d.symbol, gi))
    return _collapse(seq)


def _our_consonants(items: list) -> list[tuple[str, int]]:
    seq = []
    for it in items:
        if it.role == "cons":
            seq.append((it.ph, it.word))
        elif it.role == "tanween_n":
            seq.append(("n", it.word))
    return _collapse(seq)


def _align(a: list[tuple[str, int]], b: list[tuple[str, int]]) -> list[tuple[int, int]]:
    """Global alignment of two consonant sequences (match 0, mismatch 2, gap 1)."""
    n, m = len(a), len(b)
    cost = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        cost[i][0] = i
    for j in range(1, m + 1):
        cost[0][j] = j
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            sub = 0 if a[i - 1][0] == b[j - 1][0] else 2
            cost[i][j] = min(cost[i - 1][j] + 1, cost[i][j - 1] + 1, cost[i - 1][j - 1] + sub)
    i, j, pairs = n, m, []
    while i > 0 and j > 0:
        sub = 0 if a[i - 1][0] == b[j - 1][0] else 2
        if cost[i][j] == cost[i - 1][j - 1] + sub:
            pairs.append((i - 1, j - 1))
            i, j = i - 1, j - 1
        elif cost[i][j] == cost[i - 1][j] + 1:
            i -= 1
        else:
            j -= 1
    return pairs[::-1]


def _monotone(score: list[list[int]], rows: int, cols: int) -> list[int] | None:
    """Assign rows 0..rows-1 to columns 0..cols-1, non-decreasing, every column used, maximising score."""
    neg = -(10**9)
    dp = [[neg] * (cols + 1) for _ in range(rows + 1)]
    take_new = [[False] * (cols + 1) for _ in range(rows + 1)]
    dp[0][0] = 0
    for r in range(1, rows + 1):
        for c in range(1, min(r, cols) + 1):
            gain = score[r - 1][c - 1]
            stay = dp[r - 1][c] if c <= r - 1 else neg
            new = dp[r - 1][c - 1]
            if new >= stay:
                dp[r][c], take_new[r][c] = new + gain, True
            else:
                dp[r][c] = stay + gain
    if dp[rows][cols] <= neg // 2:
        return None
    out = [0] * rows
    c = cols
    for r in range(rows, 0, -1):
        out[r - 1] = c - 1
        if take_new[r][c]:
            c -= 1
    return out


def map_ayah(words: int, phoneme_words: list[str], our_items: list, units: set[str]) -> list[Group] | None:
    """Groups (first word, last word, units) of one ayah, or None if no consistent mapping exists."""
    ours, theirs = _our_consonants(our_items), _their_consonants(phoneme_words)
    if not ours and len(phoneme_words) >= words:
        # only spelled-out letters (2:1, 19:1 ...): the reference phonemizer skips them, so spread the
        # groups over the words in proportion
        g = len(phoneme_words)
        return [
            (c * words // g + 1, c * words // g + 1, tuple(tokenize(text, units)))
            for c, text in enumerate(phoneme_words)
        ]
    if not ours or not theirs:
        return None
    votes: dict[tuple[int, int], int] = defaultdict(
        int
    )  # (word 1-based, group 0-based) -> aligned consonants
    for i, j in _align(ours, theirs):
        votes[(ours[i][1], theirs[j][1])] += 1
    g = len(phoneme_words)
    tokens = [tuple(tokenize(text, units)) for text in phoneme_words]
    if g <= words:  # groups cover one or more consecutive words
        score = [[votes[(w + 1, c)] for c in range(g)] for w in range(words)]
        owner = _monotone(score, words, g)
        if owner is None:
            return None
        spans: dict[int, list[int]] = defaultdict(list)
        for w, c in enumerate(owner, 1):
            spans[c].append(w)
        return [(min(spans[c]), max(spans[c]), tokens[c]) for c in range(g)]
    # more groups than words (spelled-out letters): each word owns several consecutive groups
    score = [[votes[(w + 1, c)] for w in range(words)] for c in range(g)]
    owner = _monotone(score, g, words)
    if owner is None:
        return None
    return [(owner[c] + 1, owner[c] + 1, tokens[c]) for c in range(g)]


def build(quran: Quran, model_dir: Path) -> tuple[dict[tuple[int, int], list[Group]], list[tuple[int, int]]]:
    units = {u for u in json.loads((model_dir / "phoneme_units.json").read_text("utf-8")) if u != "<blank>"}
    ordered = json.loads((model_dir / "ordered_quran_phonemes.json").read_text("utf-8"))
    ayahs = [a for s in quran.surahs for a in s.ayahs]
    items_of: dict[tuple[int, int], list] = defaultdict(list)
    for item in phonemize_range(ayahs).items:
        items_of[(item.surah, item.ayah)].append(item)
    out: dict[tuple[int, int], list[Group]] = {}
    failed: list[tuple[int, int]] = []
    for ayah in ayahs:
        key = (ayah.surah, ayah.ayah)
        groups = map_ayah(
            len(ayah.words), ordered[f"{key[0]}:{key[1]}"]["aya_phonemes_list"], items_of[key], units
        )
        if groups is None:
            failed.append(key)
        else:
            out[key] = groups
    return out, failed


def write_tsv(path: Path, table: dict[tuple[int, int], list[Group]]) -> None:
    lines = []
    for (surah, ayah), groups in sorted(table.items()):
        for first, last, units in groups:
            lines.append(f"{surah}:{ayah}\t{first}-{last}\t{' '.join(units)}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model-dir", type=Path, default=ROOT / "models" / "zipformer_p-quran")
    ap.add_argument("--out", type=Path, default=ROOT / "models" / "zipformer_p-quran" / "expected_units.tsv")
    args = ap.parse_args()
    table, failed = build(load_quran(ROOT / "data" / "quran"), args.model_dir)
    write_tsv(args.out, table)
    kinds = Counter("merged" if g[1] > g[0] else "single" for groups in table.values() for g in groups)
    print(f"{len(table)} ayahs mapped, {len(failed)} without a mapping {failed[:10]}; groups: {dict(kinds)}")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
