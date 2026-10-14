"""Dump aligner test cases (heard tokens, expected words, Python result) for the C++ parity test.

Usage: python3 -I tools/prototype/dump_align.py OUT_FILE DATA_DIR [--per-case N]
Format (one token or word per line, floats as repr so they round-trip exactly):
  CASE id / P edge_free add_min_tokens wrong_below min_mismatch / W n
  / W: cont|ctx0|ctx1|ctx2|ctx3 (symbols space separated) / T n / T: symbol conf start end
  SPAN a b | SPAN none / S word status / E kind index conf / END
"""

from __future__ import annotations

import argparse
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.prototype.acoustic_sim import simulate_frames  # noqa: E402
from tools.prototype.aligner import Params, align_tokens, strictness_params  # noqa: E402
from tools.prototype.decode import greedy_decode  # noqa: E402
from tools.prototype.errors import KINDS, make_utterance, pick_range, render  # noqa: E402
from tools.prototype.phoneme_data import build_word_phonemes  # noqa: E402
from tools.quran_text.loader import load_quran  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("data_dir")
    ap.add_argument("--per-case", type=int, default=3)
    args = ap.parse_args()
    quran = load_quran(Path(args.data_dir))
    words = build_word_phonemes(quran)
    profiles = (
        Params(),
        strictness_params("relaxed"),
        strictness_params("standard"),
        strictness_params("strict"),
    )
    lines: list[str] = []
    case = 0
    for scenario in ("clean", *KINDS):
        for per in (0.05, 0.11, 0.2):
            for informative in (True, False):
                rng = random.Random(f"parity-{scenario}-{per}-{informative}")
                for _ in range(args.per_case):
                    expected = pick_range(rng, words, max_ayahs=3)
                    errors = () if scenario == "clean" else (scenario,)
                    while True:
                        try:
                            utt = make_utterance(rng, words, expected, errors)
                            break
                        except ValueError:
                            expected = pick_range(rng, words, max_ayahs=3)
                    tokens, pause_after = render(utt, words)
                    heard = greedy_decode(simulate_frames(rng, tokens, per, pause_after, informative))
                    wps = [words[k] for k in expected]
                    for params in profiles:  # the same input under both parameter profiles
                        result = align_tokens(heard, wps, params)
                        lines.append(f"CASE {case}")
                        case += 1
                        knobs = (
                            params.edge_free,
                            params.add_min_tokens,
                            repr(params.wrong_below),
                            params.min_mismatch,
                        )
                        lines.append("P " + " ".join(str(k) for k in knobs))
                        lines.append(f"W {len(wps)}")
                        for wp in wps:
                            lines.append("W: " + "|".join(" ".join(c) for c in (wp.cont, *wp.contexts)))
                        lines.append(f"T {len(heard)}")
                        for t in heard:
                            lines.append(f"T: {t.symbol} {t.confidence!r} {t.start} {t.end}")
                        span = "none" if result.span is None else f"{result.span[0]} {result.span[1]}"
                        lines.append("SPAN " + span)
                        for w in sorted(result.statuses):
                            lines.append(f"S {w} {result.statuses[w]}")
                        for e in result.events:
                            lines.append(f"E {e.kind} {e.index} {e.confidence!r}")
                        lines.append("END")
    Path(args.out).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {case} cases to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
