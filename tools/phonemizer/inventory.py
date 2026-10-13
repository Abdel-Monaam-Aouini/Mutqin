"""Phoneme inventory and output record of the reference phonemizer (docs/PHONEME_INVENTORY.md).

Approved design (2026-10-13):
- 27 consonants + hamza (28 consonant-class phonemes), each with a stable ASCII symbol, plus the
  short vowels a, i, u. Alif is not a consonant: it is a long /a/ or a hamza seat (task 1.3).
- Long vowels are a short-vowel symbol with a longer duration (`dur`), not separate symbols.
- Shaddah = the consonant emitted twice. Sukun = no vowel emitted.
- Ghunnah, qalqalah, ikhfa, idgham, iqlab... are rule tags on a phoneme, not extra tokens.
- Heavy/light (tafkhim/tarqiq) is merged for now: no separate symbols for ra / lam.

Letters are stored as Unicode code points with their official Unicode names, never as typed
Arabic text; the tests verify them against the Python Unicode database.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class Kind(str, Enum):
    CONSONANT = "consonant"
    HAMZA = "hamza"
    VOWEL = "vowel"


@dataclass(frozen=True)
class PhonemeDef:
    symbol: str  # stable ASCII symbol = acoustic model token
    kind: Kind
    codepoint: int | None  # base Arabic letter (None for vowels)
    unicode_name: str | None  # official Unicode name of that letter, e.g. "ARABIC LETTER BEH"


def _c(symbol: str, codepoint: int, name: str) -> PhonemeDef:
    return PhonemeDef(symbol, Kind.CONSONANT, codepoint, f"ARABIC LETTER {name}")


# Order is part of the contract: token ids derive from it. Hamza first, then alphabetical order.
PHONEMES: tuple[PhonemeDef, ...] = (
    PhonemeDef("hz", Kind.HAMZA, 0x0621, "ARABIC LETTER HAMZA"),
    _c("b", 0x0628, "BEH"),
    _c("t", 0x062A, "TEH"),
    _c("th", 0x062B, "THEH"),
    _c("j", 0x062C, "JEEM"),
    _c("H", 0x062D, "HAH"),
    _c("kh", 0x062E, "KHAH"),
    _c("d", 0x062F, "DAL"),
    _c("dh", 0x0630, "THAL"),
    _c("r", 0x0631, "REH"),
    _c("z", 0x0632, "ZAIN"),
    _c("s", 0x0633, "SEEN"),
    _c("sh", 0x0634, "SHEEN"),
    _c("S", 0x0635, "SAD"),
    _c("D", 0x0636, "DAD"),
    _c("T", 0x0637, "TAH"),
    _c("Z", 0x0638, "ZAH"),
    _c("E", 0x0639, "AIN"),
    _c("gh", 0x063A, "GHAIN"),
    _c("f", 0x0641, "FEH"),
    _c("q", 0x0642, "QAF"),
    _c("k", 0x0643, "KAF"),
    _c("l", 0x0644, "LAM"),
    _c("m", 0x0645, "MEEM"),
    _c("n", 0x0646, "NOON"),
    _c("h", 0x0647, "HEH"),
    _c("w", 0x0648, "WAW"),
    _c("y", 0x064A, "YEH"),
    PhonemeDef("a", Kind.VOWEL, None, None),
    PhonemeDef("i", Kind.VOWEL, None, None),
    PhonemeDef("u", Kind.VOWEL, None, None),
)

SYMBOLS: tuple[str, ...] = tuple(p.symbol for p in PHONEMES)
BY_SYMBOL: dict[str, PhonemeDef] = {p.symbol: p for p in PHONEMES}
BY_CODEPOINT: dict[int, PhonemeDef] = {p.codepoint: p for p in PHONEMES if p.codepoint is not None}

BLANK = "<blank>"
# CTC vocabulary: blank is id 0, then the symbols in the order above.
VOCAB: tuple[str, ...] = (BLANK,) + SYMBOLS
TOKEN_ID: dict[str, int] = {token: i for i, token in enumerate(VOCAB)}


class Variant(str, Enum):
    """Where a phoneme applies (spec section 1, plus START_ONLY for hamzat al-wasl)."""

    BOTH = "both"
    CONTINUE_ONLY = "continue_only"  # only when reading on past the end of this word
    WAQF_ONLY = "waqf_only"  # only when stopping at the end of this word
    # Only when the word starts a recitation (first word of the range) or follows a stop.
    # Extension of the spec's three variants, needed for hamzat al-wasl (spec 3.9).
    START_ONLY = "start_only"
    # Only when the previous word boundary is a continuation (e.g. the doubled consonant of an
    # idgham). Counterpart of START_ONLY; extension of the spec's variants, to be confirmed.
    AFTER_CONTINUE = "after_continue"


@dataclass(frozen=True)
class ExpectedPhoneme:
    """One item of the phonemizer output (docs/TAJWEED_SPEC.md section 1)."""

    ph: str  # a symbol from SYMBOLS
    word: int  # 1-based index of the source word in the ayah
    char_span: tuple[int, int]  # [start, end) code point offsets inside the Uthmani word text
    rules: tuple[str, ...] = ()  # rule tags, defined with each rule in spec section 3
    dur: tuple[int, ...] = ()  # allowed duration in counts, e.g. (2,), (4, 5), (2, 4, 6)
    variant: Variant = Variant.BOTH
    surah: int = 0  # 0 when the phonemizer was run on a bare Ayah without location
    ayah: int = 0

    def __post_init__(self) -> None:
        if self.ph not in BY_SYMBOL:
            raise ValueError(f"unknown phoneme symbol: {self.ph!r}")
        if self.word < 1:
            raise ValueError("word index is 1-based")
        start, end = self.char_span
        if not 0 <= start <= end:
            raise ValueError(f"bad char_span {self.char_span}")
        if any(d <= 0 for d in self.dur):
            raise ValueError("durations are positive counts")
