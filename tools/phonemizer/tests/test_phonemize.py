"""Phonemizer tests. Synthetic cases use arbitrary code points that do not spell Quran words;
Quran text is only read from data/quran/."""

import pytest

from tools.phonemizer import uthmani as u
from tools.phonemizer.inventory import BY_SYMBOL, Variant
from tools.phonemizer.phonemize import phonemize_ayah
from tools.quran_text import loader
from tools.quran_text.download import CHECKSUMS_NAME
from tools.quran_text.model import Ayah, Word

needs_data = pytest.mark.skipif(
    not (loader.DEFAULT_DATA_DIR / CHECKSUMS_NAME).exists(), reason="data/quran/ missing"
)

BA, TA, NOON, LAM, WAW, YEH, ALEF, MAKSURA = (
    "ب", "ت", "ن", "ل", "و", "ي", "ا", "ى")
FATHA, DAMMA, KASRA, SHADDA, SUKUN = "َ", "ُ", "ِ", "ّ", "ْ"
FATHATAN, SUPERSCRIPT_ALEF, ROUNDED_ZERO = "ً", "ٰ", "۟"


WASLA = "\u0671"


def run(*words: str):
    ayah = Ayah(1, 1, tuple(Word(1, 1, i, w) for i, w in enumerate(words, 1)), "")
    return phonemize_ayah(ayah)


def syms(result):
    return [(p.ph, p.dur) for p in result.phonemes]


def test_consonant_with_short_vowels():
    assert syms(run(BA + FATHA + TA + KASRA + LAM + DAMMA)) == [
        ("b", ()), ("a", (1,)), ("t", ()), ("i", (1,)), ("l", ()), ("u", (1,))]


def test_sukun_emits_no_vowel():
    assert [p for p, _ in syms(run(BA + SUKUN))] == ["b"]


def test_shaddah_doubles_the_consonant():
    assert [p for p, _ in syms(run(BA + SHADDA + FATHA))] == ["b", "b", "a"]


def test_alef_after_fatha_is_long_a():
    r = run(BA + FATHA + ALEF)
    assert syms(r) == [("b", ()), ("a", (2,))]
    assert r.phonemes[1].char_span == (1, 3)  # fatha .. alef


def test_waw_and_yeh_madd_letters():
    assert syms(run(BA + DAMMA + WAW))[-1] == ("u", (2,))
    assert syms(run(BA + KASRA + YEH))[-1] == ("i", (2,))


def test_waw_with_sukun_is_a_consonant():
    assert [p for p, _ in syms(run(BA + FATHA + WAW + SUKUN))] == ["b", "a", "w"]


def test_tanween_is_vowel_plus_n():
    assert [p for p, _ in syms(run(BA + FATHATAN))] == ["b", "a", "n"]


def test_superscript_alef_lengthens_the_fatha():
    assert syms(run(BA + FATHA + SUPERSCRIPT_ALEF)) == [("b", ()), ("a", (2,))]


def test_silent_alef_marked_with_rounded_zero():
    assert [p for p, _ in syms(run(BA + DAMMA + WAW + ALEF + ROUNDED_ZERO))] == ["b", "u"]


def test_alef_maksura_after_fatha_is_long_a():
    assert syms(run(BA + FATHA + MAKSURA))[-1] == ("a", (2,))


def test_yeh_shaped_letter_with_own_vowel_is_consonant_y():
    assert [p for p, _ in syms(run(BA + KASRA + MAKSURA + FATHA))] == ["b", "i", "y", "a"]


def test_unexpected_pattern_is_a_diagnostic_not_a_guess():
    r = run(BA + SUKUN + ALEF)  # alef after a consonant with no fatha
    assert [d.kind for d in r.diagnostics] == ["alef_without_fatha"]


def test_vowel_never_lengthens_across_words():
    r = run(BA + FATHA, ALEF + BA + FATHA)
    assert [d.kind for d in r.diagnostics] == ["alef_without_fatha"]


def test_article_before_assimilated_letter(tmp_path=None):
    # wasla + bare lam + (ta with shaddah): lam is silent, the next consonant is doubled and tagged
    r = run(WASLA + LAM + TA + SHADDA + FATHA)
    cont = [(p.ph, p.rules) for p in r.phonemes if p.variant is Variant.BOTH]
    assert [ph for ph, _ in cont] == ["t", "t", "a"]
    assert cont[0][1] == ("lam_shamsiyyah",) and cont[1][1] == ("lam_shamsiyyah",)
    start = [p.ph for p in r.phonemes if p.variant is Variant.START_ONLY]
    assert start == ["hz", "a"]  # hamzat al-wasl of the article, only when starting


def test_article_before_clear_letter():
    r = run(WASLA + LAM + SUKUN + BA + FATHA)
    cont = [(p.ph, p.rules) for p in r.phonemes if p.variant is Variant.BOTH]
    assert [ph for ph, _ in cont] == ["l", "b", "a"]
    assert cont[0][1] == ("lam_qamariyyah",)


def test_wasla_of_a_non_article_word_is_reported_not_guessed():
    r = run(WASLA + BA + SUKUN + TA + FATHA)
    assert [p.ph for p in r.phonemes] == ["b", "t", "a"]
    assert [d.kind for d in r.diagnostics] == ["wasla_start_vowel_unknown"]


def test_bare_lam_outside_the_article_is_tagged_neutrally():
    r = run(BA + FATHA + LAM + TA + SHADDA + FATHA)
    assert [p.ph for p in r.phonemes] == ["b", "a", "t", "t", "a"]
    assert r.phonemes[2].rules == ("lam_assimilated",)


# Locations the provisional tables leave to task 1.9 (special cases, keyed by word id).
EXPECTED_SPECIAL = {
    "long_a_without_fatha": {"11:41:6"},
    "bare_yeh_after_a": {"68:6:1"},
    "bare_waw_after_a": {"83:3:3"},
}


@needs_data
def test_whole_quran_phonemizes_with_only_known_special_cases():
    quran = loader.load_quran()
    found: dict[str, set[str]] = {}
    disconnected = set()
    wasla_unknown = 0
    for surah in quran.surahs:
        for ayah in surah.ayahs:
            result = phonemize_ayah(ayah)
            words = {w.index: w for w in ayah.words}
            for p in result.phonemes:  # ExpectedPhoneme validated its own fields on creation
                assert p.ph in BY_SYMBOL
                assert 0 <= p.char_span[0] < p.char_span[1] <= len(words[p.word].text)
                if p.ph in "aiu":
                    assert p.dur in ((1,), (2,))
                else:
                    assert p.dur == ()
            for d in result.diagnostics:
                if d.kind == "disconnected_letters":
                    disconnected.add(d.word_id)
                elif d.kind == "wasla_start_vowel_unknown":
                    wasla_unknown += 1  # starting vowels of non-article words: open question
                else:
                    found.setdefault(d.kind, set()).add(d.word_id)
    assert found == EXPECTED_SPECIAL
    assert wasla_unknown == 1488
    assert len(disconnected) == 30  # surah-opening letters: all at word 1 of ayah 1, plus 42:2:1
    assert all(w.endswith(":1:1") or w == "42:2:1" for w in disconnected)


@needs_data
def test_article_lam_sun_and_moon_letters_come_out_consistent():
    """Assimilated (bare lam) and clear (lam with sukun) next-letters must be disjoint, apart from
    teh, where the clear cases are verbs, not the article. 14 letters assimilate."""
    quran = loader.load_quran()
    assimilated, clear = set(), set()
    for surah in quran.surahs:
        for ayah in surah.ayahs:
            for word in ayah.words:
                units = u.parse_word(word.text)
                for i, unit in enumerate(units):
                    if (ord(unit.char) == 0x0644 and i and ord(units[i - 1].char) == 0x0671
                            and i + 1 < len(units)):
                        if not unit.marks:
                            assimilated.add(units[i + 1].char)
                        elif unit.has(u.SUKUN):
                            clear.add(units[i + 1].char)
    assert len(assimilated) == 14
    assert assimilated & clear == {"\u062A"}
