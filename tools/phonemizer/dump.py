"""Reference dump of the phonemizer, compared byte for byte by the C++ parity test
(core/tests/test_phonemizer_parity.cpp).

Line formats (tab separated, LF):
    I <s>:<a>:<w> <ph> <span0>,<span1> <role> <dur joined by /> <variant> <rules joined by ,>
    D <word id> <kind> <offset>                     (diagnostics of the whole-Quran run)
    R <continue|stops|ayah_end> <item indices of that reading, comma separated>
    H <s>:<a> <sha256 of the I and D lines of that ayah phonemized on its own>

Usage: python3 -I tools/phonemizer/dump.py OUT_FILE [DATA_DIR]
"""

import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.phonemizer.phonemize import (  # noqa: E402
    AyahResult, Item, phonemize_ayah, phonemize_range, select_reading)
from tools.quran_text.loader import DEFAULT_DATA_DIR, load_quran  # noqa: E402


def item_line(i: Item) -> str:
    dur = "/".join(str(d) for d in i.dur)
    return (f"I\t{i.surah}:{i.ayah}:{i.word}\t{i.ph}\t{i.span[0]},{i.span[1]}\t{i.role}\t{dur}\t"
            f"{i.variant.value}\t{','.join(i.rules)}\n")


def dump_result(result: AyahResult) -> str:
    return ("".join(item_line(i) for i in result.items)
            + "".join(f"D\t{d.word_id}\t{d.kind}\t{d.offset}\n" for d in result.diagnostics))


def dump_parity(quran) -> str:
    ayahs = [a for s in quran.surahs for a in s.ayahs]
    result = phonemize_range(ayahs)
    out = [dump_result(result)]
    position = {id(it): k for k, it in enumerate(result.items)}
    ayah_ends = {(a.surah, a.ayah, len(a.words)) for a in ayahs}
    readings = {
        "continue": select_reading(result.items),
        "stops": select_reading(result.items, all_stops=True),
        "ayah_end": select_reading(result.items, stops=ayah_ends),
    }
    for name, items in readings.items():
        out.append(f"R\t{name}\t{','.join(str(position[id(i)]) for i in items)}\n")
    for ayah in ayahs:
        digest = hashlib.sha256(dump_result(phonemize_ayah(ayah)).encode("utf-8")).hexdigest()
        out.append(f"H\t{ayah.surah}:{ayah.ayah}\t{digest}\n")
    return "".join(out)


def main(argv: list[str]) -> int:
    if not 2 <= len(argv) <= 3:
        print(__doc__, file=sys.stderr)
        return 2
    data_dir = Path(argv[2]) if len(argv) == 3 else DEFAULT_DATA_DIR
    Path(argv[1]).write_bytes(dump_parity(load_quran(data_dir)).encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
