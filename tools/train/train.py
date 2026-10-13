"""Train the CTC phoneme model on a manifest from build_manifest.py (CPU for a pipeline test, GPU for real).

Usage: python3 -I tools/train/train.py --data data/train --out data/train/run1 [--epochs 30]
         [--d-model 128 --layers 3] [--batch-seconds 120] [--lr 1e-3] [--device cpu|cuda]
Prints train loss and the phoneme error rate (PER, greedy decoding) on the validation reciters each epoch.
"""

from __future__ import annotations

import argparse
import json
import random
import sys
import time
from pathlib import Path

import numpy as np
import soundfile as sf
import torch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.train.features import log_mel  # noqa: E402
from tools.train.model import CtcModel, ModelConfig  # noqa: E402


def load_split(data: Path, split: str) -> list[dict]:
    path = data / f"{split}.jsonl"
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]


def features_of(row: dict) -> np.ndarray:
    cache = Path(row["wav"]).with_suffix(".mel.npy")
    if cache.exists():
        return np.load(cache)
    audio, rate = sf.read(row["wav"], dtype="float32")
    assert rate == 16000
    feats = log_mel(audio)
    np.save(cache, feats)
    return feats


def edit_distance(a: list[int], b: list[int]) -> int:
    prev = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        cur = [i]
        for j, y in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (x != y)))
        prev = cur
    return prev[-1]


def greedy(logprobs: torch.Tensor, length: int) -> list[int]:
    ids = logprobs[:length].argmax(-1).tolist()
    return [t for i, t in enumerate(ids) if t != 0 and (i == 0 or t != ids[i - 1])]


def batches(rows: list[dict], batch_seconds: float, shuffle: bool, rng: random.Random):
    order = sorted(rows, key=lambda r: r["seconds"])
    groups, cur, total = [], [], 0.0
    for r in order:
        if cur and total + r["seconds"] > batch_seconds:
            groups.append(cur)
            cur, total = [], 0.0
        cur.append(r)
        total += r["seconds"]
    if cur:
        groups.append(cur)
    if shuffle:
        rng.shuffle(groups)
    return groups


def collate(
    rows: list[dict], mean: np.ndarray, std: np.ndarray, device: str, augment: bool, rng: random.Random
):
    feats = [(features_of(r) - mean) / std for r in rows]
    if augment:  # light SpecAugment: one frequency band and one time band per clip
        for f in feats:
            f0 = rng.randrange(0, 70)
            f[:, f0 : f0 + rng.randrange(0, 10)] = 0.0
            if len(f) > 60:
                t0 = rng.randrange(0, len(f) - 40)
                f[t0 : t0 + rng.randrange(0, 30)] = 0.0
    lengths = torch.tensor([len(f) for f in feats])
    x = torch.zeros(len(feats), int(lengths.max()), feats[0].shape[1])
    for i, f in enumerate(feats):
        x[i, : len(f)] = torch.from_numpy(f)
    labels = torch.cat([torch.tensor(r["labels"]) for r in rows])
    label_lengths = torch.tensor([len(r["labels"]) for r in rows])
    return x.to(device), lengths.to(device), labels, label_lengths


def evaluate(model: CtcModel, rows: list[dict], mean, std, device: str) -> float:
    model.eval()
    errors = total = 0
    with torch.no_grad():
        for group in batches(rows, 60.0, False, random.Random(0)):
            x, lengths, _, _ = collate(group, mean, std, device, False, random.Random(0))
            logp, out_len = model(x, lengths)
            for i, r in enumerate(group):
                errors += edit_distance(greedy(logp[i], int(out_len[i])), r["labels"])
                total += len(r["labels"])
    return errors / max(1, total)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--epochs", type=int, default=30)
    ap.add_argument("--d-model", type=int, default=128)
    ap.add_argument("--layers", type=int, default=3)
    ap.add_argument("--batch-seconds", type=float, default=120.0)
    ap.add_argument("--lr", type=float, default=1e-3)
    ap.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    torch.manual_seed(args.seed)
    rng = random.Random(args.seed)
    train, valid = load_split(args.data, "train"), load_split(args.data, "valid")
    allf = np.concatenate([features_of(r) for r in train])
    mean, std = allf.mean(0), allf.std(0) + 1e-5
    config = ModelConfig(d_model=args.d_model, layers=args.layers, heads=4, ff=4 * args.d_model)
    model = CtcModel(config).to(args.device)
    print(
        f"{sum(p.numel() for p in model.parameters()) / 1e6:.2f}M parameters, device {args.device}, "
        f"{len(train)} train / {len(valid)} valid clips"
    )
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-2)
    ctc = torch.nn.CTCLoss(blank=0, zero_infinity=True)
    args.out.mkdir(parents=True, exist_ok=True)
    steps = args.epochs * len(batches(train, args.batch_seconds, False, rng))
    sched = torch.optim.lr_scheduler.OneCycleLR(
        opt, max_lr=args.lr, total_steps=max(steps, 2), pct_start=0.15
    )
    for epoch in range(1, args.epochs + 1):
        model.train()
        started, loss_sum, n = time.time(), 0.0, 0
        for group in batches(train, args.batch_seconds, True, rng):
            x, lengths, labels, label_lengths = collate(group, mean, std, args.device, True, rng)
            logp, out_len = model(x, lengths)
            loss = ctc(logp.transpose(0, 1), labels, out_len.cpu(), label_lengths)
            opt.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 5.0)
            opt.step()
            sched.step()
            loss_sum += float(loss)
            n += 1
        if epoch % 5 == 0 or epoch == args.epochs:
            per_train = evaluate(model, train, mean, std, args.device)
            per_valid = evaluate(model, valid, mean, std, args.device)
            print(
                f"epoch {epoch:3d} loss {loss_sum / n:7.3f}  "
                f"PER train {per_train:6.1%} valid {per_valid:6.1%}  ({time.time() - started:.1f}s/epoch)",
                flush=True,
            )
        else:
            print(f"epoch {epoch:3d} loss {loss_sum / n:7.3f}  ({time.time() - started:.1f}s)", flush=True)
    torch.save(
        {"model": model.state_dict(), "config": config.__dict__, "mean": mean, "std": std},
        args.out / "model.pt",
    )
    print(f"saved {args.out / 'model.pt'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
