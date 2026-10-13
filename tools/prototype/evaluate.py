"""Task 2.6: measure the prototype with synthetic recitations and write docs/EVAL_RESULTS.md.

Correct recitations give the false-alarm rate (the most important number). Recitations with exactly
one injected mistake give recall per kind and the extra (false) events around it. The acoustic model
is simulated at a chosen phoneme error rate (per), so the table shows how accurate a real model
must be. Usage: python3 -I tools/prototype/evaluate.py [--trials N] [--seed S] [-o FILE]
"""

from __future__ import annotations

import argparse
import random
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.prototype.acoustic_sim import simulate_frames  # noqa: E402
from tools.prototype.aligner import Event, Params, align_tokens  # noqa: E402
from tools.prototype.decode import greedy_decode  # noqa: E402
from tools.prototype.errors import KINDS, TruthEvent, make_utterance, pick_range, render  # noqa: E402
from tools.prototype.phoneme_data import WordKey, WordPhonemes, build_word_phonemes  # noqa: E402
from tools.quran_text.loader import load_quran  # noqa: E402

DEFAULT_OUT = Path(__file__).resolve().parents[2] / "docs" / "EVAL_RESULTS.md"


@dataclass
class Counts:
    utterances: int = 0
    words: int = 0
    flagged_words: int = 0  # judged words reported as skipped / wrong
    utterances_with_alarm: int = 0
    hit_exact: int = 0
    hit_any: int = 0
    extra_events: int = 0
    kinds_missed: list[str] = field(default_factory=list)


def _near(event: Event, truth: TruthEvent) -> bool:
    return abs(event.index - truth.index) <= 1


def run_trial(
    rng: random.Random,
    words: dict[WordKey, WordPhonemes],
    per: float,
    params: Params,
    errors: tuple[str, ...],
    informative: bool = True,
) -> tuple[list[Event], list[TruthEvent], int, int]:
    expected = pick_range(rng, words, max_ayahs=3)
    while True:
        try:
            utt = make_utterance(rng, words, expected, errors)
            break
        except ValueError:
            expected = pick_range(rng, words, max_ayahs=3)
    tokens, pause_after = render(utt, words)
    frames = simulate_frames(rng, tokens, per, pause_after, informative)
    result = align_tokens(greedy_decode(frames), [words[k] for k in expected], params)
    flagged = sum(1 for s in result.statuses.values() if s != "ok")
    return result.events, utt.truth, len(result.statuses), flagged


def evaluate(
    words: dict[WordKey, WordPhonemes],
    per: float,
    params: Params,
    trials: int,
    seed: int,
    informative: bool = True,
) -> dict[str, Counts]:
    out: dict[str, Counts] = {}
    for scenario in ("clean", *KINDS):
        rng = random.Random(f"{seed}-{scenario}-{per}-{informative}")
        c = Counts()
        n = trials * 2 if scenario == "clean" else trials
        for _ in range(n):
            errors = () if scenario == "clean" else (scenario,)
            events, truth, judged, flagged = run_trial(rng, words, per, params, errors, informative)
            c.utterances += 1
            c.words += judged
            c.flagged_words += flagged
            matched = set()
            for ti, tr in enumerate(truth):
                if any(e.kind == tr.kind and _near(e, tr) for e in events):
                    c.hit_exact += 1
                    matched.add(ti)
                if any(_near(e, tr) for e in events):
                    c.hit_any += 1
            extras = [e for e in events if not any(_near(e, tr) for tr in truth)]
            c.extra_events += len(extras)
            if extras:
                c.utterances_with_alarm += 1
        out[scenario] = c
    return out


def render_table(per: float, informative: bool, res: dict[str, Counts]) -> str:
    clean = res["clean"]
    lines = [
        f"### Simulated phoneme error rate {per:.0%}, "
        + (
            "model confidence tells right from wrong tokens"
            if informative
            else "confidence says nothing (worst case)"
        ),
        "",
        f"False alarms on correct recitations: **{clean.flagged_words / clean.words:.2%} of words** "
        f"({clean.flagged_words}/{clean.words}); recitations with at least one false alarm: "
        f"**{clean.utterances_with_alarm / clean.utterances:.1%}** ({clean.utterances_with_alarm}/"
        f"{clean.utterances}).",
        "",
        "| Mistake | Trials | Found (right kind) | Found (any kind) | Extra false events per trial |",
        "|---|---|---|---|---|",
    ]
    for kind in KINDS:
        c = res[kind]
        lines.append(
            f"| {kind} | {c.utterances} | {c.hit_exact / c.utterances:.1%} | "
            f"{c.hit_any / c.utterances:.1%} | {c.extra_events / c.utterances:.2f} |"
        )
    lines.append("")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trials", type=int, default=150)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("-o", "--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--per", type=float, nargs="*", default=[0.05, 0.11, 0.20])
    args = parser.parse_args(argv)

    words = build_word_phonemes(load_quran())
    params = Params()
    sections = [
        "# Evaluation results (synthetic, prototype)",
        "",
        "Generated by `tools/prototype/evaluate.py`. **These numbers use a simulated acoustic model**",
        "(`tools/prototype/acoustic_sim.py`): they show how the aligner behaves for a given phoneme error",
        "rate and a stated assumption about its confidence scores, not how any real model performs.",
        "Real-model rows will be added when a model is chosen.",
        "",
        "Method: the recited phonemes (continuing or stopping reading per word, pauses as silence) are",
        "turned into simulated frames, decoded with greedy CTC, aligned to the expected words with",
        "word-level skip and repeat arcs, and judged per word (`tools/prototype/aligner.py`). A mistake",
        "counts as found when an event of the right kind appears within one word of it. Injected wrong",
        "and added words differ from their neighbours by at least half their phonemes; one-letter",
        "differences are Phase 4 (letter-level) errors.",
        "",
        f"Seed {args.seed}, {args.trials} trials per mistake kind, {args.trials * 2} correct recitations,",
        "ranges of 1 to 3 ayahs. Targets (docs/PLAN.md): false alarms < 2%, mistake recall > 90%.",
        f"Parameters: `{params}`.",
        "",
    ]
    for per in args.per:
        for informative in (True, False):
            res = evaluate(words, per, params, args.trials, args.seed, informative)
            sections.append(render_table(per, informative, res))
            print(sections[-1])
    args.out.write_text("\n".join(sections), encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
