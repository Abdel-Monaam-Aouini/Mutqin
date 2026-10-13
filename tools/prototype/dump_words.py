"""Dump the per-word readings (cont + 4 contexts) of the whole Quran for the C++ parity test.

Usage: python3 -I tools/prototype/dump_words.py OUT_FILE DATA_DIR
Line format: "surah:ayah:word<TAB>cont|ctx0|ctx1|ctx2|ctx3" (symbols space separated).
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.prototype.phoneme_data import build_word_phonemes  # noqa: E402
from tools.quran_text.loader import load_quran  # noqa: E402


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    words = build_word_phonemes(load_quran(Path(sys.argv[2])))
    lines = []
    for (s, a, w), wp in words.items():
        parts = "|".join(" ".join(c) for c in (wp.cont, *wp.contexts))
        lines.append(f"{s}:{a}:{w}\t{parts}")
    Path(sys.argv[1]).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {len(lines)} words")
    return 0


if __name__ == "__main__":
    sys.exit(main())
