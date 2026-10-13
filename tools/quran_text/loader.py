"""Load and verify the Tanzil Quran text from data/quran/ (read-only).

Verifies SHA-256 against data/quran/checksums.txt on every load, parses the
"surah|ayah|text" lines, and builds the surah -> ayah -> word model. The source files are never
modified; all splitting happens in memory.

Parsing rules (derived from the Tanzil v1.1 files, see docs/DATA_AND_MODEL.md):
- Data lines come first; after them only blank lines and '#' lines (the copyright footer).
- Tanzil prefixes the basmala (4 tokens) to ayah 1 of every surah except 1 and 9. It is stored
  in Surah.basmala and not counted as words of the ayah. Surahs 95 and 97 carry one extra
  shaddah (U+0651) on the first basmala token compared with 1:1: this is accepted explicitly,
  nothing else is.
- Tokens made only of Quranic marks (waqf signs, sajdah sign...) are not words; they attach
  to the preceding word as Word.marks. The C++ loader must use the same code point ranges.
"""

from __future__ import annotations

import re
from pathlib import Path

from tools.quran_text.download import CHECKSUMS_NAME, SOURCES, read_checksums, sha256_hex
from tools.quran_text.model import Ayah, Quran, Surah, Word

UTHMANI_FILE = "quran-uthmani.txt"
SIMPLE_FILE = "quran-simple-clean.txt"
DEFAULT_DATA_DIR = Path(__file__).resolve().parents[2] / "data" / "quran"

EXPECTED_SURAHS = 114
EXPECTED_AYAHS = 6236
SURAHS_WITHOUT_BASMALA_PREFIX = (1, 9)
BASMALA_TOKENS = 4  # taken from 1:1 at load time; checked, not typed
SHADDAH = "ّ"
# Surahs whose first basmala token has one extra shaddah vs 1:1 in the Tanzil Uthmani text.
BASMALA_SHADDAH_SURAHS = (95, 97)

# Code point ranges of combining / annotation marks that never form a word on their own.
MARK_RANGES = ((0x0610, 0x061A), (0x064B, 0x065F), (0x06D6, 0x06ED))

_LINE = re.compile(r"^(\d+)\|(\d+)\|(.+)$")


class QuranDataError(RuntimeError):
    """Source files are missing, modified, or not in the expected shape."""


def data_available(data_dir: Path = DEFAULT_DATA_DIR) -> bool:
    """True if checksums.txt and every source file are present (tests skip otherwise)."""
    return all((data_dir / name).exists() for name in (CHECKSUMS_NAME, *SOURCES))


def is_mark_only(token: str) -> bool:
    return bool(token) and all(any(lo <= ord(c) <= hi for lo, hi in MARK_RANGES) for c in token)


def verify_checksums(data_dir: Path) -> None:
    recorded = read_checksums(data_dir / CHECKSUMS_NAME)
    if not recorded:
        raise QuranDataError(f"{data_dir / CHECKSUMS_NAME} missing or empty; run download.py")
    for name in SOURCES:
        path = data_dir / name
        if name not in recorded:
            raise QuranDataError(f"{name}: no checksum recorded")
        if not path.exists():
            raise QuranDataError(f"{name}: file missing; run download.py")
        if sha256_hex(path.read_bytes()) != recorded[name]:
            raise QuranDataError(f"{name}: checksum mismatch (file modified)")


def parse_lines(path: Path) -> dict[tuple[int, int], str]:
    """Parse one Tanzil txt file into {(surah, ayah): text}, in file order."""
    raw = path.read_bytes()
    if raw.startswith(b"\xef\xbb\xbf"):
        raise QuranDataError(f"{path.name}: unexpected BOM")
    result: dict[tuple[int, int], str] = {}
    in_footer = False
    for lineno, line in enumerate(raw.decode("utf-8").split("\n"), 1):
        match = _LINE.match(line)
        if match and not in_footer:
            key = (int(match.group(1)), int(match.group(2)))
            if key in result:
                raise QuranDataError(f"{path.name}:{lineno}: duplicate {key[0]}:{key[1]}")
            result[key] = match.group(3)
        elif line == "" or line.startswith("#"):
            in_footer = in_footer or bool(result)
        else:
            raise QuranDataError(f"{path.name}:{lineno}: unexpected line")
    return result


def _check_numbering(keys: list[tuple[int, int]], name: str) -> dict[int, int]:
    """Surahs must run 1..114 and ayahs 1..n in order. Returns {surah: ayah count}."""
    counts: dict[int, int] = {}
    for surah, ayah in keys:
        if surah not in counts:
            if surah != len(counts) + 1:
                raise QuranDataError(f"{name}: surah {surah} out of order")
            counts[surah] = 0
        elif surah != len(counts):
            raise QuranDataError(f"{name}: surah {surah} not contiguous")
        counts[surah] += 1
        if ayah != counts[surah]:
            raise QuranDataError(f"{name}: ayah {surah}:{ayah} out of order")
    return counts


def _split_basmala(surah: int, tokens: list[str], reference: list[str]) -> tuple[tuple[str, ...], list[str]]:
    """Split the basmala prefix off ayah 1, checking it against the 1:1 tokens."""
    if surah in SURAHS_WITHOUT_BASMALA_PREFIX:
        return (), tokens
    prefix, rest = tokens[:BASMALA_TOKENS], tokens[BASMALA_TOKENS:]
    expected = list(reference)
    if surah in BASMALA_SHADDAH_SURAHS:
        expected[0] = expected[0][:1] + SHADDAH + expected[0][1:]
    if prefix != expected or not rest:
        raise QuranDataError(f"{surah}:1: basmala prefix does not match 1:1")
    return tuple(prefix), rest


def _build_words(surah: int, ayah: int, tokens: list[str]) -> tuple[Word, ...]:
    words: list[Word] = []
    pending_marks: list[str] = []
    for token in tokens:
        if is_mark_only(token):
            if not words:
                raise QuranDataError(f"{surah}:{ayah}: mark token before the first word")
            pending_marks.append(token)
            continue
        if pending_marks:
            words[-1] = _with_marks(words[-1], pending_marks)
            pending_marks = []
        words.append(Word(surah, ayah, len(words) + 1, token))
    if pending_marks:
        words[-1] = _with_marks(words[-1], pending_marks)
    return tuple(words)


def _with_marks(word: Word, marks: list[str]) -> Word:
    return Word(word.surah, word.ayah, word.index, word.text, word.marks + tuple(marks))


def load_quran(data_dir: Path = DEFAULT_DATA_DIR) -> Quran:
    """Verify checksums, then load the Uthmani words and the simple-clean ayah text."""
    verify_checksums(data_dir)
    uthmani = parse_lines(data_dir / UTHMANI_FILE)
    simple = parse_lines(data_dir / SIMPLE_FILE)

    if list(uthmani) != list(simple):
        raise QuranDataError("Uthmani and simple-clean files list different ayahs")
    counts = _check_numbering(list(uthmani), UTHMANI_FILE)
    if len(counts) != EXPECTED_SURAHS or len(uthmani) != EXPECTED_AYAHS:
        raise QuranDataError(
            f"expected {EXPECTED_SURAHS} surahs / {EXPECTED_AYAHS} ayahs, got {len(counts)} / {len(uthmani)}"
        )

    reference = uthmani[(1, 1)].split(" ")[:BASMALA_TOKENS]
    surahs: list[Surah] = []
    by_id: dict[tuple[int, int], Ayah] = {}
    for number in range(1, EXPECTED_SURAHS + 1):
        ayahs: list[Ayah] = []
        basmala: tuple[str, ...] = ()
        for ayah in range(1, counts[number] + 1):
            tokens = uthmani[(number, ayah)].split(" ")
            if ayah == 1:
                basmala, tokens = _split_basmala(number, tokens, reference)
            obj = Ayah(number, ayah, _build_words(number, ayah, tokens), simple[(number, ayah)])
            ayahs.append(obj)
            by_id[(number, ayah)] = obj
        surahs.append(Surah(number, basmala, tuple(ayahs)))
    return Quran(tuple(surahs), by_id)
