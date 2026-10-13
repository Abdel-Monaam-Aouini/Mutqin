"""Loader tests. Quran text is only read from data/quran/ (run download.py first); nothing is typed here."""

import shutil
import unicodedata
from pathlib import Path

import pytest

from tools.quran_text import loader
from tools.quran_text.download import CHECKSUMS_NAME

DATA = loader.DEFAULT_DATA_DIR
needs_data = pytest.mark.skipif(
    not (DATA / CHECKSUMS_NAME).exists(),
    reason="data/quran/ missing: run tools/quran_text/download.py --agree-tanzil-terms",
)


@pytest.fixture(scope="module")
def quran():
    return loader.load_quran(DATA)


@needs_data
def test_counts(quran):
    assert len(quran.surahs) == 114
    assert quran.ayah_count == 6236
    assert [s.number for s in quran.surahs] == list(range(1, 115))


@needs_data
def test_checksums_verify():
    loader.verify_checksums(DATA)


@needs_data
def test_modified_source_file_is_rejected(tmp_path: Path):
    for p in DATA.glob("*.txt"):
        shutil.copy(p, tmp_path / p.name)
    shutil.copy(DATA / CHECKSUMS_NAME, tmp_path / CHECKSUMS_NAME)
    victim = tmp_path / loader.UTHMANI_FILE
    victim.write_bytes(victim.read_bytes() + b"\n")
    with pytest.raises(loader.QuranDataError, match="checksum mismatch"):
        loader.load_quran(tmp_path)


def test_missing_data_dir_is_rejected(tmp_path: Path):
    with pytest.raises(loader.QuranDataError):
        loader.load_quran(tmp_path)


@needs_data
def test_word_ids_are_stable_and_one_based(quran):
    for surah in quran.surahs:
        for ayah in surah.ayahs:
            assert ayah.words, ayah.id
            for i, word in enumerate(ayah.words, 1):
                assert word.id == f"{surah.number}:{ayah.ayah}:{i}"
    assert quran.word("1:1:1").id == "1:1:1"


@needs_data
def test_basmala_prefix_only_where_expected(quran):
    for surah in quran.surahs:
        assert len(surah.basmala) == (0 if surah.number in (1, 9) else 4)


@needs_data
def test_source_text_round_trips_exactly(quran):
    """Rebuilding each ayah from basmala + words + marks gives the source line text unchanged."""
    source = loader.parse_lines(DATA / loader.UTHMANI_FILE)
    for surah in quran.surahs:
        for ayah in surah.ayahs:
            tokens = list(surah.basmala) if ayah.ayah == 1 else []
            for w in ayah.words:
                tokens.append(w.text)
                tokens.extend(w.marks)
            assert " ".join(tokens) == source[(surah.number, ayah.ayah)], ayah.id


@needs_data
def test_no_word_is_mark_only(quran):
    for surah in quran.surahs:
        for ayah in surah.ayahs:
            for w in ayah.words:
                assert not loader.is_mark_only(w.text), w.id


@needs_data
def test_mark_ranges_agree_with_unicode_letters():
    """Every token the loader calls mark-only has no Unicode letter; every other token has one."""
    source = loader.parse_lines(DATA / loader.UTHMANI_FILE)
    for text in source.values():
        for token in text.split(" "):
            has_letter = any(unicodedata.category(c).startswith("L") and not 0x06D6 <= ord(c) <= 0x06ED
                             for c in token)
            assert loader.is_mark_only(token) != has_letter


@needs_data
def test_simple_text_present_for_every_ayah(quran):
    for surah in quran.surahs:
        for ayah in surah.ayahs:
            assert ayah.simple_text
