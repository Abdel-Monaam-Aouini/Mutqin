"""Data model for the verified Quran text: surah -> ayah -> word, with stable IDs.

Word IDs have the form "surah:ayah:word", all 1-based. Words come only from the Uthmani file.
No text is ever stored here as a literal; everything is loaded from data/quran/.
"""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class Word:
    surah: int
    ayah: int
    index: int  # 1-based position inside the ayah (basmala prefix not counted)
    text: str  # exactly the source token
    marks: tuple[str, ...] = ()  # standalone waqf / sajdah signs that follow this word

    @property
    def id(self) -> str:
        return f"{self.surah}:{self.ayah}:{self.index}"


@dataclass(frozen=True)
class Ayah:
    surah: int
    ayah: int
    words: tuple[Word, ...]
    simple_text: str  # Tanzil "simple-clean" ayah text, for matching only (own word split)

    @property
    def id(self) -> str:
        return f"{self.surah}:{self.ayah}"


@dataclass(frozen=True)
class Surah:
    number: int
    # Tokens of the basmala that Tanzil prefixes to ayah 1 (empty for surah 1 and surah 9;
    # for surah 1 the basmala is ayah 1 itself and its words are normal words of 1:1).
    basmala: tuple[str, ...]
    ayahs: tuple[Ayah, ...]


@dataclass(frozen=True)
class Quran:
    surahs: tuple[Surah, ...]
    _by_id: dict[tuple[int, int], Ayah] = field(repr=False, compare=False, default_factory=dict)

    @property
    def ayah_count(self) -> int:
        return sum(len(s.ayahs) for s in self.surahs)

    def ayah(self, surah: int, ayah: int) -> Ayah:
        return self._by_id[(surah, ayah)]

    def word(self, word_id: str) -> Word:
        s, a, w = (int(x) for x in word_id.split(":"))
        return self.ayah(s, a).words[w - 1]
