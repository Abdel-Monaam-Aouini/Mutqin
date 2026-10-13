import os
from pathlib import Path

import pytest

from tools.phonemizer import validate as v
from tools.quran_text import loader
from tools.quran_text.download import CHECKSUMS_NAME

REF = Path(os.environ.get("MUTQIN_TAJWEED_REF", v.DEFAULT_REF))
needs_data = pytest.mark.skipif(
    not (loader.DEFAULT_DATA_DIR / CHECKSUMS_NAME).exists(), reason="data/quran/ missing")
needs_ref = pytest.mark.skipif(not REF.exists(), reason="reference dataset not found (see validate.py)")


def test_overlaps():
    assert v.overlaps(0, 3, 2, 5)
    assert not v.overlaps(0, 3, 3, 5)  # adjacent spans do not overlap
    assert not v.overlaps(5, 6, 0, 5)


def test_compare_counts_matched_near_and_only():
    ref = {(1, 1): [("ikhfa", 10, 13), ("ikhfa", 50, 52), ("iqlab", 100, 102)]}
    ours = {(1, 1): [("ikhfa", 11, 12, "1:1:1"), ("ikhfa", 56, 57, "1:1:2"), ("ikhfa", 200, 201, "1:1:3")]}
    s = v.compare(ref, ours)["stats"]
    assert (s["ikhfa"]["ref_matched"], s["ikhfa"]["ref_near"], s["ikhfa"]["ref_only"]) == (1, 1, 0)
    assert (s["ikhfa"]["ours_matched"], s["ikhfa"]["ours_near"], s["ikhfa"]["ours_only"]) == (1, 1, 1)
    assert s["iqlab"]["ref_only"] == 1


@needs_data
def test_word_offsets_drop_waqf_signs_but_keep_the_basmala_prefix():
    quran = loader.load_quran()
    off = v.ayah_word_offsets(quran, 1, 1)
    assert off[1] == 0 and off[2] == len(quran.word("1:1:1").text) + 1  # 1:1 is the basmala itself
    off2 = v.ayah_word_offsets(quran, 2, 1)
    assert off2[1] == sum(len(t) + 1 for t in quran.surahs[1].basmala)


@needs_data
@needs_ref
def test_engine_finds_the_reference_rules():
    quran = loader.load_quran()
    from tools.phonemizer.phonemize import phonemize_range
    result = phonemize_range([a for s in quran.surahs for a in s.ayahs])
    stats = v.compare(v.load_reference(REF), v.our_annotations(quran, result))["stats"]
    floor = {"ikhfa": 0.98, "ikhfa_shafawi": 0.99, "iqlab": 0.99, "idghaam_ghunnah": 0.99,
             "idghaam_no_ghunnah": 0.98, "idghaam_shafawi": 0.99, "ghunnah": 0.99,
             "madd_2": 0.99, "madd_246": 0.99, "madd_muttasil": 0.99, "qalqalah": 0.99}
    for rule, minimum in floor.items():
        c = stats[rule]
        assert (c["ref_matched"] + c["ref_near"]) / c["ref"] >= minimum, rule
    # the one rule family that is genuinely different in labelling (vocative ya + hamza)
    assert 150 <= stats["madd_munfasil"]["ref_only"] <= 250
