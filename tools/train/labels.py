"""CTC training labels from the reference phonemizer.

An EveryAyah clip is one ayah recited from its first word and stopped at its last: the reading
is the concatenation of each word's `cont` reading (first word starts, last word stops).
Ayahs with a word the phonemizer skips (the disconnected letters of 2:1, 19:1 ...) get no label
and are left out of training. The basmala is not part of the label: those clips do not contain it
(except 1:1, whose words are the basmala).
"""

from __future__ import annotations

from tools.phonemizer.inventory import VOCAB
from tools.prototype.phoneme_data import WordKey, WordPhonemes
from tools.quran_text.model import Quran

TOKEN_ID = {symbol: i for i, symbol in enumerate(VOCAB)}  # <blank> = 0


def ayah_symbols(quran: Quran, words: dict[WordKey, WordPhonemes], surah: int, ayah: int) -> list[str] | None:
    """Expected phoneme symbols of the whole ayah, or None if any word has no phonemes."""
    out: list[str] = []
    for word in quran.ayah(surah, ayah).words:
        wp = words.get((surah, ayah, word.index))
        if wp is None or not wp.cont:
            return None
        out += wp.cont
    return out


def ayah_labels(quran: Quran, words: dict[WordKey, WordPhonemes], surah: int, ayah: int) -> list[int] | None:
    symbols = ayah_symbols(quran, words, surah, ayah)
    return None if symbols is None else [TOKEN_ID[s] for s in symbols]
