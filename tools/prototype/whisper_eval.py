"""Task 2.1 helper: transcribe a recording with a Whisper model and score it against the verified text.

The recording must be a correct recitation of known ayahs; every difference is then an error of
the model (a false alarm for Mutqin). The reference is read from data/quran/, never typed.
Usage: python3 -I tools/prototype/whisper_eval.py WAV --surah N --ayahs A-B [--seconds S] [--model DIR]
The transcript is cached next to the WAV (.transcript.txt).
WAV is 16 kHz mono (e.g. ffmpeg -i in.mp3 -ac 1 -ar 16000 out.wav).
Dev-only: needs torch + transformers.
"""

from __future__ import annotations

import argparse
import sys
import time
import unicodedata
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.quran_text.loader import load_quran  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ALEF_FORMS = str.maketrans("أإآٱ", "اااا")


def normalize(text: str) -> list[str]:
    """Words without diacritics, with alef / yeh / teh marbuta variants merged."""
    text = unicodedata.normalize("NFD", text)
    text = "".join(c for c in text if unicodedata.category(c) != "Mn" and c != "ـ")
    text = text.translate(ALEF_FORMS).replace("ى", "ي").replace("ة", "ه")
    return text.split()


def align_words(ref: list[str], hyp: list[str]) -> tuple[int, list[tuple[str, str, str]]]:
    """Edit distance with a free start and end on the reference (the recording is a part of it)."""
    n, m = len(hyp), len(ref)
    cost = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        cost[i][0] = i
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            cost[i][j] = min(
                cost[i - 1][j] + 1, cost[i][j - 1] + 1, cost[i - 1][j - 1] + (hyp[i - 1] != ref[j - 1])
            )
    j = min(range(m + 1), key=lambda jj: cost[n][jj])
    total, i, ops = cost[n][j], n, []
    while i > 0:
        if j > 0 and cost[i][j] == cost[i - 1][j - 1] + (hyp[i - 1] != ref[j - 1]):
            ops.append(("ok" if hyp[i - 1] == ref[j - 1] else "sub", ref[j - 1], hyp[i - 1]))
            i, j = i - 1, j - 1
        elif j > 0 and cost[i][j] == cost[i][j - 1] + 1:
            ops.append(("del", ref[j - 1], ""))
            j -= 1
        else:
            ops.append(("ins", "", hyp[i - 1]))
            i -= 1
    ops.reverse()
    return total, ops


def transcribe(wav: Path, model_dir: Path, seconds: int) -> list[str]:
    import soundfile as sf
    import torch
    from transformers import WhisperForConditionalGeneration, WhisperProcessor

    proc = WhisperProcessor.from_pretrained(model_dir)
    model = WhisperForConditionalGeneration.from_pretrained(model_dir).eval()
    prompt_tokens = ["<|startoftranscript|>", "<|ar|>", "<|transcribe|>", "<|notimestamps|>"]
    prompt = torch.tensor([proc.tokenizer.convert_tokens_to_ids(prompt_tokens)])
    audio, rate = sf.read(str(wav), dtype="float32")
    if rate != 16000:
        raise SystemExit("WAV must be 16 kHz mono")
    audio = audio[: seconds * rate]
    texts = []
    for start in range(0, len(audio), 30 * rate):
        feats = proc(audio[start : start + 30 * rate], sampling_rate=rate, return_tensors="pt").input_features
        t = time.time()
        with torch.no_grad():
            ids = model.generate(input_features=feats, decoder_input_ids=prompt, max_new_tokens=220)
        texts.append(proc.batch_decode(ids, skip_special_tokens=True)[0])
        print(f"chunk {start // rate}s done in {time.time() - t:.1f}s", file=sys.stderr, flush=True)
    return texts


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("wav", type=Path)
    ap.add_argument("--surah", type=int, required=True)
    ap.add_argument("--ayahs", required=True, help="reference range, e.g. 1-60 (longer than the recording)")
    ap.add_argument("--seconds", type=int, default=600)
    ap.add_argument("--model", type=Path, default=ROOT / "models" / "whisper-base-ar-quran")
    args = ap.parse_args()
    first, _, last = args.ayahs.partition("-")
    quran = load_quran(ROOT / "data" / "quran")
    ref: list[str] = []
    for a in range(int(first), int(last or first) + 1):
        ref += normalize(quran.ayah(args.surah, a).simple_text)  # modern spelling, like the model output
    cache = args.wav.with_suffix(".transcript.txt")
    if cache.exists():
        texts = cache.read_text(encoding="utf-8").splitlines()
    else:
        texts = transcribe(args.wav, args.model, args.seconds)
        cache.write_text("\n".join(texts) + "\n", encoding="utf-8")
    hyp: list[str] = []
    for text in texts:
        hyp += normalize(text)
    total, ops = align_words(ref, hyp)
    n_ref = sum(1 for o in ops if o[0] != "ins")
    kinds = {k: sum(1 for o in ops if o[0] == k) for k in ("ok", "sub", "del", "ins")}
    rate = total / max(1, n_ref)
    print(f"heard words {len(hyp)}, reference words covered {n_ref}, errors {total} -> {rate:.1%}")
    print(kinds)
    for kind, r, h in ops:
        if kind != "ok":
            print(kind, "ref=", r, "heard=", h)
    return 0


if __name__ == "__main__":
    sys.exit(main())
