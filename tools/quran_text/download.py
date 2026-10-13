"""Download the Tanzil Quran text files into data/quran/ and record SHA-256 checksums.

Source: Tanzil Project, https://tanzil.net  (Quran text, version 1.1).
Terms of Use (https://tanzil.net/docs/Text_License): verbatim copies may be copied and
distributed, but the text must not be changed; the source (Tanzil Project) must be
indicated and linked to tanzil.net. By passing --agree-tanzil-terms you accept those terms.

The files are stored exactly as received. They are never modified or overwritten: once a
file exists it is only verified against checksums.txt.

Usage:
    python3 -I tools/quran_text/download.py --agree-tanzil-terms
"""

from __future__ import annotations

import argparse
import hashlib
import sys
import urllib.parse
import urllib.request
from collections.abc import Callable
from pathlib import Path

TANZIL_ENDPOINT = "https://tanzil.net/pub/download/index.php"
CHECKSUMS_NAME = "checksums.txt"

# local file name -> Tanzil quranType. Output format txt-2 = "surah|ayah|text" lines.
SOURCES: dict[str, str] = {
    "quran-uthmani.txt": "uthmani",
    "quran-simple-clean.txt": "simple-clean",
}

Fetcher = Callable[[str], bytes]


def build_url(quran_type: str) -> str:
    """URL for one Tanzil text file, using the site's default options (pause marks,
    sajdah signs and tatweel included, rub-el-hizb signs excluded)."""
    query = urllib.parse.urlencode(
        {
            "quranType": quran_type,
            "outType": "txt-2",
            "marks": "true",
            "sajdah": "true",
            "tatweel": "true",
            "agree": "true",
        }
    )
    return f"{TANZIL_ENDPOINT}?{query}"


def http_fetch(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "mutqin-text-downloader"})
    with urllib.request.urlopen(request, timeout=60) as response:  # noqa: S310 (fixed https URL)
        return response.read()


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_checksums(path: Path) -> dict[str, str]:
    """Parse a `sha256sum`-format file: '<hex>  <name>' per line."""
    if not path.exists():
        return {}
    result: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        digest, _, name = line.partition("  ")
        result[name.strip()] = digest.strip()
    return result


def write_checksums(path: Path, checksums: dict[str, str]) -> None:
    lines = [f"{digest}  {name}\n" for name, digest in sorted(checksums.items())]
    path.write_text("".join(lines), encoding="utf-8", newline="\n")


def download_all(out_dir: Path, fetch: Fetcher = http_fetch) -> dict[str, str]:
    """Fetch every source file not yet present, verify existing ones, write checksums.txt.

    Raises RuntimeError if an existing file does not match its recorded checksum, or a
    download is empty / not valid UTF-8. Existing files are never overwritten.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    checksums_path = out_dir / CHECKSUMS_NAME
    recorded = read_checksums(checksums_path)
    result: dict[str, str] = {}

    for name, quran_type in SOURCES.items():
        target = out_dir / name
        if target.exists():
            digest = sha256_hex(target.read_bytes())
            if name in recorded and recorded[name] != digest:
                raise RuntimeError(f"{name}: checksum mismatch (file changed since download)")
            result[name] = digest
            continue

        data = fetch(build_url(quran_type))
        if not data:
            raise RuntimeError(f"{name}: empty download")
        data.decode("utf-8")  # raises UnicodeDecodeError if not valid UTF-8
        digest = sha256_hex(data)
        if name in recorded and recorded[name] != digest:
            raise RuntimeError(f"{name}: downloaded file does not match the recorded checksum (not written)")
        target.write_bytes(data)
        result[name] = digest

    write_checksums(checksums_path, {**recorded, **result})
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--agree-tanzil-terms",
        action="store_true",
        help="I accept the Tanzil Terms of Use (https://tanzil.net/docs/Text_License)",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path(__file__).resolve().parents[2] / "data" / "quran",
    )
    args = parser.parse_args(argv)

    if not args.agree_tanzil_terms:
        print(
            "You must accept the Tanzil Terms of Use: https://tanzil.net/docs/Text_License\n"
            "Re-run with --agree-tanzil-terms",
            file=sys.stderr,
        )
        return 2

    try:
        result = download_all(args.out_dir)
    except Exception as exc:  # network, disk, or verification failure
        print(f"error: {exc}", file=sys.stderr)
        print(
            "If the network is blocked, run this on a machine with access and copy "
            "data/quran/ back:\n  python3 -I tools/quran_text/download.py --agree-tanzil-terms",
            file=sys.stderr,
        )
        return 1

    for name, digest in sorted(result.items()):
        print(f"{digest}  {name}")
    print(f"checksums written to {args.out_dir / CHECKSUMS_NAME}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
