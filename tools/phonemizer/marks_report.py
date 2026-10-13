"""Context statistics for Quranic signs in the verified Tanzil Uthmani text.

Evidence for docs/DRAFT_ORTHOGRAPHY_TABLES.md: for each sign, where it occurs (host letter,
vowel marks on the host, word position, neighbours). Prints code point names only, never text.
Usage: python3 tools/phonemizer/marks_report.py
"""

from __future__ import annotations

import sys
import unicodedata
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.phonemizer import uthmani as u  # noqa: E402
from tools.quran_text.loader import load_quran  # noqa: E402


def nm(c: str) -> str:
    return unicodedata.name(c).replace("ARABIC ", "").replace("LETTER ", "").replace("SMALL ", "s.")


def marks_desc(unit: u.LetterUnit) -> str:
    return "+".join(nm(m) for m in unit.marks) or "-"


def top(counter: Counter, n: int = 6) -> str:
    total = sum(counter.values())
    return ", ".join(f"{k} {v}" for k, v in counter.most_common(n)) + (f" (total {total})")


def main() -> None:
    quran = load_quran()
    # Signs hosted as combining marks: report host letter, host's other marks, position, next letter
    targets_marks = [
        0x06DF,
        0x06E0,
        0x06E2,
        0x06ED,
        0x06E7,
        0x06E8,
        0x06E3,
        0x06DC,
        0x06EA,
        0x06EB,
        0x06EC,
        0x0653,
        0x0654,
        0x0670,
    ]
    # Signs that are base units themselves
    targets_bases = [0x06E5, 0x06E6, 0x0640, 0x0671, 0x0627, 0x0649, 0x0629]

    ctx_marks = {
        t: {"host": Counter(), "other": Counter(), "final": Counter(), "next": Counter(), "ids": []}
        for t in targets_marks
    }
    ctx_bases = {
        t: {
            "prev": Counter(),
            "final": Counter(),
            "own": Counter(),
            "next": Counter(),
            "first": Counter(),
            "ids": [],
        }
        for t in targets_bases
    }

    for surah in quran.surahs:
        for ayah in surah.ayahs:
            words = ayah.words
            for wi, word in enumerate(words):
                units = u.parse_word(word.text)
                nxt_word = u.parse_word(words[wi + 1].text) if wi + 1 < len(words) else None
                for i, unit in enumerate(units):
                    is_final = i == len(units) - 1
                    nxt = units[i + 1] if not is_final else (nxt_word[0] if nxt_word else None)
                    for m in unit.marks:
                        cp = ord(m)
                        if cp in ctx_marks:
                            c = ctx_marks[cp]
                            c["host"][nm(unit.char)] += 1
                            c["other"][
                                marks_desc(
                                    u.LetterUnit(unit.char, tuple(x for x in unit.marks if x != m), 0, 0)
                                )
                            ] += 1
                            c["final"]["word-final" if is_final else "inside"] += 1
                            c["next"][nm(nxt.char) if nxt else "end-of-ayah"] += 1
                            if len(c["ids"]) < 6:
                                c["ids"].append(word.id)
                    cp = ord(unit.char)
                    if cp in ctx_bases:
                        c = ctx_bases[cp]
                        prev = units[i - 1] if i else None
                        c["prev"][("start" if prev is None else nm(prev.char) + ":" + marks_desc(prev))] += 1
                        c["final"]["word-final" if is_final else "inside"] += 1
                        c["first"]["word-initial" if i == 0 else "not-initial"] += 1
                        c["own"][marks_desc(unit)] += 1
                        c["next"][nm(nxt.char) if nxt else "end-of-ayah"] += 1
                        if len(c["ids"]) < 4:
                            c["ids"].append(word.id)

    for t, c in ctx_marks.items():
        print(f"\n== U+{t:04X} {unicodedata.name(chr(t))} (combining)")
        print(" host letter  :", top(c["host"]))
        print(" host's others:", top(c["other"]))
        print(" position     :", top(c["final"]))
        print(" next letter  :", top(c["next"]))
        print(" first ids    :", ", ".join(c["ids"]))
    for t, c in ctx_bases.items():
        print(f"\n== U+{t:04X} {unicodedata.name(chr(t))} (base unit)")
        print(" previous unit:", top(c["prev"], 8))
        print(" position     :", top(c["final"]), "|", top(c["first"]))
        print(" own marks    :", top(c["own"]))
        print(" next letter  :", top(c["next"], 8))
        print(" first ids    :", ", ".join(c["ids"]))


if __name__ == "__main__":
    main()
