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
LAM_CP = 0x0644
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
    surah: int = 0
    ayah: int = 0
    host_marks: frozenset[int] = frozenset()  # code points of the marks on the host letter
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
            ExpectedPhoneme(i.ph, i.word, i.span, tuple(i.rules), i.dur, i.variant, i.surah, i.ayah)
            for i in self.items
        ]


def is_disconnected_letters(units: tuple[u.LetterUnit, ...]) -> bool:
    """Surah-opening letters are pronounced by name, not by orthography. Structural test: no
    vowel sign, tanween, sukun or shaddah on any unit (ordinary words always have one)."""
    voweled = set(SHORT_VOWELS) | set(TANWEEN) | {u.SUKUN, u.SHADDA}
    return bool(units) and not any(ord(m) in voweled for x in units for m in x.marks)


class _Builder:
    def __init__(self, ayahs: tuple[Ayah, ...]) -> None:
        self.ayahs = ayahs
        self.cur: Ayah = ayahs[0]
        self.items: list[Item] = []
        self.diags: list[Diagnostic] = []
        self.hooks: list[tuple[str, int, int]] = []
        self.word: Word | None = None
        self.units: tuple[u.LetterUnit, ...] = ()
        self.pos = 0  # index of the current unit in self.units
        self.pending_tags: list[str] = []  # applied to the next consonant (assimilated lam)

    # -- helpers ---------------------------------------------------------------------------
    def diag(self, kind: str, offset: int) -> None:
        assert self.word is not None
        self.diags.append(Diagnostic(self.word.id, kind, offset))

    def add(self, ph: str, span: tuple[int, int], role: str, dur: tuple[int, ...] = (),
            letter: int = 0) -> Item:
        assert self.word is not None
        item = Item(ph, self.word.index, span, role, dur, letter=letter,
                    surah=self.word.surah, ayah=self.word.ayah)
        self.items.append(item)
        return item

    def last(self) -> Item | None:
        """Previous item if it belongs to the word being processed."""
        if self.items and self.word is not None and word_key(self.items[-1]) == (
                self.word.surah, self.word.ayah, self.word.index):
            return self.items[-1]
        return None

    def lengthen(self, vowel: str, end: int) -> bool:
        """Make the previous short vowel long (dur 2). Returns False if there is none."""
        last = self.last()
        if (last and last.role == "vowel" and last.ph == vowel and last.dur == (1,)
                and last.variant is Variant.BOTH):
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
                self.add("n", span, "tanween_n").variant = Variant.CONTINUE_ONLY  # no n at waqf
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
        tags = tags + tuple(self.pending_tags)
        self.pending_tags = []
        host = None
        for n in range(times):
            host = self.add(ph, (unit.start, unit.start + 1), "cons", letter=ord(unit.char))
            host.rules.extend(tags)
            host.host_marks = frozenset(ord(m) for m in unit.marks)
            if times == 2 and n == 1 and ph in ("n", "m"):  # mushaddad noon / meem (spec 3.3)
                host.rules.append("ghunnah")
                host.dur = (2,)
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
        """Hamzat al-wasl: silent when continuing; when starting, hz + a for the definite article.
        Starting vowels of other words are not in the spec (open question): reported."""
        nxt = self.units[self.pos + 1] if self.pos + 1 < len(self.units) else None
        if nxt is not None and ord(nxt.char) == LAM_CP:
            span = (unit.start, unit.end)
            hz = self.add("hz", span, "cons", letter=ALEF_WASLA)
            a = self.add("a", span, "vowel", (1,))
            for item in (hz, a):
                item.variant = Variant.START_ONLY
                item.rules.append("hamzat_wasl")
        else:
            self.diag("wasla_start_vowel_unknown", unit.start)

    def lam(self, unit: u.LetterUnit) -> None:
        """Lam of the definite article: assimilated (shamsiyyah) or clear (qamariyyah).
        The mushaf encodes assimilation as a bare lam before a letter carrying shaddah."""
        prev = self.units[self.pos - 1] if self.pos else None
        nxt = self.units[self.pos + 1] if self.pos + 1 < len(self.units) else None
        article = prev is not None and ord(prev.char) == ALEF_WASLA
        if not unit.marks and nxt is not None and nxt.has(u.SHADDA):
            self.pending_tags = ["lam_shamsiyyah" if article else "lam_assimilated"]
            return  # not pronounced; the next consonant is doubled by its shaddah
        tags = ("lam_qamariyyah",) if article and unit.has(u.SUKUN) else ()
        self.consonant(unit, "l", tags)

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
        nxt = [x for x in self.units if x.start == unit.end]
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
        elif cp == LAM_CP:
            self.lam(unit)
        elif cp == TEH_MARBUTA:
            self.consonant(unit, "t", ("teh_marbuta",))
        elif cp in HAMZA_SEATS:
            self.consonant(unit, "hz")
        elif cp in BY_CODEPOINT:
            self.consonant(unit, BY_CODEPOINT[cp].symbol)
        else:
            self.diag(f"unhandled_letter_{cp:04X}", unit.start)

    def run(self) -> AyahResult:
        for ayah in self.ayahs:
            self.cur = ayah
            self.run_ayah(ayah)
        apply_noon_rules(self.items, self.diags)
        apply_meem_rules(self.items, self.diags)
        apply_qalqalah(self.items)
        return AyahResult(self.items, self.diags, self.hooks)

    def run_ayah(self, ayah: Ayah) -> None:
        for word in ayah.words:
            self.word = word
            units = u.parse_word(word.text)
            if is_disconnected_letters(units):
                self.diag("disconnected_letters", 0)  # letter names: task 1.9 data, not orthography
                continue
            self.units = units
            for self.pos, unit in enumerate(units):
                self.unit(unit)


def phonemize_range(ayahs: list[Ayah] | tuple[Ayah, ...]) -> AyahResult:
    """Phonemize consecutive ayahs; rules that look at the next word also see the next ayah."""
    return _Builder(tuple(ayahs)).run()


def phonemize_ayah(ayah: Ayah) -> AyahResult:
    """One ayah in isolation: at its end the reciter stops (no next ayah to continue into)."""
    return phonemize_range([ayah])


# -- noon sakinah and tanween (spec 3.1) ----------------------------------------------------------

IZHAR_HALQI = frozenset(["hz", "h", "E", "H", "gh", "kh"])
IDGHAM_GHUNNAH = frozenset(["y", "n", "m", "w"])
IDGHAM_NO_GHUNNAH = frozenset(["l", "r"])
IQLAB = frozenset(["b"])
IKHFA = frozenset(["t", "th", "j", "d", "dh", "z", "s", "sh", "S", "D", "T", "Z", "f", "q", "k"])
NOON_CP = 0x0646


def word_key(item: Item) -> tuple[int, int, int]:
    return (item.surah, item.ayah, item.word)


def _is_noon_sakin(item: Item) -> bool:
    """Noon with no vowel and no shaddah (sukun or bare), or the n of a tanween."""
    if item.role == "tanween_n":
        return True
    return (item.role == "cons" and item.ph == "n" and item.letter == NOON_CP
            and not item.host_marks & (set(SHORT_VOWELS) | {u.SHADDA}))


def _next_consonant(items: list[Item], i: int) -> int | None:
    """Index of the first item after i that is pronounced when continuing, if it is a consonant.
    A surah end is always a stop (the basmala intervenes), so nothing continues across it."""
    j = i + 1
    while j < len(items) and items[j].variant is Variant.START_ONLY:
        j += 1
    if j < len(items) and items[j].role == "cons" and items[j].surah == items[i].surah:
        return j
    return None


def apply_noon_rules(items: list[Item], diags: list[Diagnostic]) -> None:
    """Rewrites items in place. Continuing reading follows the next letter (spec 3.1); where the
    stopping reading differs, the original noon is kept as a waqf_only item."""
    for i in reversed([k for k, it in enumerate(items) if _is_noon_sakin(it)]):
        noon = items[i]
        j = _next_consonant(items, i)
        if j is None:
            continue  # end of the range, or not followed by a consonant: nothing to apply
        nxt = items[j]
        same_word = word_key(noon) == word_key(nxt)
        letter = nxt.ph

        izhar_like = letter in IZHAR_HALQI or (same_word and letter in ("y", "w"))
        if noon.role == "cons" and u.SUKUN in noon.host_marks and not izhar_like:
            # The text writes a sukun where the next-letter rule would hide / merge the noon:
            # a location-specific reading (sakt, spec 3.9), left to the special-cases data (1.9).
            diags.append(Diagnostic(f"{noon.surah}:{noon.ayah}:{noon.word}",
                                    "noon_sukun_before_non_izhar_letter", noon.span[0]))
            continue

        if letter in IZHAR_HALQI:
            noon.rules.append("izhar_halqi")
        elif letter in IDGHAM_GHUNNAH and same_word and letter in ("y", "w"):
            noon.rules.append("izhar_mutlaq")
        elif letter in IDGHAM_GHUNNAH | IDGHAM_NO_GHUNNAH:
            if same_word:
                diags.append(Diagnostic(f"{noon.surah}:{noon.ayah}:{noon.word}",
                                        "noon_idgham_inside_word", noon.span[0]))
                continue
            _idgham(items, i, j, "idgham_ghunnah" if letter in IDGHAM_GHUNNAH
                    else "idgham_no_ghunnah", ghunnah=letter in IDGHAM_GHUNNAH)
        elif letter in IQLAB:
            _hide(items, i, "iqlab", ph="m")
        elif letter in IKHFA:
            _hide(items, i, "ikhfa")
        else:
            diags.append(Diagnostic(f"{noon.surah}:{noon.ayah}:{noon.word}",
                                    f"noon_before_unmapped_{letter}", noon.span[0]))


def _hide(items: list[Item], i: int, rule: str, ph: str | None = None) -> None:
    """Ikhfa / iqlab: hidden sound with ghunnah (2 counts). At a word end this only holds when
    continuing; a stop keeps the plain noon (tanween has no n at a stop, nothing to keep)."""
    noon = items[i]
    boundary = noon.role == "tanween_n" or _word_ends_after(items, i)
    if boundary and noon.role != "tanween_n":
        plain = Item(noon.ph, noon.word, noon.span, noon.role, noon.dur, list(noon.rules),
                     Variant.WAQF_ONLY, surah=noon.surah, ayah=noon.ayah,
                     host_marks=noon.host_marks, letter=noon.letter)
        noon.variant = Variant.CONTINUE_ONLY
        items.insert(i + 1, plain)
    if ph:
        noon.ph = ph
    noon.rules.append(rule)
    noon.dur = (2,)


def _word_ends_after(items: list[Item], i: int) -> bool:
    k = i + 1
    while k < len(items) and items[k].variant is Variant.START_ONLY:
        k += 1
    return k >= len(items) or word_key(items[k]) != word_key(items[i])


def _idgham(items: list[Item], i: int, j: int, tag: str, ghunnah: bool) -> None:
    """The noon / meem is not pronounced when continuing; the next consonant is doubled."""
    noon = items[i]
    if noon.role == "tanween_n":
        del items[i]  # tanween never has an n at a stop either
        j -= 1
    else:
        noon.variant = Variant.WAQF_ONLY  # only a stop keeps the plain noon
    group = [k for k in range(j, min(j + 2, len(items)))
             if items[k].role == "cons" and items[k].ph == items[j].ph
             and word_key(items[k]) == word_key(items[j])]
    for k in group:
        items[k].rules.append(tag)
    if len(group) == 1:  # not doubled by a shaddah in the text: add the doubled copy
        first = items[j]
        copy = Item(first.ph, first.word, first.span, first.role, (), list(first.rules),
                    Variant.AFTER_CONTINUE, surah=first.surah, ayah=first.ayah,
                    host_marks=first.host_marks, letter=first.letter)
        items.insert(j + 1, copy)
        group = [j, j + 1]
    if ghunnah:
        items[group[-1]].dur = (2,)


# -- meem sakinah (spec 3.2) and qalqalah (spec 3.4) -----------------------------------------------

MEEM_CP = 0x0645
QALQALAH_LETTERS = frozenset(["q", "T", "b", "j", "d"])


def apply_meem_rules(items: list[Item], diags: list[Diagnostic]) -> None:
    sakin_block = set(SHORT_VOWELS) | {u.SHADDA}
    for i in reversed([k for k, it in enumerate(items)
                       if it.role == "cons" and it.ph == "m" and it.letter == MEEM_CP
                       and not it.host_marks & sakin_block]):
        meem = items[i]
        j = _next_consonant(items, i)
        if j is None:
            continue
        nxt = items[j]
        if nxt.ph == "b":
            _hide(items, i, "ikhfa_shafawi")
        elif nxt.ph == "m":
            if word_key(meem) == word_key(nxt):
                diags.append(Diagnostic(f"{meem.surah}:{meem.ayah}:{meem.word}",
                                        "meem_idgham_inside_word", meem.span[0]))
                continue
            _idgham(items, i, j, "idgham_mithlayn_saghir", ghunnah=True)
        else:
            meem.rules.append("izhar_shafawi")


def apply_qalqalah(items: list[Item]) -> None:
    """Qalqalah letters with a sukun. At a word end the stop variant is kubra (stronger)."""
    for i in reversed([k for k, it in enumerate(items)
                       if it.role == "cons" and it.ph in QALQALAH_LETTERS
                       and u.SUKUN in it.host_marks and u.SHADDA not in it.host_marks]):
        letter = items[i]
        if _word_ends_after(items, i):
            kubra = Item(letter.ph, letter.word, letter.span, letter.role, letter.dur,
                         list(letter.rules) + ["qalqalah_kubra"], Variant.WAQF_ONLY,
                         surah=letter.surah, ayah=letter.ayah, host_marks=letter.host_marks,
                         letter=letter.letter)
            letter.variant = Variant.CONTINUE_ONLY
            letter.rules.append("qalqalah_sughra")
            items.insert(i + 1, kubra)
        else:
            letter.rules.append("qalqalah_sughra")
