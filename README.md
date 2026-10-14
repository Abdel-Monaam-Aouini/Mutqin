# Mutqin (متقن)

An on-device Quran recitation checker. You recite, the engine listens, and it flags where your recitation
differs from the text: skipped, repeated, added or wrong words. Everything runs locally, with no network
connection and no cloud service at run time. The goal is to help learners reach *itqan*, precise mastery.

> **Status: early.** A C++ core and a command-line tool work end to end with a real recognition model, at word
> level. Letter- and tajweed-level feedback, the mobile apps and a teacher's review of the tajweed rules are
> still ahead (see [Roadmap](#roadmap)). Mutqin is not a substitute for a qualified teacher.

## How it works

```
microphone / WAV (16 kHz mono)
  -> streaming Zipformer CTC model (phoneme units, runs through sherpa-onnx)
  -> aligner: compares the units heard with the canonical units of the chosen ayah range
  -> events: skipped / wrong / repeated / added word, with its place in the Quran (surah:ayah:word)
```

Because the model outputs *what was actually said* (phonemes) and has no language model, it does not quietly
"correct" a slip back to the expected text the way a normal speech recogniser does.

## Results so far

Measured on real recitations (details and caveats in [`docs/EVAL_RESULTS.md`](docs/EVAL_RESULTS.md)):

| Check | Result |
|---|---|
| False alarms, 49 min of a correct recitation (surah 5, one reciter) | 0.9% of words flagged, 2.5% events (target: below 2% flagged) |
| False alarms, 339 single-ayah clips of a second reciter | 0.1% of words flagged |
| Mistakes injected into the real model output | skipped 100%, wrong 97%, repeated 99%, added 94% found |
| Time to report a mistake | about 1-2 s after it is spoken (the model looks ahead about 1 s) |

Not measured yet: learners' recitation, phone microphones, other recitation styles.

Speed on a desktop CPU (Release build, AMD Ryzen 7 5800H, one thread unless noted); real-time factor (RTF) is
processing time divided by audio time, lower is faster:

| Stage | Measurement | RTF |
|---|---|---|
| Log-mel features | 120 s of audio in 0.27 s | 0.0022 |
| Energy VAD | 120 s in 1.6 ms | 0.00001 |
| Aligner, 167 s recitation (700+ words), pushed every 250 ms | 0.77 s total | 0.0046 |
| sherpa-onnx + Zipformer (int8, 4 threads), 49 min of audio | 90 s | 0.031 |

Phone benchmarks come with the mobile apps.

## Quick start

`make help` lists the common tasks (`make build`, `make check`, `make demo`, `make live`, `make record` ...).
The steps below are what they run.

Requirements: a C++17 compiler, CMake 3.16+, Python 3.10+ (for the tools), `ffmpeg` to convert audio.

```
cmake -S . -B build && cmake --build build
python3 tools/quran_text/download.py            # verified Quran text from Tanzil (asks you to accept its terms)
./build/cli/mutqin-cli --surah 1 --ayah 1-7     # expected phonemes with rule tags
```

Without a recognition model you can replay simulated model output through the same pipeline:

```
python3 tools/prototype/make_posteriors.py /tmp/p.f32 --surah 1 --ayahs 1-7 --scenario skip
./build/cli/mutqin-cli --surah 1 --ayah 1-7 --posteriors /tmp/p.f32   # reports the skipped word
```

### With the real model

1. Get access to [Muno459/zipformer_p-quran](https://huggingface.co/Muno459/zipformer_p-quran) (gated, free
   use only, see [License](#license)) and download it into `models/zipformer_p-quran/`.
2. Download a [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx/releases) "shared" release (v1.13.8 was
   used) and build with it:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMUTQIN_WITH_SHERPA=ON \
      -DSHERPA_ONNX_DIR=/path/to/sherpa-onnx-v1.13.8-linux-x64-shared-no-tts
cmake --build build
python3 tools/zipformer/expected_units.py      # once: builds the expected-units table from the model's files
```

3. Create `models/zipformer_p-quran/mutqin_model.txt`:

```
model=quran_phoneme_zipformer.int8.onnx
tokens=tokens.txt
units=expected_units.tsv
collapse_runs=2
threads=4
```

4. Recite, live or from a recording (`make live`, `make record`, `make check-wav WAV=take.wav` do the same):

```
./build/cli/mutqin-cli --surah 1 --ayah 1-7 --model models/zipformer_p-quran/mutqin_model.txt --mic
./build/cli/mutqin-cli ... --strictness strict   # relaxed | standard (default) | strict
./build/cli/mutqin-cli --surah 1 --ayah 1-7 --model models/zipformer_p-quran/mutqin_model.txt --wav take.wav --timeline
```

Events look like `[ 12.3 s] skipped 1:2:3` (surah 1, ayah 2, word 3). Convert recordings to 16 kHz mono first:
`ffmpeg -i in.mp3 -ac 1 -ar 16000 out.wav`. Details of the manifest and the model-native units:
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

## Repository layout

| Path | What |
|---|---|
| `core/` | C++17 library `MutqinCore`: Quran text loader, rule engine, features, aligner, C API (`core/include/mutqin.h`) |
| `cli/` | `mutqin-cli`, a thin client of the C API (WAV, microphone, replay) |
| `tools/` | Python: Quran text download and verification, the reference rule engine, evaluation and tuning tools |
| `docs/` | plan, architecture, tajweed specification, evaluation results, licenses, task list |
| `android/`, `ios/` | placeholders for the future mobile apps |

| Document | Purpose |
|---|---|
| [`docs/PLAN.md`](docs/PLAN.md) | phased roadmap with acceptance criteria |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | components, C API (`mq_`), threading contract and limits |
| [`docs/TAJWEED_SPEC.md`](docs/TAJWEED_SPEC.md) | rule engine specification (Hafs 'an 'Asim) |
| [`docs/DATA_AND_MODEL.md`](docs/DATA_AND_MODEL.md) | datasets, models, evaluation approach |
| [`docs/EVAL_RESULTS.md`](docs/EVAL_RESULTS.md) | evaluation results and model comparisons |
| [`docs/APP_SPEC.md`](docs/APP_SPEC.md) | draft specification of the Android and iOS apps (for review) |
| [`docs/TRAINING.md`](docs/TRAINING.md) | the (paused) own-model training pipeline |
| [`docs/TASKS.md`](docs/TASKS.md) | task checklist, phase by phase |
| [`docs/LICENSES.md`](docs/LICENSES.md) | every third-party source, dataset and model with its license |

## Roadmap

- Done: Quran text loader with checksums, rule engine in Python and C++ (checked against each other over the
  whole Quran), audio front end, incremental aligner, C API, command-line tool, word-level checking with a
  real model.
- Next: report which letter or vowel is wrong inside a word; tajweed rules one by one, each only when it meets
  its quality bar; review of the rules by a qualified teacher.
- Later: Android and iOS apps on the unchanged C API.

## Development

`make venv` then `make check` runs everything below (lint, C++ tests, Python tests).

```
python3 -m venv .venv   # or: uv venv .venv
.venv/bin/pip install -r tools/requirements-dev.txt       # pytest, numpy, ruff, clang-format (pinned)
.venv/bin/ruff format tools && .venv/bin/ruff check tools
git ls-files '*.h' '*.cpp' | xargs .venv/bin/clang-format -i
cmake -S . -B build -DMUTQIN_WERROR=ON && cmake --build build && ctest --test-dir build --output-on-failure
.venv/bin/python -I -m pytest -q
```

GitHub Actions (`.github/workflows/ci.yml`) runs the same on every push and pull request: format and lint, Python
tests, and build + test on Linux (gcc, clang, AddressSanitizer + UBSan), macOS and Windows, plus the sherpa-onnx
backend on Linux. Dependabot updates the GitHub Actions and Python packages; a weekly job reports newer releases
of the pinned C++ dependencies (GoogleTest, miniaudio, sherpa-onnx).

**Quran text in CI.** `data/quran/*.txt` is not in the repository. The jobs download it from Tanzil only if the
repository Actions variable `TANZIL_TERMS_ACCEPTED=true` is set (Settings, Secrets and variables, Actions,
Variables), which records acceptance of the [Tanzil Terms of Use](https://tanzil.net/docs/Text_License). The
download must match the committed `data/quran/checksums.txt`. Without the variable, tests that need the text are
skipped with a warning. Contributions are welcome: please run the checks above first.

## Principles

- Quran text always comes from a verified source file (Tanzil) and is never typed by hand.
- Tajweed rules are never invented: unclear cases are listed as open questions in `docs/TAJWEED_SPEC.md`.
- When in doubt the engine prefers missing a mistake over a false alarm.
- Audio stays on the device.
- The tajweed rules must be reviewed by a qualified teacher (with ijazah) before release.

## License

The code in this repository is licensed under the [Apache License 2.0](LICENSE) (see also [`NOTICE`](NOTICE)).

Third-party material keeps its own license ([`docs/LICENSES.md`](docs/LICENSES.md)). In particular:

- The Quran text is from [Tanzil](https://tanzil.net) (CC BY 3.0 and the Tanzil Terms of Use) and is not
  included in this repository.
- The recognition model **Muno459/zipformer_p-quran** is **not** part of this repository and is **not** covered
  by the Apache License. It is released under the Quran-Lab No-Profit License 1.1
  ([unmodified text](licenses/NPL-1.1-zipformer.txt)): it may be used, and its outputs redistributed, only in
  applications that are free to end users (no sale, subscription, paywall or advertising), and anything derived
  from it must stay under the same license. Mutqin itself is meant to stay free to its users; anyone who builds
  an application with this model must meet those terms.
