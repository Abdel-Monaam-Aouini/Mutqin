"""Task 1.2: split an Uthmani word into letter units (base character + combining marks).

Pure, lossless parsing: no text is changed, nothing is interpreted. Rule meaning (what a given
mark implies for pronunciation) belongs to later tasks and to docs/TAJWEED_SPEC.md.

Allowed characters are exactly the ones present in the verified Tanzil Uthmani text (checked by
tests against the real data). Any other character raises, so a changed source is noticed.
"""

from __future__ import annotations

import unicodedata
from dataclasses import dataclass

# Base characters (each starts a new unit).
LETTERS = frozenset(
    [0x0621, 0x0671]  # hamza, alef wasla
    + list(range(0x0623, 0x063B))  # alef/waw/yeh with hamza ... ghain (includes alef, teh marbuta)
    + list(range(0x0641, 0x064B))  # feh ... yeh (includes alef maksura)
)
TATWEEL = 0x0640  # carrier for marks
SMALL_LETTERS = frozenset([0x06E5, 0x06E6])  # small waw, small yeh (non-combining modifiers)
BASES = LETTERS | {TATWEEL} | SMALL_LETTERS

# Combining marks (attach to the preceding base).
MARKS = frozenset(
    list(range(0x064B, 0x0655))  # tanween, harakat, shaddah, sukun, maddah, hamza above
    + [0x0670]  # superscript alef
    + [0x06DC, 0x06DF, 0x06E0, 0x06E2, 0x06E3, 0x06E7, 0x06E8, 0x06EA, 0x06EB, 0x06EC, 0x06ED]
)

FATHATAN, DAMMATAN, KASRATAN = 0x064B, 0x064C, 0x064D
FATHA, DAMMA, KASRA = 0x064E, 0x064F, 0x0650
SHADDA, SUKUN, MADDAH_ABOVE, HAMZA_ABOVE = 0x0651, 0x0652, 0x0653, 0x0654
SUPERSCRIPT_ALEF = 0x0670


class UthmaniParseError(ValueError):
    pass


@dataclass(frozen=True)
class LetterUnit:
    char: str  # one base code point
    marks: tuple[str, ...]  # combining marks in source order, one code point each
    start: int  # [start, end) code point offsets inside the word text
    end: int

    @property
    def text(self) -> str:
        return self.char + "".join(self.marks)

    def has(self, codepoint: int) -> bool:
        return chr(codepoint) in self.marks


def parse_word(text: str) -> tuple[LetterUnit, ...]:
    """Split a word into units. Joining `unit.text` over the result gives `text` back exactly."""
    units: list[LetterUnit] = []
    char = ""
    marks: list[str] = []
    start = 0
    for i, c in enumerate(text):
        cp = ord(c)
        if cp in BASES:
            if char:
                units.append(LetterUnit(char, tuple(marks), start, i))
            char, marks, start = c, [], i
        elif cp in MARKS:
            if not char:
                raise UthmaniParseError(f"mark U+{cp:04X} at offset {i} has no base character")
            marks.append(c)
        else:
            name = unicodedata.name(c, "?")
            raise UthmaniParseError(f"unexpected character U+{cp:04X} {name} at offset {i}")
    if char:
        units.append(LetterUnit(char, tuple(marks), start, len(text)))
    return tuple(units)
