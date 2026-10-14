"""Run the Quran phoneme Zipformer (Muno459/zipformer_p-quran, NPL-1.1, not in the repo) on a correct
recitation and count the false alarms of the aligner against the model's own canonical phonemes.

The model emits 250 units; its canonical units per group of words come from `expected_units.tsv`, built by
tools/zipformer/expected_units.py from the model's `ordered_quran_phonemes.json`. A professional
recitation is correct, so every flagged word is a false alarm.
Usage: python3 -I tools/prototype/zipformer_eval.py WAV --surah N --ayahs A-B [--model-dir DIR] [--diag]
Dev-only: needs the sherpa-onnx Python wheel (tools/requirements-train.txt). WAV is 16 kHz mono.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from collections import Counter
from dataclasses import replace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.prototype.aligner import Params, align_tokens  # noqa: E402
from tools.prototype.decode import Token  # noqa: E402
from tools.prototype.phoneme_data import WordPhonemes  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def expected_words(model_dir: Path, surah: int, first: int, last: int) -> list[WordPhonemes]:
    """Groups of the ayah range from expected_units.tsv (see tools/zipformer/expected_units.py)."""
    table = model_dir / "expected_units.tsv"
    if not table.exists():
        raise SystemExit(f"{table} is missing: run python3 -I tools/zipformer/expected_units.py")
    words = []
    for line in table.read_text("utf-8").splitlines():
        ayah_id, span, units = line.split("\t")
        s_num, a_num = (int(x) for x in ayah_id.split(":"))
        if s_num == surah and first <= a_num <= last:
            cont = tuple(units.split())
            first_word = int(span.split("-")[0])
            words.append(WordPhonemes((s_num, a_num, first_word), cont, (cont, cont, cont, cont)))
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
    ap.add_argument(
        "--save-heard", type=Path, help="write the heard units and times as JSON (for offline tuning)"
    )
    ap.add_argument(
        "--tuned", action="store_true", help="the C++ profile: edge_free=2, add_min_tokens=3, runs cut to 2"
    )
    ap.add_argument("--load-heard", type=Path, help="reuse saved heard units instead of running the model")
    args = ap.parse_args()
    first, _, last = args.ayahs.partition("-")
    words = expected_words(args.model_dir, args.surah, int(first), int(last or first))
    if args.load_heard:
        saved = json.loads(args.load_heard.read_text("utf-8"))
        tokens, times, rtf = saved["tokens"], saved["timestamps"], 0.0
    else:
        tokens, times, rtf = recognise(args.model_dir, args.wav)
        if args.save_heard:
            args.save_heard.write_text(
                json.dumps({"tokens": tokens, "timestamps": times}, ensure_ascii=False)
            )
    heard = [Token(t, 1.0, round(s * 100), round(s * 100) + 4) for t, s in zip(tokens, times, strict=True)]
    params = replace(Params(), edge_free=2, add_min_tokens=3) if args.tuned else Params()
    if args.tuned:
        from tools.prototype.zipformer_tune import prepare

        words, heard = prepare(words, heard, 2)
        tokens = [t.symbol for t in heard]
    result = align_tokens(heard, words, params)
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
