import json

import pytest

from tools.phonemizer import special_cases as sc
from tools.phonemizer.phonemize import phonemize_range
from tools.quran_text import loader
from tools.quran_text.download import CHECKSUMS_NAME

needs_data = pytest.mark.skipif(
    not (loader.DEFAULT_DATA_DIR / CHECKSUMS_NAME).exists(), reason="data/quran/ missing"
)


@pytest.fixture(scope="module")
def quran():
    return loader.load_quran()


@needs_data
def test_shipped_data_file_is_valid_and_still_empty(quran):
    assert sc.load_special_cases(quran) == {}


@needs_data
def test_valid_entry_loads(quran, tmp_path):
    path = tmp_path / "s.json"
    path.write_text(json.dumps({"kinds": list(sc.KINDS), "cases": {
        "75:27:2": {"kind": "sakt", "note": "n", "source": "teacher", "verified_by": None}}}))
    cases = sc.load_special_cases(quran, path)
    assert cases["75:27:2"].kind == "sakt" and cases["75:27:2"].verified_by is None


@needs_data
def test_unknown_word_or_kind_is_rejected(quran, tmp_path):
    path = tmp_path / "s.json"
    for cases, message in (({"1:1:99": {"kind": "sakt"}}, "no such word"),
                           ({"1:1:1": {"kind": "magic"}}, "unknown kind")):
        path.write_text(json.dumps({"kinds": list(sc.KINDS), "cases": cases}))
        with pytest.raises(sc.SpecialCasesError, match=message):
            sc.load_special_cases(quran, path)


@needs_data
def test_kinds_in_the_file_must_match_the_code(quran, tmp_path):
    path = tmp_path / "s.json"
    path.write_text(json.dumps({"kinds": ["sakt"], "cases": {}}))
    with pytest.raises(sc.SpecialCasesError, match="kinds"):
        sc.load_special_cases(quran, path)


@needs_data
def test_applying_a_case_tags_only_that_word(quran):
    ayah = quran.ayah(1, 2)
    result = phonemize_range([ayah])
    case = sc.SpecialCase("1:2:3", "sakt", "", "test", None)
    sc.apply_special_cases(result.items, {case.word_id: case})
    tagged = {i.word for i in result.items if "special_sakt" in i.rules}
    assert tagged == {3}


@needs_data
def test_candidates_point_at_the_locations_spec_3_9_lists(quran):
    result = phonemize_range([a for s in quran.surahs for a in s.ayahs])
    found = sc.candidates(result)
    assert found["sign U+06EA (imalah)"] == ["11:41:6"]
    assert found["sign U+06EB (ishmam)"] == ["12:11:6"]
    assert found["sign U+06EC (tashil)"] == ["41:44:9"]
    assert found["sign U+06DC (dual_reading)"] == ["2:245:14", "7:69:22"]
    assert found["sign U+06E3 (dual_reading)"] == ["52:37:7"]
    assert found["sign U+06E8 (other)"] == ["21:88:7"]
    assert len(found["diagnostic disconnected_letters"]) == 30
    assert found["diagnostic noon_sukun_before_non_izhar_letter"] == ["75:27:2"]
