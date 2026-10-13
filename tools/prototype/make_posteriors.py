"""Write simulated model output (posteriors) for a recitation, to replay in mutqin-cli --posteriors.

Usage: python3 -I tools/prototype/make_posteriors.py OUT --surah N --ayahs A-B
         [--scenario clean|skip|wrong|repeat|add] [--per 0.05] [--seed 1] [--data DIR]
OUT is raw little-endian float32, 32 values per frame (blank + 31 phonemes in contract order).
The acoustic model is simulated (tools/prototype/acoustic_sim.py), not a real one.
"""

from __future__ import annotations

import argparse
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.phonemizer.inventory import VOCAB  # noqa: E402
from tools.prototype.acoustic_sim import simulate_frames  # noqa: E402
from tools.prototype.errors import make_utterance, render  # noqa: E402
from tools.prototype.phoneme_data import build_word_phonemes  # noqa: E402
from tools.quran_text.loader import load_quran  # noqa: E402

SCENARIOS = {
    "clean": (),
    "skip": ("skipped",),
    "wrong": ("wrong",),
    "repeat": ("repeated",),
    "add": ("added",),
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("out", type=Path)
    ap.add_argument("--surah", type=int, required=True)
    ap.add_argument("--ayahs", required=True)
    ap.add_argument("--scenario", choices=sorted(SCENARIOS), default="clean")
    ap.add_argument("--per", type=float, default=0.05)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--data", type=Path, default=Path(__file__).resolve().parents[2] / "data" / "quran")
    args = ap.parse_args()
    first, _, last = args.ayahs.partition("-")
    words = build_word_phonemes(load_quran(args.data))
    expected = [k for k in words if k[0] == args.surah and int(first) <= k[1] <= int(last or first)]
    rng = random.Random(args.seed)
    utt = make_utterance(rng, words, expected, SCENARIOS[args.scenario])
    tokens, pause_after = render(utt, words)
    frames = simulate_frames(rng, tokens, args.per, pause_after, True)
    others = (1.0, len(VOCAB) - 1)
    with args.out.open("wb") as fh:
        for label, prob in frames:
            row = [(others[0] - prob) / others[1]] * len(VOCAB)
            row[label] = prob
            fh.write(struct.pack(f"<{len(VOCAB)}f", *row))
    for t in utt.truth:
        print(f"truth: {t.kind} at word index {t.index}")
    print(f"wrote {len(frames)} frames to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
