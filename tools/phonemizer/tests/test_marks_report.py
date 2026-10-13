import pytest

from tools.phonemizer import marks_report
from tools.quran_text import loader
from tools.quran_text.download import CHECKSUMS_NAME


@pytest.mark.skipif(
    not (loader.DEFAULT_DATA_DIR / CHECKSUMS_NAME).exists(), reason="data/quran/ missing"
)
def test_report_runs_and_mentions_every_target(capsys):
    marks_report.main()
    out = capsys.readouterr().out
    for code in ("06DF", "06E2", "06ED", "06E5", "06E6", "0671"):
        assert f"U+{code}" in out
