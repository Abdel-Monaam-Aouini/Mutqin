# CLAUDE.md — Mutqin (متقن)

On-device Quran recitation checker. Monorepo: one repo for the whole product.
Read and follow `AGENTS.md` (rules) and the relevant file in `docs/` before every task.

## Current scope: C++ core + CLI only (a real model already works end to end)
- Build only `core/` (library **MutqinCore**, C++17) and `cli/` (**mutqin-cli**), plus Python in `tools/`.
- `android/` and `ios/` exist as **placeholders only**. Write no Kotlin, Swift, JNI, Oboe, or
  AVAudioEngine code and add no files there except their README until the user says so.
- No platform-specific code in `core/`. Must build on Linux, macOS, Windows.
- Naming: C API prefix `mq_`, header `core/include/mutqin.h`, C++ namespace `mutqin`.

## Hard rules (never break)
1. Never write, type, or generate Quran text from memory — not one word, not in tests.
   Load from `data/quran/` and verify the SHA-256 checksum. Tests refer to (surah, ayah).
2. Never modify files in `data/quran/` (also blocked in `.claude/settings.json`).
3. Never invent tajweed rules. Unclear → add to "Open questions" in `docs/TAJWEED_SPEC.md`, ask the user.
4. Prefer missing an error over a false alarm.
5. New dependency → tell the user why + its license BEFORE adding it.
6. Network download fails → give the user the exact command to run themselves.
7. Never commit anything under `models/` or `data/`, recordings, or files derived from the Zipformer
   (NPL-1.1 share-alike); the repo is **public**: no AI mention in README/docs, no secrets, no personal paths.
8. Mutqin stays free: no ads, payments, subscriptions or tracking.

## Workflow
- One task from `docs/TASKS.md` at a time, in order. Use `/next-task`.
- Before: 2–3 lines on what and which files. After: run tests, show results, tick the box,
  commit (Conventional Commits, e.g. `feat(tools): add Tanzil download script`, no AI mention),
  push, then continue with the next task. Stop only when blocked on the user.
- Run `make check` (lint + C++ tests + Python tests) in the foreground and read the output before each commit.
  Never force-push or rewrite history without explicit permission.
- Track multi-step state with claude-mem `work_state_write`.

## Layout
`core/` C++ library · `cli/` mutqin-cli · `tools/` Python · `data/quran/` verified text ·
`models/` · `docs/` · `android/`, `ios/` (deferred) · `.claude/` rules and skills.
CI: `.github/workflows/ci.yml` (build + test on Linux/macOS/Windows, ASan/UBSan, pytest, lint, format).
Before every commit run the same checks locally (`make check`; see README, "Development"). `make help` lists
the tasks (build, test, lint, live, record, check-wav ...).

## Decisions to respect (details in `AGENTS.md`)
- Code is Apache-2.0; the Zipformer model is NPL-1.1 and stays outside the repo (`licenses/`, `NOTICE`,
  `docs/LICENSES.md` must be updated with any dependency, dataset or model change).
- Model-native units + tuned aligner profile; no own training, no GPU/cloud proposals, no extra models unless
  asked. Whisper and wav2vec2 were rejected.
- Core API for the apps: ABI-stable (add fields at the end), threading contract, bounded memory, backends
  optional, no judging of the word being spoken while streaming. Mobile and the NDK are deferred.
- Every measured number goes into `docs/EVAL_RESULTS.md` with caveats; never state unmeasured numbers.
- Commands for the user's terminal must work in fish.

Detailed rules: `.claude/rules/`. Skills: `/next-task`, `/add-dependency`, `/verify-quran-data`.
