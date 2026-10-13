# Architecture

Monorepo: core, CLI, tools, and (later) the mobile apps all live in one repository.

## Pipeline

```
Audio source (16 kHz mono: WAV file or microphone, supplied by the CLI/app)
  → Ring buffer
  → VAD (speech / silence, pause detection for waqf)
  → Feature extraction (80-dim log-mel)
  → Acoustic model (streaming CTC, ONNX int8)  → per-frame phoneme/char posteriors
  → Aligner (against expected sequence from the rule engine)
  → Error detector (word / harakat / tajweed, confidence thresholds)
  → Event queue → C API → CLI (later: mobile UI)
```

The **rule engine** runs ahead of time per ayah range: Quran text → expected annotated
phoneme sequence (with waqf / continue variants).

## Where things stand

| Stage | State |
|---|---|
| Quran text, rule engine (phonemizer), word readings | done in C++, checked against the Python reference for the whole Quran |
| Ring buffer, log-mel features, energy VAD | done in C++ (`audio/`, `features/`, `vad/`) |
| Recognition model | **not chosen yet** (candidates and results in `EVAL_RESULTS.md`). Two plug-in shapes exist: CTC posteriors over a fixed 32-token vocabulary (blank + the 31 phonemes of `PHONEME_INVENTORY.md`), or phoneme tokens from the optional sherpa-onnx backend (see "Recognition backends") |
| CTC decoding, aligner, word detector | done in C++ (`align/`), same results as the Python prototype on generated cases |
| C API | expected phonemes, posteriors in, word statuses and events out (see below) |
| `mutqin-cli` | prints expected phonemes, reads WAV, replays recorded posteriors (`--posteriors`) |

Python is development-only (`tools/`); the apps will use only the C++ core through the C API.

## Folder layout

```
mutqin/
├── AGENTS.md, CLAUDE.md, README.md, START_PROMPT.md
├── .claude/               # rules, skills, settings for Claude Code
├── docs/
├── data/                  # gitignored except small manifests + checksums
│   └── quran/             # Tanzil text files (read-only) + checksums.txt
├── models/                # exported ONNX models (git LFS or download script)
├── core/                  # C++17 shared core, library MutqinCore, namespace mutqin
│   ├── include/mutqin.h   # public C API, prefix mq_ (only public header)
│   ├── src/
│   │   ├── text/          # Quran loader, data model
│   │   ├── tajweed/       # phonemizer + rule engine (port of Python reference)
│   │   ├── audio/         # ring buffer, resampler
│   │   ├── vad/
│   │   ├── features/      # log-mel
│   │   ├── inference/     # sherpa-onnx / ONNX Runtime wrapper
│   │   ├── align/         # streaming CTC alignment, GOP scoring
│   │   ├── detect/        # error detectors
│   │   └── api/           # C API implementation
│   ├── tests/
│   └── CMakeLists.txt
├── cli/                   # mutqin-cli: WAV reader, posterior replay, prints events (uses the C API)
├── tools/                 # Python
│   ├── quran_text/        # download + verify text
│   ├── phonemizer/        # REFERENCE rule engine (source of truth)
│   ├── data/              # dataset prep, error injection
│   ├── train/             # model fine-tuning + ONNX export + quantization
│   ├── eval/              # metrics and evaluation sets
│   └── prototype/         # Phase 2 desktop prototype
├── android/               # DEFERRED: placeholder; later Kotlin + JNI + Oboe app
└── ios/                   # DEFERRED: placeholder; later Swift + AVAudioEngine app
```

Rules: `core/` contains no platform-specific code and builds on Linux, macOS, Windows.
Microphone capture lives in `cli/` (and later in the apps), never in `core/`; the core only
receives samples (`mq_session_push_audio`) or model posteriors through the C API.

## C API (`core/include/mutqin.h`)

Implemented now (plain C, `int` return codes `MQ_OK` / `MQ_ERR_*`, error text in a caller buffer):

```c
mq_engine_create(data_dir, &engine, err, err_len);      /* loads + verifies the Quran text */
mq_session_create(engine, surah, ayah_first, ayah_last, &session, err, err_len);
mq_session_phoneme_count / mq_session_phoneme(...)      /* expected phonemes with rule tags */
mq_session_push_posteriors(session, probs, num_frames); /* MQ_VOCAB_SIZE floats per frame */
mq_session_push_audio(session, samples, n);             /* 16 kHz mono; needs a model, else MQ_ERR_NO_MODEL */
mq_session_word_count / mq_session_word_status(...)     /* MQ_WORD_OK / WRONG / SKIPPED */
mq_session_event_count / mq_session_event(...)          /* skipped, wrong, repeated, added */
```

Models plug in through `AcousticModel` (`core/src/inference/acoustic_model.h`); no real backend exists yet
(tests use a scripted fake). Still planned: the sherpa-onnx backend and a way for the C API to select it,
a configuration struct (model directory, confidence threshold, madd preference), harakah and
tajweed events with character spans and timestamps, `mq_reset`.

## Recognition backends

A model plugs into a session in one of two shapes (`core/src/inference/`):

- `AcousticModel`: log-mel frames in, CTC posteriors out (full confidences; used by tests and by
  a future ONNX Runtime backend).
- `TokenRecognizer`: raw audio in, recognised phoneme tokens with timestamps out. The
  **sherpa-onnx backend** (`sherpa_recognizer.cpp`, Apache-2.0, optional) is this shape: its C API
  returns tokens and times but no probabilities, so every token has confidence 1.0.

Build it with `-DMUTQIN_WITH_SHERPA=ON -DSHERPA_ONNX_DIR=<extracted sherpa-onnx release>`; without
the switch `mq_session_load_model` explains that the backend is not built. A model is loaded from a
manifest (`mutqin_model.txt`, `key=value` lines, paths relative to it):

```
model=model.int8.onnx        # streaming Zipformer2 CTC
tokens=tokens.txt
token_map=token_map.tsv      # "<model unit><TAB><our phoneme symbols>", one line per unit
threads=2
```

`token_map.tsv` maps each of the model's units to one or more of our 31 symbols (a long vowel unit
becomes the vowel; a geminate unit may become two symbols); units not in the map are dropped. It
has to be written against the real model's `tokens.txt` and checked by someone who knows its units.
Tested with a public Chinese streaming model only (plumbing, real-time factor 0.04 on the dev CPU).

## Threading

- Audio callback (CLI/app): only copies samples into a lock-free ring buffer.
- Worker thread: VAD → features → inference → alignment → events.
- UI / CLI main thread: polls events (e.g. every 50 ms) through the C API.

## Alignment strategy

- Keep a window of expected tokens around the current position (e.g. current ayah ± 1).
- Streaming CTC alignment; when the user jumps (repeats or skips), re-anchor using a
  local search over the window.
- Pauses from VAD decide whether the waqf or continue variant of the expected sequence
  applies at word boundaries.
