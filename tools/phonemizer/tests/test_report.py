from html.parser import HTMLParser

import pytest

from tools.phonemizer import report
from tools.quran_text import loader
from tools.quran_text.download import CHECKSUMS_NAME

needs_data = pytest.mark.skipif(
    not (loader.DEFAULT_DATA_DIR / CHECKSUMS_NAME).exists(), reason="data/quran/ missing"
)


class Counter(HTMLParser):
    def __init__(self):
        super().__init__()
        self.stack, self.tags = [], {}
        self.errors = []

    def handle_starttag(self, tag, attrs):
        self.tags[tag] = self.tags.get(tag, 0) + 1
        if tag not in ("meta", "br"):
            self.stack.append(tag)

    def handle_endtag(self, tag):
        if not self.stack or self.stack.pop() != tag:
            self.errors.append(tag)


@pytest.fixture(scope="module")
def quran():
    return loader.load_quran()


@needs_data
def test_parse_ranges(quran):
    assert len(report.parse_ranges(["1"], quran)) == 7
    assert len(report.parse_ranges(["2:1-50"], quran)) == 50
    assert [(a.surah, a.ayah) for a in report.parse_ranges(["112:2", "113:1-2"], quran)] == [
        (112, 2), (113, 1), (113, 2)]
    assert len(report.parse_ranges(["112-114"], quran)) == 15


@needs_data
def test_report_is_well_formed_and_complete(quran):
    ayahs = report.parse_ranges(["1"], quran)
    page = report.build_report(quran, ayahs)
    parser = Counter()
    parser.feed(page)
    assert parser.errors == [] and parser.stack == []
    assert parser.tags["tr"] == 1 + sum(1 + len(a.words) for a in ayahs) - 1  # one header row per ayah
    assert "PROVISIONAL" in page and "tanzil.net" in page
    for ayah in ayahs:
        for word in ayah.words:
            assert word.id in page and word.text in page


@needs_data
def test_report_shows_both_readings_and_flags_unhandled_words(quran):
    page = report.build_report(quran, report.parse_ranges(["2:1", "11:41"], quran))
    assert "not handled: disconnected_letters" in page  # 2:1:1 letter names
    assert "long_a_without_fatha" in page  # 11:41:6
    assert "<th>Continue</th><th>Stop</th>" in page


@needs_data
def test_cli_writes_the_file(tmp_path):
    out = tmp_path / "r.html"
    assert report.main(["1", "-o", str(out)]) == 0
    assert out.stat().st_size > 5000
    with pytest.raises(SystemExit):
        report.main([])
