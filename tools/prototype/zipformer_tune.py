"""Tune the aligner on REAL Zipformer output of a correct recitation (see zipformer_eval.py --save-heard).

False alarms: the recording is tiled into windows of W words; every flagged word or extra event is false.
Recall: one mistake is injected into the real heard units of a window (skipped: the word's units removed;
wrong: replaced by the units of another word; repeated: the previous two words heard again; added: units
of an unrelated word inserted) and the aligner must report it within one word.
Usage: python3 -I tools/prototype/zipformer_tune.py HEARD.json --surah N --ayahs A-B [--trials 100]
"""

from __future__ import annotations

import argparse
import json
import random
import re
import sys
from dataclasses import replace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.prototype.aligner import Params, align_tokens, edit_distance_wild  # noqa: E402
from tools.prototype.decode import Token  # noqa: E402
from tools.prototype.phoneme_data import WordPhonemes  # noqa: E402
from tools.prototype.zipformer_eval import ROOT, expected_words  # noqa: E402

KINDS = ("skipped", "wrong", "repeated", "added")
EDGE = 3  # words at each end of a window that are not scored


def collapse(unit: str, keep: int) -> str:
    """Runs of the same character longer than `keep` are cut to `keep` (length-insensitive units)."""
    pattern = "(.)\\1{" + str(keep) + ",}"  # a character repeated more than `keep` times
    return re.sub(pattern, lambda m: m.group(1) * keep, unit) if keep else unit


def prepare(
    words: list[WordPhonemes], heard: list[Token], keep: int
) -> tuple[list[WordPhonemes], list[Token]]:
    if not keep:
        return words, heard
    new_words = []
    for w in words:
        cont = tuple(collapse(u, keep) for u in w.cont)
        new_words.append(WordPhonemes(w.key, cont, (cont, cont, cont, cont)))
    return new_words, [Token(collapse(t.symbol, keep), t.confidence, t.start, t.end) for t in heard]


def groups_of(heard: list[Token], words: list[WordPhonemes], params: Params, chunk: int) -> list[list[int]]:
    """Heard token indices per word, from alignments of consecutive chunks (the stream is long)."""
    result: list[list[int]] = [[] for _ in words]
    flat_of = [[] for _ in words]
    total = 0
    for wi, w in enumerate(words):
        total += len(w.cont)
        flat_of[wi] = total
    lo = 0
    while lo < len(words):
        hi = min(len(words), lo + chunk)
        # heard range for this chunk: by proportion of expected units (the first pass is only a guide)
        units_before = flat_of[lo - 1] if lo else 0
        a = int(units_before * len(heard) / total)
        b = int(flat_of[hi - 1] * len(heard) / total) + 40
        res = align_tokens(heard[max(0, a - 40) : b], words[lo:hi], params)
        flat = [(wi, u) for wi in range(lo, hi) for u in words[wi].cont]
        for op in res.ops:
            if op[0] == "diag":
                result[flat[op[2]][0]].append(max(0, a - 40) + op[1])
        lo = hi
    return result


def run_window(words, heard, params):
    return align_tokens(heard, words, params)


def false_alarms(words, heard, owner, params, window):
    judged = flagged = events = 0
    lo = 0
    while lo + window <= len(words):
        idx = [i for w in range(lo, lo + window) for i in owner[w]]
        if idx:
            res = run_window(words[lo : lo + window], heard[min(idx) : max(idx) + 1], params)
            inner = range(EDGE, window - EDGE)  # the window cuts the recording: ignore its edges
            judged += sum(1 for w in res.statuses if w in inner)
            flagged += sum(1 for w, s in res.statuses.items() if w in inner and s != "ok")
            events += sum(1 for e in res.events if e.index in inner)
        lo += window
    return judged, flagged, events


def different_word(rng, words, target, minimum=0.5):
    for _ in range(200):
        other = rng.choice(words)
        if other is target or not 3 <= len(other.cont) <= len(target.cont) + 3:
            continue
        distance = edit_distance_wild([Token(u, 1.0, 0, 0) for u in other.cont], target.cont, 0.0)
        if distance / max(len(target.cont), len(other.cont)) >= minimum:
            return other
    return None


def inject(rng, kind, words, heard, owner, g, lo, hi):
    """Heard window with one mistake at word g; returns (window words, window heard, truth index)."""
    idx = {w: owner[w] for w in range(lo, hi)}
    first = min(i for w in idx for i in idx[w])
    last = max(i for w in idx for i in idx[w])
    seg = list(heard[first : last + 1])
    mine = [i - first for i in idx[g]]
    if len(mine) < 3:
        return None
    start, end = min(mine), max(mine) + 1
    base = seg[end - 1].end
    if kind == "skipped":
        seg = seg[:start] + seg[end:]
    elif kind == "wrong":
        other = different_word(rng, words, words[g])
        if other is None:
            return None
        step = max(1, (seg[end - 1].start - seg[start].start) // max(1, len(other.cont)))
        fake = [
            Token(u, 1.0, seg[start].start + k * step, seg[start].start + k * step + 4)
            for k, u in enumerate(other.cont)
        ]
        seg = seg[:start] + fake + seg[end:]
    elif kind == "repeated":
        prev = idx.get(g - 1, [])
        if not prev:
            return None
        copy_from = [i - first for i in prev] + mine
        shift = base - seg[min(copy_from)].start + 8
        copy = [
            Token(seg[i].symbol, 1.0, seg[i].start + shift, seg[i].end + shift) for i in sorted(copy_from)
        ]
        seg = seg[:end] + copy + seg[end:]
        g = g  # the last repeated word is g itself
    elif kind == "added":
        other = different_word(rng, words, words[g])
        if other is None:
            return None
        fake = [Token(u, 1.0, base + 8 + 4 * k, base + 12 + 4 * k) for k, u in enumerate(other.cont)]
        seg = seg[:end] + fake + seg[end:]
    return words[lo:hi], seg, g - lo


def recall(words, heard, owner, params, window, trials, seed):
    out = {}
    for kind in KINDS:
        rng = random.Random(f"{seed}-{kind}")
        hit_exact = hit_any = extra = done = 0
        while done < trials:
            g = rng.randrange(window // 2 + 2, len(words) - window // 2 - 2)
            lo, hi = g - window // 2, g + window // 2
            made = inject(rng, kind, words, heard, owner, g, lo, hi)
            if made is None:
                continue
            w_words, w_heard, truth = made
            res = run_window(w_words, w_heard, params)
            inner = range(EDGE, window - EDGE)
            near = [e for e in res.events if abs(e.index - truth) <= 1]
            res_events = [e for e in res.events if e.index in inner]
            hit_exact += any(e.kind == kind for e in near)
            hit_any += bool(near)
            extra += len([e for e in res_events if e not in near])
            done += 1
        out[kind] = (hit_exact / trials, hit_any / trials, extra / trials)
    return out


def parse_variant(spec: str) -> tuple[Params, int]:
    """ "base" or "key=value;key=value" over the Params fields plus keep (run-length cap on units)."""
    fields: dict[str, float] = {}
    keep = 0
    if spec != "base":
        for part in spec.split(";"):
            key, _, value = part.partition("=")
            if key == "keep":
                keep = int(value)
            else:
                fields[key] = type(getattr(Params(), key))(value)
    return replace(Params(), **fields), keep


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("heard", type=Path)
    ap.add_argument("--surah", type=int, required=True)
    ap.add_argument("--ayahs", required=True)
    ap.add_argument("--model-dir", type=Path, default=ROOT / "models" / "zipformer_p-quran")
    ap.add_argument("--window", type=int, default=50)
    ap.add_argument("--trials", type=int, default=100)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--owner-cache", type=Path, required=True, help="per-word heard indices (computed once)")
    ap.add_argument(
        "--variants",
        default="base",
        help="comma list of: base | key=value;key=value (Params fields and keep)",
    )
    args = ap.parse_args()
    first, _, last = args.ayahs.partition("-")
    words0 = expected_words(args.model_dir, args.surah, int(first), int(last or first))
    saved = json.loads(args.heard.read_text("utf-8"))
    heard0 = [
        Token(t, 1.0, round(s * 100), round(s * 100) + 4)
        for t, s in zip(saved["tokens"], saved["timestamps"], strict=True)
    ]
    if args.owner_cache.exists():
        owner = json.loads(args.owner_cache.read_text())
    else:
        owner = groups_of(heard0, words0, Params(), len(words0))
        args.owner_cache.write_text(json.dumps(owner))
    print(f"{len(words0)} words, {len(heard0)} heard units, windows of {args.window}")
    for name in args.variants.split(","):
        params, keep = parse_variant(name)
        words, heard = prepare(words0, heard0, keep)
        judged, flagged, events = false_alarms(words, heard, owner, params, args.window)
        rec = recall(words, heard, owner, params, args.window, args.trials, args.seed)
        rate, event_rate = flagged / max(1, judged), events / max(1, judged)
        clean = f"flagged {flagged}/{judged} words = {rate:.1%}, events {events} = {event_rate:.1%}"
        print(f"[{name}] clean: {clean}")
        print(
            "   recall exact/any/extra-per-trial: "
            + "  ".join(f"{k} {v[0]:.0%}/{v[1]:.0%}/{v[2]:.2f}" for k, v in rec.items())
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
