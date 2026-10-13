"""Convert downloaded EveryAyah clips to 16 kHz mono WAV and write a training manifest (JSON lines).

Each line: {"id", "reciter", "surah", "ayah", "wav", "seconds", "labels"}; labels are CTC token ids
(tools/train/labels.py). Ayahs without labels are skipped. Splits are by reciter (a speaker never
appears in both train and validation). Needs ffmpeg on PATH.
Usage: python3 -I tools/train/build_manifest.py --audio data/audio/everyayah --out data/train
         [--valid-reciters NAME,NAME]
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.prototype.phoneme_data import build_word_phonemes  # noqa: E402
from tools.quran_text.loader import load_quran  # noqa: E402
from tools.train.labels import ayah_labels  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def manifest_line(reciter: str, surah: int, ayah: int, wav: Path, seconds: float, labels: list[int]) -> str:
    return json.dumps(
        {
            "id": f"{reciter}/{surah:03d}{ayah:03d}",
            "reciter": reciter,
            "surah": surah,
            "ayah": ayah,
            "wav": str(wav),
            "seconds": round(seconds, 3),
            "labels": labels,
        },
        ensure_ascii=False,
    )


def to_wav16k(src: Path, dst: Path) -> bool:
    dst.parent.mkdir(parents=True, exist_ok=True)
    cmd = ["ffmpeg", "-loglevel", "error", "-y", "-i", str(src), "-ac", "1", "-ar", "16000", str(dst)]
    return subprocess.run(cmd, check=False).returncode == 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--audio", type=Path, default=ROOT / "data" / "audio" / "everyayah")
    ap.add_argument("--out", type=Path, default=ROOT / "data" / "train")
    ap.add_argument("--valid-reciters", default="", help="comma separated; default: the last reciter")
    args = ap.parse_args()
    quran = load_quran(ROOT / "data" / "quran")
    words = build_word_phonemes(quran)
    reciters = sorted(p.name for p in args.audio.iterdir() if p.is_dir())
    if not reciters:
        raise SystemExit(f"no reciter folders in {args.audio}; run tools/train/fetch_everyayah.py first")
    valid = set(filter(None, args.valid_reciters.split(","))) or {reciters[-1]}
    args.out.mkdir(parents=True, exist_ok=True)
    lines: dict[str, list[str]] = {"train": [], "valid": []}
    skipped = 0
    for reciter in reciters:
        for mp3 in sorted((args.audio / reciter).glob("*.mp3")):
            surah, ayah = int(mp3.stem[:3]), int(mp3.stem[3:])
            labels = ayah_labels(quran, words, surah, ayah)
            wav = args.out / "wav16k" / reciter / f"{mp3.stem}.wav"
            if labels is None or not (wav.exists() or to_wav16k(mp3, wav)):
                skipped += 1
                continue
            seconds = max(0, wav.stat().st_size - 44) / 32000
            lines["valid" if reciter in valid else "train"].append(
                manifest_line(reciter, surah, ayah, wav, seconds, labels)
            )
    for split, rows in lines.items():
        (args.out / f"{split}.jsonl").write_text("\n".join(rows) + ("\n" if rows else ""), encoding="utf-8")
        print(f"{split}: {len(rows)} clips")
    print(f"skipped {skipped} clips (no labels or conversion failed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
