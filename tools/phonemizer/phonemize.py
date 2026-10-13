"""Reference phonemizer: Uthmani ayah -> expected phoneme sequence (docs/PHONEME_INVENTORY.md).

PROVISIONAL: the letter / sign mappings implement docs/DRAFT_ORTHOGRAPHY_TABLES.md, which the
owner asked to be implemented for testing (2026-10-13) but which no teacher has verified yet.
Every case the tables do not cover is reported as a Diagnostic instead of being guessed.

This is the "continuing" (wasl) reading; stop (waqf) variants are added in task 1.8.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from tools.phonemizer import uthmani as u
from tools.phonemizer.inventory import BY_CODEPOINT, ExpectedPhoneme, Variant
from tools.quran_text.model import Ayah, Word

ALEF, ALEF_WASLA, ALEF_MAKSURA, TEH_MARBUTA = 0x0627, 0x0671, 0x0649, 0x0629
WAW, YEH, TATWEEL = 0x0648, 0x064A, 0x0640
SMALL_WAW, SMALL_YEH = 0x06E5, 0x06E6
SMALL_HIGH_YEH = 0x06E7
ROUNDED_ZERO, RECTANGULAR_ZERO = 0x06DF, 0x06E0
HAMZA_SEATS = frozenset([0x0623, 0x0624, 0x0625, 0x0626])

SHORT_VOWELS = {u.FATHA: "a", u.DAMMA: "u", u.KASRA: "i"}
TANWEEN = {u.FATHATAN: "a", u.DAMMATAN: "u", u.KASRATAN: "i"}
# Signs kept on the host item for later tasks (iqlab, special cases by location).
SIGN_MARKS = frozenset([0x06E2, 0x06ED, 0x06DC, 0x06E3, 0x06E8, 0x06EA, 0x06EB, 0x06EC])


@dataclass
class Diagnostic:
    word_id: str
    kind: str  # short machine-readable reason
    offset: int  # code point offset inside the word


@dataclass
class Item:
    """Working phoneme, converted to ExpectedPhoneme at the end."""

    ph: str
    word: int
    span: tuple[int, int]
    role: str  # "cons" | "vowel" | "tanween_n"
    dur: tuple[int, ...] = ()
    rules: list[str] = field(default_factory=list)
    variant: Variant = Variant.BOTH
    maddah: bool = False  # a maddah sign was written on/after this vowel (cross-check for 1.7)
    signs: set[int] = field(default_factory=set)
    letter: int = 0  # code point of the host letter


@dataclass
class AyahResult:
    items: list[Item]
    diagnostics: list[Diagnostic]
    # Positions where later tasks add variants: (kind, word index, item index)
    hooks: list[tuple[str, int, int]]

    @property
    def phonemes(self) -> list[ExpectedPhoneme]:
        return [
            ExpectedPhoneme(i.ph, i.word, i.span, tuple(i.rules), i.dur, i.variant)
            for i in self.items
        ]


def is_disconnected_letters(units: tuple[u.LetterUnit, ...]) -> bool:
    """Surah-opening letters are pronounced by name, not by orthography. Structural test: no
    vowel sign, tanween, sukun or shaddah on any unit (ordinary words always have one)."""
    voweled = set(SHORT_VOWELS) | set(TANWEEN) | {u.SUKUN, u.SHADDA}
    return bool(units) and not any(ord(m) in voweled for x in units for m in x.marks)


class _Builder:
    def __init__(self, ayah: Ayah) -> None:
        self.ayah = ayah
        self.items: list[Item] = []
        self.diags: list[Diagnostic] = []
        self.hooks: list[tuple[str, int, int]] = []
        self.word: Word | None = None

    # -- helpers ---------------------------------------------------------------------------
    def diag(self, kind: str, offset: int) -> None:
        assert self.word is not None
        self.diags.append(Diagnostic(self.word.id, kind, offset))

    def add(self, ph: str, span: tuple[int, int], role: str, dur: tuple[int, ...] = (),
            letter: int = 0) -> Item:
        assert self.word is not None
        item = Item(ph, self.word.index, span, role, dur, letter=letter)
        self.items.append(item)
        return item

    def last(self) -> Item | None:
        if self.items and self.word is not None and self.items[-1].word == self.word.index:
            return self.items[-1]
        return None

    def lengthen(self, vowel: str, end: int) -> bool:
        """Make the previous short vowel long (dur 2). Returns False if there is none."""
        last = self.last()
        if last and last.role == "vowel" and last.ph == vowel and last.dur == (1,):
            last.dur = (2,)
            last.span = (last.span[0], end)
            return True
        return False

    def sign_on_last_vowel(self, unit: u.LetterUnit, mark_cp: int) -> None:
        last = self.last()
        if last is None:
            self.diag(f"mark_{mark_cp:04X}_without_host", unit.start)
        elif mark_cp == u.MADDAH_ABOVE:
            last.maddah = True
        else:
            last.signs.add(mark_cp)

    # -- marks on a host letter --------------------------------------------------------------
    def marks(self, unit: u.LetterUnit, host: Item | None, host_is_tatweel: bool = False) -> None:
        order = list(enumerate(unit.marks))
        if host_is_tatweel:  # hamza first, then its vowel
            order.sort(key=lambda km: ord(km[1]) != u.HAMZA_ABOVE)
        for k, mark in order:
            cp = ord(mark)
            span = (unit.start + 1 + k, unit.start + 2 + k)
            if cp in SHORT_VOWELS:
                self.add(SHORT_VOWELS[cp], span, "vowel", (1,))
            elif cp in TANWEEN:
                self.add(TANWEEN[cp], span, "vowel", (1,))
                self.add("n", span, "tanween_n")
            elif cp == u.SUPERSCRIPT_ALEF:
                if not self.lengthen("a", span[1]):
                    self.add("a", span, "vowel", (2,))
                    self.diag("long_a_without_fatha", span[0])
            elif cp == u.HAMZA_ABOVE:
                self.add("hz", span, "cons", letter=u.HAMZA_ABOVE)
            elif cp == u.MADDAH_ABOVE:
                self.sign_on_last_vowel(unit, cp)
            elif cp in SIGN_MARKS:
                target = host or self.last()
                if target is not None:
                    target.signs.add(cp)
                else:
                    self.diag(f"mark_{cp:04X}_without_host", span[0])
            elif cp in (u.SHADDA, u.SUKUN, ROUNDED_ZERO, RECTANGULAR_ZERO):
                pass  # shaddah handled at emission; sukun = nothing; zeros handled per letter
            elif cp == SMALL_HIGH_YEH:
                if any(ord(m) in SHORT_VOWELS or ord(m) in TANWEEN for m in unit.marks):
                    self.add("y", span, "cons", letter=SMALL_HIGH_YEH)  # yeh with its own vowel
                elif not self.lengthen("i", span[1]):
                    self.diag("small_high_yeh_without_kasra", span[0])
            else:
                self.diag(f"unhandled_mark_{cp:04X}", span[0])

    # -- letters -----------------------------------------------------------------------------
    def consonant(self, unit: u.LetterUnit, ph: str, tags: tuple[str, ...] = ()) -> None:
        times = 2 if unit.has(u.SHADDA) else 1
        host = None
        for _ in range(times):
            host = self.add(ph, (unit.start, unit.start + 1), "cons", letter=ord(unit.char))
            host.rules.extend(tags)
        self.marks(unit, host)

    def alef(self, unit: u.LetterUnit) -> None:
        if unit.has(ROUNDED_ZERO):
            return  # written, not pronounced
        if unit.has(RECTANGULAR_ZERO):  # DRAFT: silent when continuing, pronounced at waqf
            self.hooks.append(("alef_waqf", self.word.index, len(self.items)))
            return
        last = self.last()
        if last and last.role == "vowel" and last.ph == "a" and last.dur == (1,):
            self.lengthen("a", unit.end)
        elif last and last.role == "tanween_n" and len(self.items) >= 2 and self.items[-2].ph == "a":
            self.hooks.append(("iwad", self.word.index, len(self.items)))  # tanween alef
        else:
            self.diag("alef_without_fatha", unit.start)
        for k, mark in enumerate(unit.marks):
            if ord(mark) in (u.MADDAH_ABOVE, *SIGN_MARKS):
                self.sign_on_last_vowel(unit, ord(mark))

    def wasla(self, unit: u.LetterUnit) -> None:
        # Silent when continuing. Starting pronunciation is added in task 1.4.
        self.hooks.append(("wasla", self.word.index, len(self.items)))

    def tatweel(self, unit: u.LetterUnit) -> None:
        self.marks(unit, None, host_is_tatweel=True)

    def small(self, unit: u.LetterUnit, vowel: str) -> None:
        if any(ord(m) in SHORT_VOWELS or ord(m) in TANWEEN for m in unit.marks):
            self.consonant(unit, "w" if vowel == "u" else "y")  # small letter with its own vowel
            return
        if not self.lengthen(vowel, unit.end):
            self.diag(f"small_letter_without_{vowel}", unit.start)
        for mark in unit.marks:
            if ord(mark) == u.MADDAH_ABOVE:
                self.sign_on_last_vowel(unit, u.MADDAH_ABOVE)
            else:
                self.diag(f"unhandled_mark_{ord(mark):04X}_on_small_letter", unit.start)

    def weak(self, unit: u.LetterUnit) -> None:
        """waw, yeh, alef maksura: consonant when carrying its own vowel / sukun / shaddah,
        otherwise a madd letter or a silent carrier (DRAFT tables A)."""
        cp = ord(unit.char)
        if unit.has(ROUNDED_ZERO):
            return
        own = any(
            ord(m) in SHORT_VOWELS or ord(m) in TANWEEN or ord(m) in (u.SHADDA, u.SUKUN)
            for m in unit.marks
        )
        if own:
            self.consonant(unit, "y" if cp in (YEH, ALEF_MAKSURA) else "w")  # yeh-shaped: y
            return
        if unit.has(u.SUPERSCRIPT_ALEF):  # carrier for the superscript alef
            self.marks(unit, None)
            return
        last = self.last()
        prev_vowel = last.ph if last and last.role == "vowel" else None
        if (cp == ALEF_MAKSURA and last and last.role == "tanween_n"
                and len(self.items) >= 2 and self.items[-2].ph == "a"):
            self.hooks.append(("iwad", self.word.index, len(self.items)))  # tanween alef
        elif cp == WAW and prev_vowel == "a" and self.next_unit_has(unit, ROUNDED_ZERO):
            # bare waw after fatha before a silent alef: consonant w, sukun implied by encoding
            self.add("w", (unit.start, unit.start + 1), "cons", letter=cp).rules.append("implicit_sukun")
        elif cp == WAW and prev_vowel == "u":
            self.lengthen("u", unit.end)
        elif cp == YEH and prev_vowel == "i":
            self.lengthen("i", unit.end)
        elif cp == ALEF_MAKSURA and prev_vowel in ("a", "i"):
            self.lengthen(prev_vowel, unit.end)
        else:
            self.diag(f"bare_{'waw' if cp == WAW else 'yeh' if cp == YEH else 'maksura'}"
                      f"_after_{prev_vowel or 'nothing'}", unit.start)
        self.marks(unit, None)

    def next_unit_has(self, unit: u.LetterUnit, mark_cp: int) -> bool:
        assert self.word is not None
        units = u.parse_word(self.word.text)
        nxt = [x for x in units if x.start == unit.end]
        return bool(nxt) and nxt[0].has(mark_cp)

    def unit(self, unit: u.LetterUnit) -> None:
        cp = ord(unit.char)
        if cp == ALEF:
            self.alef(unit)
        elif cp == ALEF_WASLA:
            self.wasla(unit)
        elif cp == TATWEEL:
            self.tatweel(unit)
        elif cp == SMALL_WAW:
            self.small(unit, "u")
        elif cp == SMALL_YEH:
            self.small(unit, "i")
        elif cp in (WAW, YEH, ALEF_MAKSURA):
            self.weak(unit)
        elif cp == TEH_MARBUTA:
            self.consonant(unit, "t", ("teh_marbuta",))
        elif cp in HAMZA_SEATS:
            self.consonant(unit, "hz")
        elif cp in BY_CODEPOINT:
            self.consonant(unit, BY_CODEPOINT[cp].symbol)
        else:
            self.diag(f"unhandled_letter_{cp:04X}", unit.start)

    def run(self) -> AyahResult:
        for word in self.ayah.words:
            self.word = word
            units = u.parse_word(word.text)
            if is_disconnected_letters(units):
                self.diag("disconnected_letters", 0)  # letter names: task 1.9 data, not orthography
                continue
            for unit in units:
                self.unit(unit)
        return AyahResult(self.items, self.diags, self.hooks)


def phonemize_ayah(ayah: Ayah) -> AyahResult:
    return _Builder(ayah).run()
