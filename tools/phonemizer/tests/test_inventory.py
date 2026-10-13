import unicodedata

import pytest

from tools.phonemizer import inventory as inv


def test_inventory_size():
    consonants = [p for p in inv.PHONEMES if p.kind is inv.Kind.CONSONANT]
    assert len(consonants) == 27  # 28 letters minus alif, which is a long /a/ or hamza seat, not a consonant
    assert sum(p.kind is inv.Kind.HAMZA for p in inv.PHONEMES) == 1
    assert [p.symbol for p in inv.PHONEMES if p.kind is inv.Kind.VOWEL] == ["a", "i", "u"]
    assert len(inv.SYMBOLS) == 31  # 27 consonants + hamza + 3 vowels


def test_symbols_are_unique_ascii():
    assert len(set(inv.SYMBOLS)) == len(inv.SYMBOLS)
    assert all(s.isascii() and s.isalnum() for s in inv.SYMBOLS)
    assert inv.BLANK not in inv.SYMBOLS


def test_codepoints_match_official_unicode_names():
    for p in inv.PHONEMES:
        if p.codepoint is None:
            continue
        assert unicodedata.name(chr(p.codepoint)) == p.unicode_name, p.symbol


def test_letter_codepoints_are_unique():
    cps = [p.codepoint for p in inv.PHONEMES if p.codepoint is not None]
    assert len(cps) == len(set(cps)) == 28


def test_vocab_blank_is_zero_and_ids_are_dense():
    assert inv.VOCAB[0] == inv.BLANK
    assert inv.TOKEN_ID[inv.BLANK] == 0
    assert sorted(inv.TOKEN_ID.values()) == list(range(32))
    assert inv.VOCAB[1] == "hz"  # order is part of the contract


def test_expected_phoneme_validation():
    ok = inv.ExpectedPhoneme("n", word=1, char_span=(0, 1), rules=("ikhfa",), dur=(2,))
    assert ok.variant is inv.Variant.BOTH
    with pytest.raises(ValueError):
        inv.ExpectedPhoneme("xx", 1, (0, 1))
    with pytest.raises(ValueError):
        inv.ExpectedPhoneme("n", 0, (0, 1))
    with pytest.raises(ValueError):
        inv.ExpectedPhoneme("n", 1, (2, 1))
    with pytest.raises(ValueError):
        inv.ExpectedPhoneme("a", 1, (0, 1), dur=(0,))
