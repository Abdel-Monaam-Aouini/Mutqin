# Training our own phoneme model

**On hold (owner decision, 2026-10-13):** training is skipped for now (no GPU); the project waits for
access to Muno459/zipformer_p-quran. The pipeline below stays in the repository so training can be
resumed without rework.

Status (2026-10-13): the whole pipeline exists and runs on CPU; real training needs a GPU and much
more audio. Decisions by the owner: EveryAyah audio for **research only** (the trained model stays
private until the audio license is cleared, see `LICENSES.md`); no GPU yet.

## Pipeline (all in `tools/train/`)

| Step | Tool | Notes |
|---|---|---|
| Download audio | `fetch_everyayah.py --reciter R --surahs 1,78-114` | polite delay, resumable; prints a `curl` command for any failure |
| Labels | `labels.py` | per-ayah phoneme sequence from the reference phonemizer (`cont` reading of each word); ayahs with disconnected letters (2:1, 19:1 ...) are left out; no basmala except 1:1 |
| Manifest + 16 kHz WAV | `build_manifest.py` | train/valid split **by reciter**; needs `ffmpeg` |
| Features | `features.py` | numpy log-mel identical to `core/src/features/logmel.cpp`; the parity test `FeaturesParity.CppEqualsNumpy` guards that |
| Model + training | `model.py`, `train.py` | conv x4 subsampling + Transformer encoder, 32 outputs (blank + 31 phonemes), CTC loss, greedy PER on the validation reciter |

Install: `pip install -r tools/requirements-train.txt` (torch, soundfile, numpy). Example:
`python3 -I tools/train/train.py --data data/train --out data/train/run1 --epochs 250 --d-model 192 --layers 4`.

## First CPU run (pipeline check, not a result)

Data: 3 reciters (Alafasy 128k, Husary 128k, Minshawy Murattal 128k), surah 1 and 105-114, 55 ayahs
each; train = 2 reciters (13.6 min), valid = the third.

| Model | Train PER | Valid PER (unseen reciter) | Time |
|---|---|---|---|
| 2.4M params, 60 epochs | 60% | 69% | 2 min |
| about 4M params, 250 epochs | 0.0% | 59.8% | 15 min |

The model memorises the 110 clips and does not generalise: 55 distinct ayahs and two voices cannot
teach acoustics. The point is that labels, features, loss and decoding work end to end.

## Second CPU run: one reciter, 3 hours (2026-10-13)

Data: Husary 128 kbps only (the owner chose one reciter), 314 train clips = 167 min from surahs
1, 2, 10, 60-63, 105-111, 114; validation = whole surahs 62, 64, 112, 113 (25 clips, 8 min).
The voice is **not** held out (there is only one), so even a good number would be optimistic about
new speakers.

| Model | Train PER | Valid PER | Time |
|---|---|---|---|
| 0.64M params, 40 epochs (about 2,700 steps) | 88.7% | 87.4% | 47 min on 16 CPU cores |

The loss plateaued near 2.5 and the model had barely started to align. A small encoder trained
from scratch needs far more steps than this CPU can give in a reasonable time, so further CPU runs
of this kind are not worth it. The next real step is the pretrained-encoder fine-tuning
(`finetune_xlsr.py`) on a GPU.

## What real training needs

- **Audio:** about 100-200 hours (a full-Quran murattal recording is roughly 15-20 hours per
  reciter, so 8-10 reciters), several recitation styles, speakers held out for validation.
- **Start from a pretrained speech encoder** (for example a wav2vec2 / XLS-R checkpoint; check its
  license) and fine-tune with our 32-token CTC head; this needs far less labelled data than training
  from scratch. A from-scratch Zipformer / Conformer is the later, streaming, mobile-sized model.
- **Chosen encoder:** `facebook/wav2vec2-xls-r-300m` (Apache-2.0, 128 languages incl. Arabic;
  see `LICENSES.md`). At 300M parameters it is a **teacher**, far over the 100 MB mobile budget;
  the app model is a small streaming student distilled from it (or trained on its pseudo-labels).
  `tools/train/finetune_xlsr.py` fine-tunes it with the 32-token CTC head (feature encoder frozen,
  fp16 on GPU). Its own front end (raw waveform, 50 frames/s) differs from the app's log-mel, which is fine
  for a teacher but is why the student, not the teacher, is what ships.
- **Streaming:** the app needs a causal / chunked encoder; this prototype is non-streaming.
- **Hardware:** one rented 24 GB GPU for hours to days; the script already takes `--device cuda`.
- **Targets:** valid PER well under 12% on held-out reciters before the model is worth wiring into
  the C++ core (the reference candidate Muno459/zipformer_p-quran reports 11.63%).
- **Label quality:** labels come from the provisional rules, so training also bakes in any rule
  error (docs/DRAFT_ORTHOGRAPHY_TABLES.md); a teacher review remains necessary.
