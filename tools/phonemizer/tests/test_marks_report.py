import pytest

from tools.phonemizer import marks_report
from tools.quran_text import loader


@pytest.mark.skipif(not loader.data_available(), reason="data/quran/ missing")
def test_report_runs_and_mentions_every_target(capsys):
    marks_report.main()
    out = capsys.readouterr().out
    for code in ("06DF", "06E2", "06ED", "06E5", "06E6", "0671"):
        assert f"U+{code}" in out
