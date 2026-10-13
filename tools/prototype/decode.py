"""CTC greedy decoding: frames -> recognised tokens with confidence and time."""

from __future__ import annotations

from dataclasses import dataclass

from tools.phonemizer.inventory import VOCAB
from tools.prototype.acoustic_sim import Frame


@dataclass(frozen=True)
class Token:
    symbol: str
    confidence: float  # mean probability over the frames of the token
    start: int  # first frame
    end: int  # one past the last frame


def greedy_decode(frames: list[Frame]) -> list[Token]:
    """Collapse runs of the same label and drop blanks (label 0)."""
    tokens: list[Token] = []
    i = 0
    while i < len(frames):
        label = frames[i][0]
        j = i
        while j < len(frames) and frames[j][0] == label:
            j += 1
        if label != 0:
            probs = [p for _, p in frames[i:j]]
            tokens.append(Token(VOCAB[label], sum(probs) / len(probs), i, j))
        i = j
    return tokens
