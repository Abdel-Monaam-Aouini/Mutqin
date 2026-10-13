"""Print the expected phonemes of an ayah range, for manual testing. Output is ASCII only.

Usage: python3 -I tools/phonemizer/show.py SURAH:AYAH [SURAH:AYAH_TO]
Example: python3 -I tools/phonemizer/show.py 1:1 1:7

Each word shows its phonemes as symbol[:dur][#rule,...] with variant markers (c = continue only,
w = waqf only). Diagnostics list every case the provisional tables do not cover.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.phonemizer.inventory import Variant  # noqa: E402
from tools.phonemizer.phonemize import phonemize_ayah  # noqa: E402
from tools.quran_text.loader import load_quran  # noqa: E402


def fmt(p) -> str:
    s = p.ph
    if p.dur:
        s += ":" + "/".join(str(d) for d in p.dur)
    if p.rules:
        s += "#" + ",".join(p.rules)
    if p.variant is Variant.CONTINUE_ONLY:
        s += "(c)"
    elif p.variant is Variant.WAQF_ONLY:
        s += "(w)"
    return s


def main(argv: list[str]) -> int:
    if not 2 <= len(argv) <= 3:
        print(__doc__, file=sys.stderr)
        return 2
    s1, a1 = (int(x) for x in argv[1].split(":"))
    s2, a2 = (int(x) for x in (argv[2] if len(argv) == 3 else argv[1]).split(":"))
    quran = load_quran()
    for surah in range(s1, s2 + 1):
        first = a1 if surah == s1 else 1
        last = a2 if surah == s2 else len(quran.surahs[surah - 1].ayahs)
        for ayah in range(first, last + 1):
            result = phonemize_ayah(quran.ayah(surah, ayah))
            print(f"== {surah}:{ayah}")
            by_word: dict[int, list[str]] = {}
            for p in result.phonemes:
                by_word.setdefault(p.word, []).append(fmt(p))
            for word in quran.ayah(surah, ayah).words:
                print(f"  {word.id:>9}  " + " ".join(by_word.get(word.index, ["(none)"])))
            for d in result.diagnostics:
                print(f"  ! {d.word_id} {d.kind} @{d.offset}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
