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
    long_by: int = 0  # code point of the letter / sign that made this vowel long
    flags: set[str] = field(default_factory=set)  # "iwad", "alef_waqf": stop-only lengthening
    signs: set[int] = field(default_factory=set)
    letter: int = 0  # code point of the host letter


@dataclass
class AyahResult:
    items: list[Item]
    diagnostics: list[Diagnostic]

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


def _marks_of_host(unit: u.LetterUnit) -> tuple[str, ...]:
    """Marks that belong to the base letter: those before a hamza above (what follows it, such as
    a sukun, belongs to the hamza)."""
    for k, m in enumerate(unit.marks):
        if ord(m) == u.HAMZA_ABOVE:
            return unit.marks[:k]
    return unit.marks


class _Builder:
    def __init__(self, ayahs: tuple[Ayah, ...]) -> None:
        self.ayahs = ayahs
        self.cur: Ayah = ayahs[0]
        self.items: list[Item] = []
        self.diags: list[Diagnostic] = []
        self.word: Word | None = None
        self.units: tuple[u.LetterUnit, ...] = ()
        self.pos = 0  # index of the current unit in self.units
        self.pending_tags: list[str] = []  # applied to the next consonant (assimilated lam)

    # -- helpers ---------------------------------------------------------------------------
    def diag(self, kind: str, offset: int) -> None:
        assert self.word is not None
        self.diags.append(Diagnostic(self.word.id, kind, offset))

    def add(
        self, ph: str, span: tuple[int, int], role: str, dur: tuple[int, ...] = (), letter: int = 0
    ) -> Item:
        assert self.word is not None
        item = Item(
            ph, self.word.index, span, role, dur, letter=letter, surah=self.word.surah, ayah=self.word.ayah
        )
        self.items.append(item)
        return item

    def last(self) -> Item | None:
        """Previous item if it belongs to the word being processed."""
        if (
            self.items
            and self.word is not None
            and word_key(self.items[-1]) == (self.word.surah, self.word.ayah, self.word.index)
        ):
            return self.items[-1]
        return None

    def lengthen(self, vowel: str, end: int, by: int = 0) -> bool:
        """Make the previous short vowel long (dur 2). Returns False if there is none."""
        last = self.last()
        if (
            last
            and last.role == "vowel"
            and last.ph == vowel
            and last.dur == (1,)
            and last.variant is Variant.BOTH
        ):
            last.dur = (2,)
            last.span = (last.span[0], end)
            last.long_by = by
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
                if not self.lengthen("a", span[1], cp):
                    self.add("a", span, "vowel", (2,)).long_by = cp
                    self.diag("long_a_without_fatha", span[0])
            elif cp == u.HAMZA_ABOVE:
                hz = self.add("hz", span, "cons", letter=u.HAMZA_ABOVE)
                hz.host_marks = frozenset(ord(m) for m in unit.marks[k + 1 :])  # e.g. its sukun
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
                elif not self.lengthen("i", span[1], cp):
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
            host.host_marks = frozenset(ord(m) for m in _marks_of_host(unit))
            if ph == "hz" and self.pos and ord(self.units[self.pos - 1].char) == ALEF_WASLA:
                host.rules.append("hamza_after_wasla")  # no madd before it (the text has no maddah)
            if times == 2 and n == 1 and ph in ("n", "m"):  # mushaddad noon / meem (spec 3.3)
                host.rules.append("ghunnah")
                host.dur = (2,)
        self.marks(unit, host)

    def alef(self, unit: u.LetterUnit) -> None:
        if unit.has(ROUNDED_ZERO):
            return  # written, not pronounced
        if unit.has(RECTANGULAR_ZERO):  # DRAFT: silent when continuing, pronounced at waqf
            last = self.last()
            if last and last.role == "vowel" and last.ph == "a":
                last.flags.add("alef_waqf")
            else:
                self.diag("rectangular_zero_alef_without_fatha", unit.start)
            return
        last = self.last()
        if last and last.role == "vowel" and last.ph == "a" and last.dur == (1,):
            self.lengthen("a", unit.end, ALEF)
        elif last and last.role == "tanween_n" and len(self.items) >= 2 and self.items[-2].ph == "a":
            self.items[-2].flags.add("iwad")  # tanween alef
        else:
            self.diag("alef_without_fatha", unit.start)
        for mark in unit.marks:
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
        if not self.lengthen(vowel, unit.end, ord(unit.char)):
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
            ord(m) in SHORT_VOWELS or ord(m) in TANWEEN or ord(m) in (u.SHADDA, u.SUKUN) for m in unit.marks
        )
        if own:
            self.consonant(unit, "y" if cp in (YEH, ALEF_MAKSURA) else "w")  # yeh-shaped: y
            return
        if unit.has(u.SUPERSCRIPT_ALEF):  # carrier for the superscript alef
            self.marks(unit, None)
            return
        last = self.last()
        prev_vowel = last.ph if last and last.role == "vowel" else None
        if (
            cp == ALEF_MAKSURA
            and last
            and last.role == "tanween_n"
            and len(self.items) >= 2
            and self.items[-2].ph == "a"
        ):
            self.items[-2].flags.add("iwad")  # tanween alef
        elif cp == WAW and prev_vowel == "a" and self.next_unit_has(unit, ROUNDED_ZERO):
            # bare waw after fatha before a silent alef: consonant w, sukun implied by encoding
            self.add("w", (unit.start, unit.start + 1), "cons", letter=cp).rules.append("implicit_sukun")
        elif cp == WAW and prev_vowel == "u":
            self.lengthen("u", unit.end, cp)
        elif cp == YEH and prev_vowel == "i":
            self.lengthen("i", unit.end, cp)
        elif cp == ALEF_MAKSURA and prev_vowel in ("a", "i"):
            self.lengthen(prev_vowel, unit.end, cp)
        else:
            self.diag(
                f"bare_{'waw' if cp == WAW else 'yeh' if cp == YEH else 'maksura'}"
                f"_after_{prev_vowel or 'nothing'}",
                unit.start,
            )
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
        apply_madd(self.items)
        apply_leen(self.items)
        apply_waqf(self.items)
        return AyahResult(self.items, self.diags)

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
    return (
        item.role == "cons"
        and item.ph == "n"
        and item.letter == NOON_CP
        and not item.host_marks & (set(SHORT_VOWELS) | {u.SHADDA})
    )


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
            diags.append(
                Diagnostic(
                    f"{noon.surah}:{noon.ayah}:{noon.word}",
                    "noon_sukun_before_non_izhar_letter",
                    noon.span[0],
                )
            )
            continue

        if letter in IZHAR_HALQI:
            noon.rules.append("izhar_halqi")
        elif letter in IDGHAM_GHUNNAH and same_word and letter in ("y", "w"):
            noon.rules.append("izhar_mutlaq")
        elif letter in IDGHAM_GHUNNAH | IDGHAM_NO_GHUNNAH:
            if same_word:
                diags.append(
                    Diagnostic(
                        f"{noon.surah}:{noon.ayah}:{noon.word}", "noon_idgham_inside_word", noon.span[0]
                    )
                )
                continue
            _idgham(
                items,
                i,
                j,
                "idgham_ghunnah" if letter in IDGHAM_GHUNNAH else "idgham_no_ghunnah",
                ghunnah=letter in IDGHAM_GHUNNAH,
            )
        elif letter in IQLAB:
            _hide(items, i, "iqlab", ph="m")
        elif letter in IKHFA:
            _hide(items, i, "ikhfa")
        else:
            diags.append(
                Diagnostic(
                    f"{noon.surah}:{noon.ayah}:{noon.word}", f"noon_before_unmapped_{letter}", noon.span[0]
                )
            )


def _hide(items: list[Item], i: int, rule: str, ph: str | None = None) -> None:
    """Ikhfa / iqlab: hidden sound with ghunnah (2 counts). At a word end this only holds when
    continuing; a stop keeps the plain noon (tanween has no n at a stop, nothing to keep)."""
    noon = items[i]
    boundary = noon.role == "tanween_n" or _word_ends_after(items, i)
    if boundary and noon.role != "tanween_n":
        plain = Item(
            noon.ph,
            noon.word,
            noon.span,
            noon.role,
            noon.dur,
            list(noon.rules),
            Variant.WAQF_ONLY,
            surah=noon.surah,
            ayah=noon.ayah,
            host_marks=noon.host_marks,
            letter=noon.letter,
        )
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
    group = [
        k
        for k in range(j, min(j + 2, len(items)))
        if items[k].role == "cons" and items[k].ph == items[j].ph and word_key(items[k]) == word_key(items[j])
    ]
    # The merged (doubled, ghunnah) form exists only when the reading goes on from the previous
    # word; after a stop the next word starts plainly. The tag therefore goes on the extra copy.
    last = items[group[-1]]
    fused = _clone(last, Variant.AFTER_CONTINUE, list(last.rules) + [tag], (2,) if ghunnah else last.dur)
    if len(group) == 2:  # the text already doubles the letter (shaddah): keep a plain restart form
        last.variant = Variant.START_ONLY
    items.insert(group[-1] + 1, fused)


# -- meem sakinah (spec 3.2) and qalqalah (spec 3.4) -----------------------------------------------

MEEM_CP = 0x0645
QALQALAH_LETTERS = frozenset(["q", "T", "b", "j", "d"])


def apply_meem_rules(items: list[Item], diags: list[Diagnostic]) -> None:
    sakin_block = set(SHORT_VOWELS) | {u.SHADDA}
    for i in reversed(
        [
            k
            for k, it in enumerate(items)
            if it.role == "cons" and it.ph == "m" and it.letter == MEEM_CP and not it.host_marks & sakin_block
        ]
    ):
        meem = items[i]
        j = _next_consonant(items, i)
        if j is None:
            continue
        nxt = items[j]
        if nxt.ph == "b":
            _hide(items, i, "ikhfa_shafawi")
        elif nxt.ph == "m":
            if word_key(meem) == word_key(nxt):
                diags.append(
                    Diagnostic(
                        f"{meem.surah}:{meem.ayah}:{meem.word}", "meem_idgham_inside_word", meem.span[0]
                    )
                )
                continue
            _idgham(items, i, j, "idgham_mithlayn_saghir", ghunnah=True)
        else:
            meem.rules.append("izhar_shafawi")


def apply_qalqalah(items: list[Item]) -> None:
    """Qalqalah letters with a sukun. At a word end the stop variant is kubra (stronger)."""
    for i in reversed(
        [
            k
            for k, it in enumerate(items)
            if it.role == "cons"
            and it.ph in QALQALAH_LETTERS
            and u.SUKUN in it.host_marks
            and u.SHADDA not in it.host_marks
        ]
    ):
        letter = items[i]
        if _word_ends_after(items, i):
            kubra = Item(
                letter.ph,
                letter.word,
                letter.span,
                letter.role,
                letter.dur,
                list(letter.rules) + ["qalqalah_kubra"],
                Variant.WAQF_ONLY,
                surah=letter.surah,
                ayah=letter.ayah,
                host_marks=letter.host_marks,
                letter=letter.letter,
            )
            letter.variant = Variant.CONTINUE_ONLY
            letter.rules.append("qalqalah_sughra")
            items.insert(i + 1, kubra)
        else:
            letter.rules.append("qalqalah_sughra")


# -- madd (spec 3.7) --------------------------------------------------------------------------------

HEH_CP = 0x0647
MUTTASIL_MUNFASIL = (4, 5)  # allowed counts; the user's 4 or 5 preference narrows this later
LAZIM = (6,)
AROID_LEEN = (2, 4, 6)


def _clone(item: Item, variant: Variant, rules: list[str], dur: tuple[int, ...]) -> Item:
    return Item(
        item.ph,
        item.word,
        item.span,
        item.role,
        dur,
        rules,
        variant,
        surah=item.surah,
        ayah=item.ayah,
        host_marks=item.host_marks,
        letter=item.letter,
        maddah=item.maddah,
        long_by=item.long_by,
    )


def _next_pronounced(items: list[Item], i: int) -> int | None:
    """Next item after i that is read when continuing (skips start-only items), same surah."""
    j = i + 1
    while j < len(items) and items[j].variant is Variant.START_ONLY:
        j += 1
    if j < len(items) and items[j].surah == items[i].surah:
        return j
    return None


def _is_madd_hamza(item: Item | None) -> bool:
    """A hamza that lengthens the preceding vowel. A hamza right after hamzat al-wasl does not:
    the wasla is dropped, and the text writes no maddah there (open question for the teacher)."""
    return (
        item is not None and item.role == "cons" and item.ph == "hz" and "hamza_after_wasla" not in item.rules
    )


def _is_sakin_consonant(items: list[Item], k: int) -> bool:
    """Consonant that is silent-vowelled: explicit sukun, or the first half of a shaddah pair."""
    c = items[k]
    if c.role != "cons":
        return False
    if u.SUKUN in c.host_marks:
        return True
    return (
        u.SHADDA in c.host_marks
        and k + 1 < len(items)
        and items[k + 1].ph == c.ph
        and word_key(items[k + 1]) == word_key(c)
        and items[k + 1].span == c.span
    )


def _last_consonant_of_word(items: list[Item], k: int) -> bool:
    """True if items[k] is a consonant and no other consonant of its word follows it."""
    key = word_key(items[k])
    for m in range(k + 1, len(items)):
        if word_key(items[m]) != key:
            return True
        if items[m].role == "cons":
            return False
    return True


def apply_madd(items: list[Item]) -> None:
    for i in reversed(
        [
            k
            for k, it in enumerate(items)
            if it.role == "vowel" and it.dur == (2,) and it.long_by and it.variant is Variant.BOTH
        ]
    ):
        vowel = items[i]
        prev = items[i - 1] if i else None
        silah = (
            vowel.long_by in (SMALL_WAW, SMALL_YEH)
            and prev is not None
            and prev.role == "cons"
            and prev.letter == HEH_CP
        )
        badal = (
            prev is not None
            and prev.role == "cons"
            and prev.ph == "hz"
            and prev.variant is Variant.BOTH
            and word_key(prev) == word_key(vowel)
        )
        k = _next_pronounced(items, i)
        nxt = items[k] if k is not None else None
        word_final = nxt is None or word_key(nxt) != word_key(vowel)

        base_rule = "madd_badal" if badal else "madd_tabii"
        if silah:
            hamza_follows = _is_madd_hamza(nxt)
            vowel.variant = Variant.CONTINUE_ONLY  # the pronoun's silah only exists when continuing
            vowel.rules.append("madd_silah_kubra" if hamza_follows else "madd_silah_sughra")
            vowel.dur = MUTTASIL_MUNFASIL if hamza_follows else (2,)
        elif _is_madd_hamza(nxt):
            if not word_final:
                vowel.rules.append("madd_muttasil")
                vowel.dur = MUTTASIL_MUNFASIL
            else:  # the long vowel ends its word: munfasil only if the reading goes on
                stop = _clone(vowel, Variant.WAQF_ONLY, [base_rule], (2,))
                vowel.variant = Variant.CONTINUE_ONLY
                vowel.rules.append("madd_munfasil")
                vowel.dur = MUTTASIL_MUNFASIL
                items.insert(i + 1, stop)
        elif nxt is not None and not word_final and _is_sakin_consonant(items, k):
            vowel.rules.append("madd_lazim")
            vowel.dur = LAZIM
        elif (
            nxt is not None
            and not word_final
            and nxt.role == "cons"
            and _last_consonant_of_word(items, k)
            and not _has_fathatan(items, k)
        ):
            # sukun only appears at a stop ('arid lis-sukun)
            stop = _clone(vowel, Variant.WAQF_ONLY, ["madd_arid_lissukun"], AROID_LEEN)
            vowel.variant = Variant.CONTINUE_ONLY
            vowel.rules.append(base_rule)
            items.insert(i + 1, stop)
        else:
            vowel.rules.append(base_rule)


def _has_fathatan(items: list[Item], k: int) -> bool:
    """The consonant at k carries tanween fatha (it becomes a long a at a stop, not a sukun)."""
    key = word_key(items[k])
    for m in range(k + 1, len(items)):
        if word_key(items[m]) != key:
            return False
        if items[m].role == "tanween_n":
            return len(items) > m and items[m - 1].ph == "a"
        if items[m].role == "cons":
            return False
    return False


def apply_leen(items: list[Item]) -> None:
    """Waw / yeh with a sukun after a fatha, before the last letter of the word: at a stop the
    leen letter may be lengthened (2, 4 or 6 counts)."""
    for i in reversed(
        [
            k
            for k, it in enumerate(items)
            if it.role == "cons"
            and it.ph in ("w", "y")
            and u.SUKUN in it.host_marks
            and it.variant is Variant.BOTH
        ]
    ):
        prev = items[i - 1] if i else None
        if not (
            prev
            and prev.role == "vowel"
            and prev.ph == "a"
            and prev.dur == (1,)
            and word_key(prev) == word_key(items[i])
        ):
            continue
        k = _next_pronounced(items, i)
        if k is None or items[k].role != "cons" or word_key(items[k]) != word_key(items[i]):
            continue
        if not _last_consonant_of_word(items, k):
            continue
        leen = items[i]
        stop = _clone(leen, Variant.WAQF_ONLY, list(leen.rules) + ["madd_leen"], AROID_LEEN)
        leen.variant = Variant.CONTINUE_ONLY
        items.insert(i + 1, stop)


# -- waqf (spec section 5) ---------------------------------------------------------------------------


def apply_waqf(items: list[Item]) -> None:
    """End-of-word behaviour at a stop: the final short vowel is dropped (tanween fatha becomes a
    long a, 'iwad), teh marbuta becomes heh, a final qalqalah letter that is only sakin because of
    the stop is kubra. Every such item pair keeps the continuing reading as continue_only."""
    groups: list[list[int]] = []
    for k, it in enumerate(items):
        if groups and word_key(items[groups[-1][0]]) == word_key(it):
            groups[-1].append(k)
        else:
            groups.append([k])
    for idxs in reversed(groups):
        cont = [k for k in idxs if items[k].variant in (Variant.BOTH, Variant.CONTINUE_ONLY)]
        end = len(cont)
        while end and items[cont[end - 1]].role == "tanween_n":
            end -= 1
        if not end:
            continue
        v_idx = cont[end - 1]
        vowel = items[v_idx]
        if not (vowel.role == "vowel" and vowel.dur == (1,) and vowel.variant is Variant.BOTH):
            continue
        if any(items[k].role == "cons" for k in idxs if k > v_idx):
            continue  # a stop-only consonant (the noon of an idgham) follows: the vowel is not final
        cons = [k for k in cont[: end - 1] if items[k].role == "cons"]
        c_idx = cons[-1] if cons else None

        extra = None
        if "iwad" in vowel.flags:
            extra = _clone(vowel, Variant.WAQF_ONLY, ["madd_iwad"], (2,))
        elif "alef_waqf" in vowel.flags:
            extra = _clone(vowel, Variant.WAQF_ONLY, ["alef_waqf"], (2,))
        vowel.variant = Variant.CONTINUE_ONLY  # a stop drops the final vowel
        if extra is not None:
            items.insert(v_idx + 1, extra)
        if c_idx is None:
            continue
        c = items[c_idx]
        if c.variant is not Variant.BOTH:
            continue
        if "teh_marbuta" in c.rules:
            stop = _clone(c, Variant.WAQF_ONLY, ["teh_marbuta_waqf"], ())
            stop.ph = "h"
        elif c.ph in QALQALAH_LETTERS:
            stop = _clone(c, Variant.WAQF_ONLY, list(c.rules) + ["qalqalah_kubra"], c.dur)
        else:
            continue
        c.variant = Variant.CONTINUE_ONLY
        items.insert(c_idx + 1, stop)


def select_reading(
    items: list[Item], stops: set[tuple[int, int, int]] | None = None, all_stops: bool = False
) -> list[Item]:
    """The phonemes of one concrete reading. `stops` holds (surah, ayah, word) keys of words the
    reciter stops at; a surah end is always a stop and the first word starts a recitation."""
    stops = stops or set()
    out: list[Item] = []
    prev_key: tuple[int, int, int] | None = None
    prev_stopped = True
    stopped_here = False
    for it in items:
        key = word_key(it)
        if key != prev_key:
            if prev_key is not None:
                prev_stopped = all_stops or prev_key in stops or prev_key[0] != key[0]
            stopped_here = all_stops or key in stops
            prev_key = key
        v = it.variant
        keep = (
            v is Variant.BOTH
            or (v is Variant.CONTINUE_ONLY and not stopped_here)
            or (v is Variant.WAQF_ONLY and stopped_here)
            or (v is Variant.START_ONLY and prev_stopped)
            or (v is Variant.AFTER_CONTINUE and not prev_stopped)
        )
        if keep:
            out.append(it)
    return out
