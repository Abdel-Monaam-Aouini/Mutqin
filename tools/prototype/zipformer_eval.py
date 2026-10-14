"""Run the Quran phoneme Zipformer (Muno459/zipformer_p-quran, NPL-1.1, not in the repo) on a correct
recitation and count the false alarms of the aligner against the model's own canonical phonemes.

The model emits 250 units; `ordered_quran_phonemes.json` (shipped with it) holds the canonical units of
every word of every ayah. A professional recitation is correct, so every flagged word is a false alarm.
Usage: python3 -I tools/prototype/zipformer_eval.py WAV --surah N --ayahs A-B [--model-dir DIR] [--diag]
Dev-only: needs the sherpa-onnx Python wheel (tools/requirements-train.txt). WAV is 16 kHz mono.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.prototype.aligner import Params, align_tokens  # noqa: E402
from tools.prototype.decode import Token  # noqa: E402
from tools.prototype.phoneme_data import WordPhonemes  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def tokenize(text: str, units: set[str]) -> list[str]:
    """Greedy longest-match segmentation into model units (as the model card describes)."""
    longest = max(len(u) for u in units)
    out: list[str] = []
    i = 0
    while i < len(text):
        for length in range(min(longest, len(text) - i), 0, -1):
            if text[i : i + length] in units:
                out.append(text[i : i + length])
                i += length
                break
        else:
            raise ValueError(f"unit not in the model vocabulary: {text[i]!r}")
    return out


def expected_words(model_dir: Path, surah: int, first: int, last: int) -> list[WordPhonemes]:
    units = {u for u in json.loads((model_dir / "phoneme_units.json").read_text("utf-8")) if u != "<blank>"}
    ordered = json.loads((model_dir / "ordered_quran_phonemes.json").read_text("utf-8"))
    words = []
    for ayah in range(first, last + 1):
        for index, phonemes in enumerate(ordered[f"{surah}:{ayah}"]["aya_phonemes_list"], 1):
            cont = tuple(tokenize(phonemes, units))
            words.append(WordPhonemes((surah, ayah, index), cont, (cont, cont, cont, cont)))
    return words


def recognise(model_dir: Path, wav: Path) -> tuple[list[str], list[float], float]:
    import sherpa_onnx
    import soundfile as sf

    recognizer = sherpa_onnx.OnlineRecognizer.from_zipformer2_ctc(
        tokens=str(model_dir / "tokens.txt"),
        model=str(model_dir / "quran_phoneme_zipformer.int8.onnx"),
        num_threads=4,
        sample_rate=16000,
        feature_dim=80,
        decoding_method="greedy_search",
        enable_endpoint_detection=False,
    )
    audio, rate = sf.read(str(wav), dtype="float32")
    if rate != 16000:
        raise SystemExit("WAV must be 16 kHz mono")
    stream = recognizer.create_stream()
    started = time.time()
    for at in range(0, len(audio), 3200):
        stream.accept_waveform(16000, audio[at : at + 3200])
        while recognizer.is_ready(stream):
            recognizer.decode_stream(stream)
    stream.input_finished()
    while recognizer.is_ready(stream):
        recognizer.decode_stream(stream)
    result = recognizer.get_result_all(stream)
    return list(result.tokens), list(result.timestamps), (time.time() - started) / (len(audio) / 16000)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("wav", type=Path)
    ap.add_argument("--surah", type=int, required=True)
    ap.add_argument("--ayahs", required=True, help="reference range that covers the recording, e.g. 1-120")
    ap.add_argument("--model-dir", type=Path, default=ROOT / "models" / "zipformer_p-quran")
    ap.add_argument("--diag", action="store_true", help="print expected and heard units of flagged words")
    args = ap.parse_args()
    first, _, last = args.ayahs.partition("-")
    words = expected_words(args.model_dir, args.surah, int(first), int(last or first))
    tokens, times, rtf = recognise(args.model_dir, args.wav)
    heard = [Token(t, 1.0, round(s * 100), round(s * 100) + 4) for t, s in zip(tokens, times, strict=True)]
    result = align_tokens(heard, words, Params())
    flagged = [w for w, s in result.statuses.items() if s != "ok"]
    judged = len(result.statuses)
    print(f"{len(tokens)} units heard, real-time factor {rtf:.3f}, expected words {len(words)}")
    rate = len(flagged) / max(1, judged)
    print(f"judged words {judged}, flagged {len(flagged)} -> false alarm rate {rate:.1%}")
    print(f"events: {dict(Counter(e.kind for e in result.events))}  ({len(result.events)} in total)")
    if args.diag:
        flat = [(wi, unit) for wi, wp in enumerate(words) for unit in wp.cont]
        heard_of: dict[int, list[str]] = {}
        for op in result.ops:
            if op[0] == "diag":
                heard_of.setdefault(flat[op[2]][0], []).append(tokens[op[1]])
        for w in flagged:
            print(
                result.statuses[w], words[w].key, " ".join(words[w].cont), "|", " ".join(heard_of.get(w, []))
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
