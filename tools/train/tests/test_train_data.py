from __future__ import annotations

import json
from pathlib import Path

import pytest

from tools.quran_text.loader import data_available, load_quran
from tools.train.build_manifest import manifest_line
from tools.train.fetch_everyayah import clip_url, parse_surahs
from tools.train.labels import TOKEN_ID, ayah_labels

needs_data = pytest.mark.skipif(not data_available(), reason="data/quran missing")


def test_clip_url_and_surah_list():
    assert clip_url("Alafasy_128kbps", 1, 7) == "https://everyayah.com/data/Alafasy_128kbps/001007.mp3"
    assert parse_surahs("1,78-80") == [1, 78, 79, 80]
    with pytest.raises(ValueError):
        parse_surahs("0")


def test_manifest_line_round_trips():
    row = json.loads(manifest_line("r", 1, 2, Path("a.wav"), 3.14159, [1, 2]))
    assert row["id"] == "r/001002" and row["seconds"] == 3.142 and row["labels"] == [1, 2]


@needs_data
def test_labels_for_an_ayah_with_phonemes():
    from tools.prototype.phoneme_data import build_word_phonemes

    quran = load_quran()
    words = build_word_phonemes(quran)
    labels = ayah_labels(quran, words, 1, 2)
    assert labels and all(1 <= t <= len(TOKEN_ID) - 1 for t in labels)
    assert len(labels) > 10


@needs_data
def test_disconnected_letters_ayah_has_no_labels():
    from tools.prototype.phoneme_data import build_word_phonemes

    quran = load_quran()
    assert ayah_labels(quran, build_word_phonemes(quran), 2, 1) is None
