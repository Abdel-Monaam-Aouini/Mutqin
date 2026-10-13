"""Simulated acoustic model: spoken phonemes -> frame-level model output, with a chosen error rate.

Stands in for a real CTC model so the aligner can be measured before any model exists. Each frame
is (label, probability); label 0 is the CTC blank. A phoneme becomes 2..5 frames, with blanks
between phonemes and pauses. Errors follow a target phoneme error rate (per): 60% substitutions,
25% deletions, 15% insertions. With informative_confidence a wrong token gets a lower confidence
than a right one, as in a real model, which is what lets the aligner stay quiet on doubtful
tokens; without it the confidence is independent of correctness (the pessimistic case).
"""

from __future__ import annotations

import random

from tools.phonemizer.inventory import SYMBOLS, TOKEN_ID

Frame = tuple[int, float]  # (label id, probability of that label)

VOWELS = ("a", "i", "u")
CONSONANTS = tuple(s for s in SYMBOLS if s not in VOWELS)


def _confusable(rng: random.Random, symbol: str) -> str:
    pool = VOWELS if symbol in VOWELS else CONSONANTS
    return rng.choice([s for s in pool if s != symbol])


def simulate_frames(
    rng: random.Random,
    tokens: list[str],
    per: float = 0.11,
    pauses_after: set[int] | None = None,
    informative_confidence: bool = True,
) -> list[Frame]:
    """Frames for a token sequence. `pauses_after` holds token indices followed by a silence."""
    pauses_after = pauses_after or set()
    frames: list[Frame] = []
    ins, dele = per * 0.15, per * 0.25  # the rest of `per` is substitutions
    for idx, tok in enumerate(tokens):
        r = rng.random()
        emit: list[tuple[str, bool]] = []  # (symbol, is_error)
        if r < ins:  # an extra token before the right one
            emit += [(rng.choice(SYMBOLS), True), (tok, False)]
        elif r < ins + dele:
            pass  # deletion: the token is not heard
        elif r < per:
            emit.append((_confusable(rng, tok), True))  # substitution
        else:
            emit.append((tok, False))
        for symbol, wrong in emit:
            n = rng.randint(2, 5)
            if informative_confidence:
                lo, hi = (0.35, 0.8) if wrong else (0.7, 0.99)
            else:  # worst case: a model whose confidence says nothing about correctness
                lo, hi = 0.45, 0.99
            frames += [(TOKEN_ID[symbol], rng.uniform(lo, hi)) for _ in range(n)]
            frames += [(0, rng.uniform(0.8, 0.99)) for _ in range(rng.randint(0, 2))]
        if idx in pauses_after:
            frames += [(0, rng.uniform(0.9, 0.99)) for _ in range(rng.randint(8, 25))]
    return frames
