"""Fine-tune facebook/wav2vec2-xls-r-300m (Apache-2.0) with a 32-token phoneme CTC head: the TEACHER model.

Uses the same manifest and labels as train.py (blank = 0 plus the 31 phonemes). Input is the raw 16 kHz
waveform (the model's own front end), output is 50 frames per second. Run on a GPU:
  python3 -I tools/train/finetune_xlsr.py --data data/train --out data/train/xlsr1 --epochs 20
Smoke test on CPU: add --max-steps 2 --batch-seconds 10. Needs torch, transformers, soundfile.
"""

from __future__ import annotations

import argparse
import random
import sys
import time
from pathlib import Path

import soundfile as sf
import torch
from transformers import Wav2Vec2ForCTC

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.train.model import VOCAB_SIZE  # noqa: E402
from tools.train.train import batches, edit_distance, greedy, load_split  # noqa: E402

MODEL_ID = "facebook/wav2vec2-xls-r-300m"


def load_audio(row: dict) -> torch.Tensor:
    audio, rate = sf.read(row["wav"], dtype="float32")
    assert rate == 16000
    x = torch.from_numpy(audio)
    return (x - x.mean()) / (x.std() + 1e-7)


def collate(rows: list[dict], device: str):
    waves = [load_audio(r) for r in rows]
    lengths = torch.tensor([len(w) for w in waves])
    x = torch.zeros(len(waves), int(lengths.max()))
    mask = torch.zeros(len(waves), int(lengths.max()), dtype=torch.long)
    for i, w in enumerate(waves):
        x[i, : len(w)] = w
        mask[i, : len(w)] = 1
    labels = torch.cat([torch.tensor(r["labels"]) for r in rows])
    label_lengths = torch.tensor([len(r["labels"]) for r in rows])
    return x.to(device), mask.to(device), lengths, labels, label_lengths


def evaluate(model, rows: list[dict], device: str) -> float:
    model.eval()
    errors = total = 0
    with torch.no_grad():
        for group in batches(rows, 60.0, False, random.Random(0)):
            x, mask, lengths, _, _ = collate(group, device)
            logits = model(x, attention_mask=mask).logits
            out_len = model._get_feat_extract_output_lengths(lengths)
            for i, r in enumerate(group):
                errors += edit_distance(greedy(logits[i].log_softmax(-1).cpu(), int(out_len[i])), r["labels"])
                total += len(r["labels"])
    return errors / max(1, total)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--epochs", type=int, default=20)
    ap.add_argument("--batch-seconds", type=float, default=100.0)
    ap.add_argument("--lr", type=float, default=3e-5)
    ap.add_argument("--max-steps", type=int, default=0, help="stop after N steps (smoke test)")
    ap.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    rng = random.Random(args.seed)
    torch.manual_seed(args.seed)
    train, valid = load_split(args.data, "train"), load_split(args.data, "valid")
    model = Wav2Vec2ForCTC.from_pretrained(
        MODEL_ID,
        vocab_size=VOCAB_SIZE,
        pad_token_id=0,
        ctc_loss_reduction="mean",
        ctc_zero_infinity=True,
        layerdrop=0.05,
        mask_time_prob=0.05,
    ).to(args.device)
    model.freeze_feature_encoder()
    model.gradient_checkpointing_enable()
    opt = torch.optim.AdamW([p for p in model.parameters() if p.requires_grad], lr=args.lr, weight_decay=1e-2)
    scaler = torch.amp.GradScaler(enabled=args.device == "cuda")
    args.out.mkdir(parents=True, exist_ok=True)
    step = 0
    for epoch in range(1, args.epochs + 1):
        model.train()
        started, loss_sum, n = time.time(), 0.0, 0
        for group in batches(train, args.batch_seconds, True, rng):
            x, mask, _, labels, label_lengths = collate(group, args.device)
            # the HF CTC loss takes padded labels, -100 on the padding
            padded = torch.full((len(group), int(label_lengths.max())), -100, dtype=torch.long)
            offset = 0
            for i, ln in enumerate(label_lengths.tolist()):
                padded[i, :ln] = labels[offset : offset + ln]
                offset += ln
            with torch.autocast(device_type=args.device.split(":")[0], enabled=args.device == "cuda"):
                loss = model(x, attention_mask=mask, labels=padded.to(args.device)).loss
            opt.zero_grad()
            scaler.scale(loss).backward()
            scaler.unscale_(opt)
            torch.nn.utils.clip_grad_norm_(model.parameters(), 5.0)
            scaler.step(opt)
            scaler.update()
            loss_sum += float(loss.detach())
            n += 1
            step += 1
            if args.max_steps and step >= args.max_steps:
                break
        per = evaluate(model, valid, args.device) if valid else float("nan")
        mean_loss = loss_sum / max(1, n)
        print(f"epoch {epoch:3d} loss {mean_loss:7.3f}  valid PER {per:6.1%}  ({time.time() - started:.0f}s)")
        model.save_pretrained(args.out)
        if args.max_steps and step >= args.max_steps:
            break
    print(f"saved {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
