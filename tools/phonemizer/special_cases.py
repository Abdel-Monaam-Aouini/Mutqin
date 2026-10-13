"""Location-based special cases (docs/TAJWEED_SPEC.md section 3.9), kept as data, never hardcoded.

data/special_cases.json maps "surah:ayah:word" to an entry:
    {"kind": <one of KINDS>, "note": str, "source": str, "verified_by": str | null}
The file is empty until it is filled together with a qualified teacher. `candidates()` lists
locations the text encoding or the engine's diagnostics point to, for the teacher to confirm; it
never changes the data file.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

from tools.phonemizer.phonemize import AyahResult, Item
from tools.quran_text.model import Quran

DATA_FILE = Path(__file__).resolve().parent / "data" / "special_cases.json"
KINDS = (
    "sakt",
    "imalah",
    "ishmam",
    "tashil",
    "dual_reading",
    "disconnected_letters",
    "izhar_mutlaq",
    "other",
)

# Signs that occur only at special-case locations (draft tables C), by code point.
LOCATION_SIGNS = {
    0x06EA: "imalah",
    0x06EB: "ishmam",
    0x06EC: "tashil",
    0x06DC: "dual_reading",
    0x06E3: "dual_reading",
    0x06E8: "other",
}


class SpecialCasesError(ValueError):
    pass


@dataclass(frozen=True)
class SpecialCase:
    word_id: str
    kind: str
    note: str
    source: str
    verified_by: str | None


def load_special_cases(quran: Quran, path: Path = DATA_FILE) -> dict[str, SpecialCase]:
    """Load and validate: every key must be an existing word id, every kind a known kind."""
    raw = json.loads(path.read_text(encoding="utf-8"))
    kinds = tuple(raw.get("kinds", ()))
    if kinds != KINDS:
        raise SpecialCasesError(f"kinds in {path.name} do not match KINDS in special_cases.py")
    result: dict[str, SpecialCase] = {}
    for word_id, entry in raw.get("cases", {}).items():
        try:
            quran.word(word_id)
        except (KeyError, IndexError, ValueError) as exc:
            raise SpecialCasesError(f"{word_id}: no such word") from exc
        if entry.get("kind") not in KINDS:
            raise SpecialCasesError(f"{word_id}: unknown kind {entry.get('kind')!r}")
        result[word_id] = SpecialCase(
            word_id, entry["kind"], entry.get("note", ""), entry.get("source", ""), entry.get("verified_by")
        )
    return result


def apply_special_cases(items: list[Item], cases: dict[str, SpecialCase]) -> None:
    """Tag every phoneme of a special-case word with `special_<kind>` (no phoneme changes yet:
    what each kind does to the sound is defined with the teacher)."""
    for item in items:
        case = cases.get(f"{item.surah}:{item.ayah}:{item.word}")
        if case is not None:
            item.rules.append(f"special_{case.kind}")


def candidates(result: AyahResult) -> dict[str, list[str]]:
    """Locations to confirm with the teacher: {reason: [word ids]}, from signs and diagnostics."""
    found: dict[str, set[str]] = {}
    for item in result.items:
        for cp in item.signs:
            if cp in LOCATION_SIGNS:
                found.setdefault(f"sign U+{cp:04X} ({LOCATION_SIGNS[cp]})", set()).add(
                    f"{item.surah}:{item.ayah}:{item.word}"
                )
    for d in result.diagnostics:
        if d.kind not in ("wasla_start_vowel_unknown",):
            found.setdefault(f"diagnostic {d.kind}", set()).add(d.word_id)
    key = lambda w: tuple(int(x) for x in w.split(":"))  # noqa: E731
    return {reason: sorted(ids, key=key) for reason, ids in sorted(found.items())}
