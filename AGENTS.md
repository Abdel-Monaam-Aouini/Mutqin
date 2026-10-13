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
- `mutqin-cli` is a thin client of the C API: reads a 16 kHz mono WAV or live microphone input,
  plus a surah and ayah range, and prints detected events and mistakes.
- Android (Kotlin, JNI, Oboe) and iOS (Swift, AVAudioEngine) are **deferred**. Do not create
  code in `android/` or `ios/` until told.
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
- CI (GitHub Actions) is deferred; run tests locally.

## How to work

- Work on **one task from `docs/TASKS.md` at a time**, in order.
- Before coding: state the plan in 2-3 lines and list the files you will create or change.
- After coding: run the tests, report results, tick the task in `docs/TASKS.md`, suggest a
  commit message, then stop and wait for "next".
- If something in the docs is contradictory or missing, ask instead of assuming.
- If a download fails because of network restrictions, give the user the exact command.
- Commit messages follow Conventional Commits (commitlint `config-conventional` rules):
  `type(scope): subject`, e.g. `build(core): add CMake project with GoogleTest`. Types: feat, fix,
  docs, test, build, chore, refactor, perf, ci. Scopes: core, cli, tools, docs, data, repo.
  Subject is lower-case imperative, no trailing period, header <= 100 chars. Never mention
  Claude or any AI tool in commit messages.
