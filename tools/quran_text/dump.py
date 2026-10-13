"""Write the reference text dump compared by the C++ parity test (core/tests/test_parity.cpp).

Line formats (tab separated, LF):
    S <surah> <basmala tokens joined by space>
    A <surah>:<ayah> <simple-clean text>
    W <surah>:<ayah>:<word> <word text> <marks joined by space>

Usage: python3 -I tools/quran_text/dump.py OUT_FILE [DATA_DIR]
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))  # repo root (python -I drops it)

from tools.quran_text.loader import DEFAULT_DATA_DIR, load_quran  # noqa: E402
from tools.quran_text.model import Quran  # noqa: E402


def dump_tsv(quran: Quran) -> str:
    out: list[str] = []
    for surah in quran.surahs:
        out.append(f"S\t{surah.number}\t{' '.join(surah.basmala)}\n")
        for ayah in surah.ayahs:
            out.append(f"A\t{ayah.id}\t{ayah.simple_text}\n")
            for word in ayah.words:
                out.append(f"W\t{word.id}\t{word.text}\t{' '.join(word.marks)}\n")
    return "".join(out)


def main(argv: list[str]) -> int:
    if not 2 <= len(argv) <= 3:
        print(__doc__, file=sys.stderr)
        return 2
    data_dir = Path(argv[2]) if len(argv) == 3 else DEFAULT_DATA_DIR
    Path(argv[1]).write_bytes(dump_tsv(load_quran(data_dir)).encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
