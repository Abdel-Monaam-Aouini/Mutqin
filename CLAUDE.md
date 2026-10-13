# CLAUDE.md — Mutqin (متقن)

On-device Quran recitation checker. Monorepo: one repo for the whole product.
Read and follow `AGENTS.md` (rules) and the relevant file in `docs/` before every task.

## Current scope: C++ core + CLI only
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

## Workflow
- One task from `docs/TASKS.md` at a time, in order. Use `/next-task`.
- Before: 2–3 lines on what and which files. After: run tests, show results, tick the box,
  commit (Conventional Commits, e.g. `feat(tools): add Tanzil download script`, no AI mention),
  push, then continue with the next task. Stop only when blocked on the user.
- Track multi-step state with claude-mem `work_state_write`.

## Layout
`core/` C++ library · `cli/` mutqin-cli · `tools/` Python · `data/quran/` verified text ·
`models/` · `docs/` · `android/`, `ios/` (deferred) · `.claude/` rules and skills.
CI (GitHub Actions) is deferred: do not create `.github/` workflows; run tests locally.

Detailed rules: `.claude/rules/`. Skills: `/next-task`, `/add-dependency`, `/verify-quran-data`.
