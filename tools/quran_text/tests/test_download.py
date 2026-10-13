"""Tests for download.py. Uses a fake fetcher with placeholder bytes: no network, no Quran text."""

from pathlib import Path

import pytest

from tools.quran_text import download


class FakeFetcher:
    def __init__(self) -> None:
        self.urls: list[str] = []

    def __call__(self, url: str) -> bytes:
        self.urls.append(url)
        return f"placeholder for {url}\n".encode()


def test_build_url_has_type_and_agree():
    url = download.build_url("uthmani")
    assert url.startswith(download.TANZIL_ENDPOINT + "?")
    assert "quranType=uthmani" in url
    assert "outType=txt-2" in url


def test_downloads_all_sources_and_writes_sha256sum_format(tmp_path: Path):
    fetch = FakeFetcher()
    result = download.download_all(tmp_path, fetch)
    assert set(result) == set(download.SOURCES)
    assert len(fetch.urls) == len(download.SOURCES)

    lines = (tmp_path / download.CHECKSUMS_NAME).read_text(encoding="utf-8").splitlines()
    assert len(lines) == len(download.SOURCES)
    for line in lines:
        digest, sep, name = line.partition("  ")
        assert sep == "  " and len(digest) == 64
        assert download.sha256_hex((tmp_path / name).read_bytes()) == digest


def test_existing_files_are_not_refetched_or_overwritten(tmp_path: Path):
    download.download_all(tmp_path, FakeFetcher())
    before = {p.name: p.read_bytes() for p in tmp_path.glob("*.txt") if p.name != "checksums.txt"}

    second = FakeFetcher()
    download.download_all(tmp_path, second)
    assert second.urls == []
    after = {p.name: p.read_bytes() for p in tmp_path.glob("*.txt") if p.name != "checksums.txt"}
    assert before == after


def test_modified_file_is_detected(tmp_path: Path):
    download.download_all(tmp_path, FakeFetcher())
    victim = tmp_path / next(iter(download.SOURCES))
    victim.write_bytes(victim.read_bytes() + b"x")
    with pytest.raises(RuntimeError, match="checksum mismatch"):
        download.download_all(tmp_path, FakeFetcher())


def test_empty_or_invalid_download_rejected(tmp_path: Path):
    with pytest.raises(RuntimeError, match="empty"):
        download.download_all(tmp_path, lambda url: b"")
    with pytest.raises(UnicodeDecodeError):
        download.download_all(tmp_path, lambda url: b"\xff\xfe\xfa")


def test_main_requires_terms_flag(tmp_path: Path, capsys):
    assert download.main(["--out-dir", str(tmp_path)]) == 2
    assert not list(tmp_path.iterdir())


def test_download_must_match_a_recorded_checksum(tmp_path: Path):
    download.download_all(tmp_path, FakeFetcher())
    recorded = (tmp_path / download.CHECKSUMS_NAME).read_text(encoding="utf-8")
    for name in download.SOURCES:
        (tmp_path / name).unlink()  # simulate a fresh CI checkout: only checksums.txt is committed

    changed = lambda url: b"upstream text changed\n"  # noqa: E731
    with pytest.raises(RuntimeError, match="does not match the recorded checksum"):
        download.download_all(tmp_path, changed)
    assert not any((tmp_path / name).exists() for name in download.SOURCES)  # nothing written
    assert (tmp_path / download.CHECKSUMS_NAME).read_text(encoding="utf-8") == recorded  # untouched

    download.download_all(tmp_path, FakeFetcher())  # the genuine bytes are accepted again
    assert all((tmp_path / name).exists() for name in download.SOURCES)
