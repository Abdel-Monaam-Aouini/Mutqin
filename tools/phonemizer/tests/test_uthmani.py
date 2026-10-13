"""Parsing tests. Quran text comes only from data/quran/; synthetic cases use code points that
do not spell Quran words."""

import unicodedata
from collections import Counter

import pytest

from tools.phonemizer import uthmani as u
from tools.quran_text import loader
from tools.quran_text.download import CHECKSUMS_NAME

needs_data = pytest.mark.skipif(
    not (loader.DEFAULT_DATA_DIR / CHECKSUMS_NAME).exists(), reason="data/quran/ missing"
)


@pytest.fixture(scope="module")
def quran():
    return loader.load_quran()


def test_synthetic_split_and_spans():
    units = u.parse_word("بَّتْ")  # beh+fatha+shadda, teh+sukun
    assert [x.char for x in units] == ["ب", "ت"]
    assert units[0].marks == ("َ", "ّ")
    assert (units[0].start, units[0].end, units[1].start, units[1].end) == (0, 3, 3, 5)
    assert units[0].has(u.SHADDA) and not units[0].has(u.SUKUN)


def test_non_combining_small_letters_and_tatweel_are_own_units():
    units = u.parse_word("بـۥ")
    assert [x.char for x in units] == ["ب", "ـ", "ۥ"]


def test_empty_word_has_no_units():
    assert u.parse_word("") == ()


def test_mark_without_base_is_rejected():
    with pytest.raises(u.UthmaniParseError, match="no base"):
        u.parse_word("َب")


def test_unknown_character_is_rejected():
    with pytest.raises(u.UthmaniParseError, match="unexpected"):
        u.parse_word("a")


def test_constants_have_expected_unicode_categories():
    assert all(unicodedata.category(chr(c)).startswith("L") for c in u.BASES)
    assert all(unicodedata.category(chr(c)) == "Mn" for c in u.MARKS)
    assert not u.BASES & u.MARKS


@needs_data
def test_every_word_parses_and_round_trips_exactly(quran):
    seen: Counter[int] = Counter()
    for surah in quran.surahs:
        for ayah in surah.ayahs:
            for word in ayah.words:
                units = u.parse_word(word.text)
                assert "".join(x.text for x in units) == word.text, word.id
                assert units and units[0].start == 0 and units[-1].end == len(word.text), word.id
                for a, b in zip(units, units[1:]):
                    assert a.end == b.start, word.id
                seen.update(ord(c) for c in word.text)
    # constants are not stale: every allowed character really occurs in the data
    assert set(seen) == u.BASES | u.MARKS
