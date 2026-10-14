# AGENTS.md — Instructions for AI coding assistants

## Project summary

**Mutqin (متقن)**: an on-device Quran recitation checker. The user recites; the engine follows
along and flags mistakes, helping them reach itqan (precise mastery). The expected text is always
known, so this is **verification** (alignment against a known reference), not open speech
recognition.

**Monorepo, current scope: C++ core + CLI only.** The Android and iOS apps will live in this
same repo (`android/`, `ios/`) but are deferred; those folders are placeholders.

Error levels, built in this order:
1. **Word level**: skipped, wrong, repeated, or added words (hifz / memorization mode).
2. **Harakat level**: wrong vowels (fatha/kasra/damma/sukun), wrong letters.
3. **Tajweed level**: ghunnah, madd lengths, qalqalah, idgham, ikhfa, iqlab, tafkhim/tarqiq.

Riwayah in scope: **Hafs 'an 'Asim, via ash-Shatibiyyah**. Others are out of scope until stated.

## Naming

Library **MutqinCore**, CLI **mutqin-cli**, C API prefix **`mq_`** (header `core/include/mutqin.h`),
C++ namespace **`mutqin`**.

## Always read first

- `docs/PLAN.md` — which phase we are in and its acceptance criteria
- `docs/ARCHITECTURE.md` — folder layout, module boundaries, C API
- `docs/TAJWEED_SPEC.md` — before touching the phonemizer / rule engine
- `docs/DATA_AND_MODEL.md` — before touching training, datasets, or evaluation
- `docs/TASKS.md` — the task checklist; update checkboxes when you finish a task

## Non-negotiable rules (religious accuracy)

1. **Never write Quran text from memory.** Never type, generate, autocomplete, or "fix" any
   Arabic Quran text in code, tests, or fixtures. Always load it from the verified source
   files in `data/quran/` (Tanzil Uthmani). Tests must reference ayahs by (surah, ayah)
   and read the text from that file.
2. **Never modify the source Quran text files.** They are read-only; verify their checksum
   on load.
3. **Never invent tajweed rules.** Implement only what is written in `docs/TAJWEED_SPEC.md`.
   If a case is unclear or missing, stop and add it to the "Open questions" section of the
   spec instead of guessing.
4. **Prefer missing an error over a false alarm.** Only report an error above a configurable
   confidence threshold. Telling a correct reciter they made a mistake is the worst failure.
5. Where the spec allows several valid options (e.g. madd munfasil 4 or 5 counts), accept
   all of them.

## Engineering rules

- **Shared core in C++17**, in `core/`. Portable (Linux, macOS, Windows). **No platform-specific
  code inside `core/`**; microphone capture lives in `cli/` (and later in the apps).
- Expose the core only through the **C API** in `core/include/mutqin.h` (stable ABI, so JNI and
  Swift bindings can be added later without changing the core).
- `mutqin-cli` is a thin client of the C API: reads a WAV (any rate, resampled inside the core) or live
  microphone input, plus a surah and ayah range, and prints detected events and mistakes.
- The Android app lives in `android/` (Kotlin, Jetpack Compose, JNI over the C API; started by the owner on 2026-10-14,
  decisions in `docs/APP_SPEC.md`, how to build in `android/README.md`). iOS (Swift, AVAudioEngine) is still **deferred**:
  do not create code in `ios/` until told. Keep `minSdk 21` and the pinned Compose 1.8 versions unless the owner agrees to raise them.
- Inference: **sherpa-onnx / ONNX Runtime** (int8 quantized models). No network calls at
  runtime; everything works offline.
- Python is used only in `tools/` (data prep, training, evaluation, reference phonemizer).
- The **Python reference phonemizer is the source of truth**. The C++ phonemizer must produce
  identical output; a test compares them over the whole Quran.
- Audio format inside the core: 16 kHz, mono, float32.
- Build: CMake. Tests: GoogleTest (C++), pytest (Python). Every new module needs tests.
- No new dependency without explaining why and checking its license (prefer MIT/Apache/BSD).
  Tell the user before adding it.
- Keep functions small, documented, and real-time safe in the audio path (no allocations
  or locks in the audio callback).
- CI (GitHub Actions) runs build, tests, lint and format checks; run the same checks locally before a
  commit (README, "Checks before a commit"). Tool versions are pinned in `tools/requirements-dev.txt`;
  formatting is `ruff format` (Python) and `clang-format` (C++), so never hand-format.

## Decisions already made by the owner (do not reopen unless asked)

- **Mutqin stays free.** Never add ads, payments, subscriptions, tracking or analytics. Audio never
  leaves the device; no network calls at run time.
- **Licenses.** The code is **Apache-2.0** (`LICENSE`, `NOTICE`). The recognition model is *not* part of
  the repository and is *not* Apache: Muno459/zipformer_p-quran is NPL-1.1 (free apps only, text in
  `licenses/NPL-1.1-zipformer.txt`). Its section 7 (share-alike) makes anything produced from the model,
  such as `expected_units.tsv`, a Derivative that must stay under NPL-1.1, so **never commit model files,
  derived tables, recordings, or anything under `models/` or `data/`**. Whenever a dependency, dataset,
  font or model is added or changed, update `docs/LICENSES.md` (and `NOTICE` if it ships or is linked).
- **Recognition model.** Zipformer through the optional sherpa-onnx backend, comparing the model's own
  canonical units (`tools/zipformer/expected_units.py`) instead of mapping them onto our 31 phonemes.
  Strictness levels relaxed / standard (default) / strict (`StrictnessProfile` in C++, `STRICTNESS_PROFILES` in
  Python, parity-tested): standard is `edge_free=2` (at most a third of a word missing at its edges), `add_min_tokens=3`;
  runs of one character are cut to 2 (madd length ignored) at every level. Change a profile only after re-measuring
  (`tools/prototype/zipformer_tune.py`, `zipformer_clips.py`) and record it in `docs/EVAL_RESULTS.md`. Our phonemizer and `TAJWEED_SPEC.md` stay for the
  harakat/tajweed phases. Whisper and wav2vec2 phonetics were measured and rejected.
- **No own model training for now** (no GPU). Do not propose GPU rentals, more audio downloads or new
  models unless asked. Training audio from EveryAyah has no stated license: research use only, never
  published.
- **Public repository.** README and docs are written for outside readers: no mention of AI tools in
  README or docs, no secrets, no personal paths. The assistant files (`AGENTS.md`, `CLAUDE.md`,
  `.claude/`) stay as they are.
- **Mobile is deferred** (task 3.5a in `docs/TASKS.md`). Ignore the Android NDK and Xcode for now; keep the
  core ready for the apps (below) and write no code in `android/` or `ios/`.

## Core and C API conventions (the apps depend on them)

- Plain C API, `int` return codes, error text in a caller buffer, no exceptions across the boundary,
  opaque handles. **ABI-stable structs: add fields only at the end.** Document every new function in
  `core/include/mutqin.h` and `docs/ARCHITECTURE.md`.
- **Threading contract:** calls on one session are serialised by a lock inside the core (push on one thread,
  poll on another is fine); an engine is read-only and may be shared; push audio from a worker thread,
  never from a real-time audio callback; the core starts no threads. **Memory is bounded** (`MQ_MAX_EXPECTED_PHONEMES`, the heard-phoneme cap, `MQ_ERR_LIMIT`).
- Recognition backends sit behind `AcousticModel` / `TokenRecognizer`. The sherpa-onnx backend
  (`MUTQIN_WITH_SHERPA`) and the CLI microphone (`MUTQIN_CLI_MIC`) stay **optional**: the default build and
  CI must pass without any model.
- While audio streams, the word being spoken is not judged (no transient false events); a mistake is
  reported about 1-2 s after it is spoken. Keep that behaviour when changing the aligner.
- Any C++ port keeps a Python-vs-C++ parity test; when the aligner behaviour or parameters change, update
  both sides (`tools/prototype/aligner.py`, `dump_align.py` profiles, `core/src/align/`).

## Measure, record, be honest

- Every measured result goes into `docs/EVAL_RESULTS.md` with its method and caveats (one reciter, simulated
  or real, synthetic mistakes). Never state a number that was not measured; report failures plainly.
- Targets (`docs/PLAN.md`): false alarms < 2% of words, mistake recall > 90%, real-time factor < 0.3.
- Real-model checks use `tools/prototype/zipformer_*.py` and `make check-wav`; the gated model is not in CI.

## Checks and working habits

- Use the `Makefile` (`make help`): `make check` before every commit; `make live`, `make record`,
  `make check-wav` to try the real model.
- Run lint and format checks in the foreground and read their output before committing (a commit once
  failed CI because the output was missed). After changing a pinned dependency, reconfigure from a clean
  build directory (stale `libgtest.a` broke a build once).
- Pinned C++ downloads (GoogleTest, miniaudio, sherpa-onnx) are updated by hand: change the version and the
  SHA-256 together, re-read the license, update `docs/LICENSES.md`. The weekly `cpp-deps` workflow reports new
  releases; Dependabot handles GitHub Actions and Python packages.
- Put scratch files, test recordings and temporary builds in the session scratchpad, not in the repository.
  Ask the owner before downloads of 100 MB or more or any external service.
- Commands for the owner to run in their terminal must work in **fish** (use `set -x VAR value`, avoid
  bash-only syntax) and use explicit paths.
- If every CI job fails within seconds with a billing message, it is the GitHub account's billing limit,
  not the code.
- Never push with `--force` (or rewrite history) without the owner's explicit permission.

## How to work

- Work on **one task from `docs/TASKS.md` at a time**, in order.
- Before coding: state the plan in 2-3 lines and list the files you will create or change.
- After coding: run the checks (`make check`), report results, tick the task in `docs/TASKS.md`, commit
  and push, then continue with the next task. Stop only when blocked on the owner.
- If something in the docs is contradictory or missing, ask instead of assuming.
- If a download fails because of network restrictions, give the user the exact command.
- Commit messages follow Conventional Commits (commitlint `config-conventional` rules):
  `type(scope): subject`, e.g. `build(core): add CMake project with GoogleTest`. Types: feat, fix,
  docs, test, build, chore, refactor, perf, ci. Scopes: core, cli, tools, docs, data, repo.
  Subject is lower-case imperative, no trailing period, header <= 100 chars. Never mention
  Claude or any AI tool in commit messages.
