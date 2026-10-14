"""False alarms of the Zipformer + aligner on a folder of single-ayah recordings (file names SSSAAA.wav).

The clips are correct recitations of one ayah each, so every flagged word or extra event is a false alarm.
Uses the tuned profile (edge_free=2, add_min_tokens=3, runs cut to 2) and the model's expected units.
Usage: python3 -I tools/prototype/zipformer_clips.py WAV_DIR [--model-dir DIR] [--limit N] [--cache FILE]
         [--variants "edge_free=2;add_min_tokens=3;repeat_cost=1.5;keep=2,..."]
With --cache the model output is stored once and later runs only repeat the (fast) alignment.
Dev-only: needs the sherpa-onnx Python wheel (tools/requirements-train.txt).
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.prototype.aligner import align_tokens  # noqa: E402
from tools.prototype.decode import Token  # noqa: E402
from tools.prototype.zipformer_eval import ROOT, expected_words  # noqa: E402
from tools.prototype.zipformer_tune import parse_variant, prepare  # noqa: E402


def recognise_all(files: list[Path], model_dir: Path) -> dict[str, dict]:
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
    out = {}
    for path in files:
        audio, _ = sf.read(str(path), dtype="float32")
        stream = recognizer.create_stream()
        stream.accept_waveform(16000, audio)
        stream.input_finished()
        while recognizer.is_ready(stream):
            recognizer.decode_stream(stream)
        result = recognizer.get_result_all(stream)
        out[path.stem] = {
            "tokens": list(result.tokens),
            "timestamps": list(result.timestamps),
            "seconds": len(audio) / 16000,
        }
    return out


def evaluate(heard_of: dict[str, dict], model_dir: Path, variant: str) -> None:
    params, keep = parse_variant(variant)
    judged = flagged = events = 0
    seconds = 0.0
    kinds: Counter[str] = Counter()
    with_event = 0
    for stem, saved in heard_of.items():
        surah, ayah = int(stem[:3]), int(stem[3:])
        words = expected_words(model_dir, surah, ayah, ayah)
        heard = [
            Token(t, 1.0, round(s * 100), round(s * 100) + 4)
            for t, s in zip(saved["tokens"], saved["timestamps"], strict=True)
        ]
        words, heard = prepare(words, heard, keep)
        res = align_tokens(heard, words, params)
        seconds += saved["seconds"]
        judged += len(res.statuses)
        flagged += sum(1 for s in res.statuses.values() if s != "ok")
        events += len(res.events)
        kinds.update(e.kind for e in res.events)
        with_event += bool(res.events)
    print(
        f"[{variant}] {len(heard_of)} clips, {seconds / 60:.0f} min, {judged} words: flagged {flagged} = "
        f"{flagged / max(1, judged):.1%}, events {events} = {events / max(1, judged):.1%} {dict(kinds)}; "
        f"clips with an event {with_event}"
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("wav_dir", type=Path)
    ap.add_argument("--model-dir", type=Path, default=ROOT / "models" / "zipformer_p-quran")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--cache", type=Path)
    ap.add_argument("--variants", default="edge_free=2;add_min_tokens=3;keep=2")
    args = ap.parse_args()
    if args.cache and args.cache.exists():
        heard_of = json.loads(args.cache.read_text("utf-8"))
    else:
        files = sorted(args.wav_dir.glob("*.wav"))[: args.limit or None]
        heard_of = recognise_all(files, args.model_dir)
        if args.cache:
            args.cache.write_text(json.dumps(heard_of, ensure_ascii=False))
    for variant in args.variants.split(","):
        evaluate(heard_of, args.model_dir, variant)
    return 0


if __name__ == "__main__":
    sys.exit(main())
