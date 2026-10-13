"""Download a subset of EveryAyah recordings for RESEARCH training (task: own model).

EveryAyah states no license (docs/LICENSES.md). Per the owner's decision (2026-10-13) the audio is
used for research and any model trained on it stays private until the license is cleared. Nothing
downloaded here is committed (data/ is gitignored).

Usage: python3 -I tools/train/fetch_everyayah.py --reciter Alafasy_128kbps --surahs 1,78-114
         [--out data/audio/everyayah] [--delay 0.3]
If a download fails, run the printed URL yourself (curl -O URL) and put the file in the folder.
"""

from __future__ import annotations

import argparse
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.quran_text.loader import load_quran  # noqa: E402

BASE = "https://everyayah.com/data"
ROOT = Path(__file__).resolve().parents[2]


def clip_url(reciter: str, surah: int, ayah: int) -> str:
    return f"{BASE}/{reciter}/{surah:03d}{ayah:03d}.mp3"


def parse_surahs(spec: str) -> list[int]:
    out: list[int] = []
    for part in spec.split(","):
        first, _, last = part.partition("-")
        out += range(int(first), int(last or first) + 1)
    if not out or any(not 1 <= s <= 114 for s in out):
        raise ValueError(f"bad surah list: {spec}")
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reciter", required=True, help="EveryAyah folder name, e.g. Alafasy_128kbps")
    ap.add_argument("--surahs", required=True)
    ap.add_argument("--out", type=Path, default=ROOT / "data" / "audio" / "everyayah")
    ap.add_argument("--delay", type=float, default=0.3, help="seconds between requests (be polite)")
    args = ap.parse_args()
    quran = load_quran(ROOT / "data" / "quran")
    folder = args.out / args.reciter
    folder.mkdir(parents=True, exist_ok=True)
    got = failed = 0
    for surah in parse_surahs(args.surahs):
        for ayah in range(1, len(quran.surahs[surah - 1].ayahs) + 1):
            target = folder / f"{surah:03d}{ayah:03d}.mp3"
            if target.exists() and target.stat().st_size > 0:
                continue
            url = clip_url(args.reciter, surah, ayah)
            try:
                with urllib.request.urlopen(url, timeout=30) as resp:
                    target.write_bytes(resp.read())
                got += 1
            except (urllib.error.URLError, TimeoutError) as err:
                failed += 1
                print(f"failed ({err}): curl -L -o {target} {url}", file=sys.stderr)
            time.sleep(args.delay)
    print(f"downloaded {got} clips to {folder}, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
