"""Per-word expected phonemes from the reference phonemizer, for the Phase 2 prototype.

Two readings per word, both taken from one whole-Quran phonemizer run:
  cont: the reading when the reciter goes on to the next word (stopping only at ayah ends),
  stop: the reading when the reciter stops on the word.
Consecutive identical phonemes inside a word (a shaddah) are collapsed to one token: a CTC model
cannot emit the same label twice without a blank, so both sides are compared collapsed.
"""

from __future__ import annotations

from dataclasses import dataclass

from tools.phonemizer.phonemize import phonemize_range, word_key
from tools.quran_text.model import Quran

WordKey = tuple[int, int, int]


@dataclass(frozen=True)
class WordPhonemes:
    """Expected phonemes of one word. `contexts` holds the reading for each (pause before, pause
    after) combination, indexed by 2 * before + after. `cont` is the baseline the aligner walks: the
    reading when the reciter stops only at ayah ends."""

    key: WordKey
    cont: tuple[str, ...]
    contexts: tuple[tuple[str, ...], tuple[str, ...], tuple[str, ...], tuple[str, ...]]

    def tokens_for(self, before: bool, after: bool) -> tuple[str, ...]:
        return self.contexts[2 * int(before) + int(after)]

    def readings(self, pause_before: bool, pause_after: bool) -> list[tuple[str, ...]]:
        """The readings possible when the audio shows (or does not show) a pause on each side."""
        return [
            self.tokens_for(b, a)
            for b in (False, True)
            for a in (False, True)
            if (not b or pause_before) and (not a or pause_after)
        ]


def collapse(tokens: list[str]) -> tuple[str, ...]:
    out: list[str] = []
    for t in tokens:
        if not out or out[-1] != t:
            out.append(t)
    return tuple(out)


def _keep(variant: str, before: bool, after: bool) -> bool:
    return (
        variant == "both"
        or (variant == "continue_only" and not after)
        or (variant == "waqf_only" and after)
        or (variant == "start_only" and before)
        or (variant == "after_continue" and not before)
    )


def build_word_phonemes(quran: Quran) -> dict[WordKey, WordPhonemes]:
    ayahs = [a for s in quran.surahs for a in s.ayahs]
    result = phonemize_range(ayahs)
    ayah_len = {(a.surah, a.ayah): len(a.words) for a in ayahs}
    items_of: dict[WordKey, list] = {}
    for item in result.items:
        items_of.setdefault(word_key(item), []).append(item)
    out: dict[WordKey, WordPhonemes] = {}
    for key, items in items_of.items():
        combos = [
            collapse([i.ph for i in items if _keep(i.variant.value, b, a)])
            for b in (False, True)
            for a in (False, True)
        ]
        first, last = key[2] == 1, key[2] == ayah_len[(key[0], key[1])]
        cont = combos[2 * int(first) + int(last)]
        out[key] = WordPhonemes(key, cont, tuple(combos))  # type: ignore[arg-type]
    return out
