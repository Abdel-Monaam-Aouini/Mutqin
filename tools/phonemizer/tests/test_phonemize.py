"""Phonemizer tests. Synthetic cases use arbitrary code points that do not spell Quran words;
Quran text is only read from data/quran/."""

import pytest

from tools.phonemizer.inventory import BY_SYMBOL
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
                else:
                    found.setdefault(d.kind, set()).add(d.word_id)
    assert found == EXPECTED_SPECIAL
    assert len(disconnected) == 30  # surah-opening letters: all at word 1 of ayah 1, plus 42:2:1
    assert all(w.endswith(":1:1") or w == "42:2:1" for w in disconnected)
