"""Teacher review report (task 1.11): ayah, words, phonemes, rules and durations as one HTML page.

Open the file in a browser and print it to PDF. The Quran text is read from the verified Tanzil
files (never typed); the page carries the Tanzil copyright notice and a PROVISIONAL banner.

Usage:
    python3 -I tools/phonemizer/report.py RANGE [RANGE...] [-o OUT.html]
    RANGE = SURAH | SURAH-SURAH | SURAH:AYAH | SURAH:AYAH-AYAH      e.g.  1   78-114   2:1-50
    python3 -I tools/phonemizer/report.py --validation-set     (Al-Fatiha, Juz' 'Amma, Al-Baqarah 1-50)
"""

from __future__ import annotations

import argparse
import html
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.phonemizer.inventory import Variant  # noqa: E402
from tools.phonemizer.phonemize import Item, phonemize_range, select_reading, word_key  # noqa: E402
from tools.quran_text.loader import load_quran  # noqa: E402
from tools.quran_text.model import Ayah, Quran  # noqa: E402

DEFAULT_OUT = Path(__file__).resolve().parents[2] / "build" / "phonemizer_report.html"

TANZIL_NOTICE = (
    "Tanzil Quran Text (Uthmani, Version 1.1). Copyright (C) 2007-2026 Tanzil Project. "
    "License: Creative Commons Attribution 3.0. Source: https://tanzil.net. "
    "The text is shown unchanged."
)

CSS = """
body{font-family:system-ui,sans-serif;margin:24px;color:#111;background:#fff}
.banner{border:2px solid #b45309;background:#fffbeb;padding:10px 14px;margin-bottom:18px}
h2{margin:26px 0 6px;border-bottom:1px solid #999}
table{border-collapse:collapse;width:100%;margin-bottom:8px}
th,td{border:1px solid #bbb;padding:5px 7px;vertical-align:top;text-align:left}
th{background:#f3f4f6}
td.ar{font-family:'Amiri','Scheherazade New',serif;font-size:1.7em;direction:rtl;text-align:right}
.ph{display:inline-block;margin:1px 2px;padding:1px 5px;border-radius:4px;background:#e5e7eb;
font-family:monospace}
.ph sub{font-size:.7em}
.ph.rule{background:#dbeafe}
.ph.alt{background:#fde68a}
.diag{color:#b91c1c;font-size:.9em}
.notes{width:18%}
footer{margin-top:30px;font-size:.8em;color:#444;border-top:1px solid #999;padding-top:8px}
@media print{.banner{break-inside:avoid}tr{break-inside:avoid}}
"""

LEGEND = [
    ("n:2/ikhfa", "a phoneme symbol, `:` its allowed durations in counts, tags are its rules"),
    ("blue", "the phoneme carries at least one tajweed rule tag (hover to read the tags)"),
    (
        "yellow",
        "the phoneme exists only in this reading (a variant: e.g. only when continuing, "
        "or only at a stop, or only when starting)",
    ),
    ("Continue", "reading on into the next word"),
    ("Stop", "stopping at the end of this word (waqf)"),
]


def fmt_item(item: Item, in_other: set[int] | None = None) -> str:
    label = html.escape(item.ph)
    if item.dur:
        label += "<sub>:" + "/".join(str(d) for d in item.dur) + "</sub>"
    classes = ["ph"]
    if item.rules:
        classes.append("rule")
    if item.variant is not Variant.BOTH:
        classes.append("alt")
    title = html.escape(
        ", ".join(item.rules + [f"variant: {item.variant.value}"])
        if (item.rules or item.variant is not Variant.BOTH)
        else ""
    )
    return f'<span class="{" ".join(classes)}" title="{title}">{label}</span>'


def parse_ranges(specs: list[str], quran: Quran) -> list[Ayah]:
    ayahs: list[Ayah] = []
    for spec in specs:
        if ":" in spec:
            surah_s, rest = spec.split(":", 1)
            a1_s, _, a2_s = rest.partition("-")
            surah, a1, a2 = int(surah_s), int(a1_s), int(a2_s or a1_s)
            ayahs += [quran.ayah(surah, a) for a in range(a1, a2 + 1)]
        else:
            lo, _, hi = spec.partition("-")
            for surah in range(int(lo), int(hi or lo) + 1):
                ayahs += list(quran.surahs[surah - 1].ayahs)
    return ayahs


def build_report(quran: Quran, ayahs: list[Ayah]) -> str:
    result = phonemize_range(ayahs)
    cont = select_reading(result.items)
    stop = select_reading(result.items, all_stops=True)
    by_word: dict[tuple[int, int, int], tuple[list[Item], list[Item]]] = {}
    for k, reading in ((0, cont), (1, stop)):
        for item in reading:
            by_word.setdefault(word_key(item), ([], []))[k].append(item)
    diags: dict[str, list[str]] = {}
    for d in result.diagnostics:
        diags.setdefault(d.word_id, []).append(d.kind)

    out = [
        "<!doctype html><html lang='en'><head><meta charset='utf-8'>",
        "<meta name='viewport' content='width=device-width,initial-scale=1'>",
        "<title>Mutqin phonemizer report</title>",
        f"<style>{CSS}</style></head><body>",
        "<div class='banner'><b>PROVISIONAL, NOT TEACHER-VERIFIED.</b> The rules follow "
        "docs/TAJWEED_SPEC.md and docs/DRAFT_ORTHOGRAPHY_TABLES.md. Please mark every word that is "
        "wrong in the notes column. Phoneme symbols: docs/PHONEME_INVENTORY.md.</div>",
        "<h1>Mutqin: expected phonemes for review</h1>",
        "<details open><summary>How to read</summary><ul>",
    ]
    out += [f"<li><b>{html.escape(k)}</b>: {html.escape(v)}</li>" for k, v in LEGEND]
    out.append("</ul></details>")

    for ayah in ayahs:
        out.append(f"<h2>Surah {ayah.surah}, ayah {ayah.ayah}</h2>")
        out.append(
            "<table><tr><th>Word</th><th>Text</th><th>Continue</th><th>Stop</th>"
            "<th class='notes'>Teacher notes</th></tr>"
        )
        for word in ayah.words:
            key = (ayah.surah, ayah.ayah, word.index)
            c, s = by_word.get(key, ([], []))
            text = word.text + "".join(" " + m for m in word.marks)
            flags = ""
            if word.id in diags:
                flags = "<div class='diag'>not handled: " + html.escape(", ".join(diags[word.id])) + "</div>"
            out.append(
                f"<tr><td>{word.id}</td><td class='ar'>{html.escape(text)}</td>"
                f"<td>{''.join(fmt_item(i) for i in c) or '&mdash;'}{flags}</td>"
                f"<td>{''.join(fmt_item(i) for i in s) or '&mdash;'}</td><td></td></tr>"
            )
        out.append("</table>")
    out.append(f"<footer>{html.escape(TANZIL_NOTICE)}</footer></body></html>")
    return "\n".join(out)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("ranges", nargs="*")
    parser.add_argument(
        "--validation-set", action="store_true", help="Al-Fatiha, Juz' 'Amma (surahs 78-114), Al-Baqarah 1-50"
    )
    parser.add_argument("-o", "--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args(argv)
    specs = list(args.ranges)
    if args.validation_set:
        specs += ["1", "2:1-50", "78-114"]
    if not specs:
        parser.error("give at least one range or --validation-set")
    quran = load_quran()
    ayahs = parse_ranges(specs, quran)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(build_report(quran, ayahs), encoding="utf-8")
    print(f"wrote {args.out} ({len(ayahs)} ayahs)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
