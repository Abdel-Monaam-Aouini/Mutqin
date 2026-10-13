# Project Plan

Each phase ends with something working and testable. Do not start a phase before the
previous one meets its acceptance criteria.

## Phase 0 — Foundation (≈1 week)

Goal: repository, build system, verified Quran text.

- Monorepo skeleton per `ARCHITECTURE.md` (incl. placeholder `android/` and `ios/`); CMake
  builds an empty `core` with GoogleTest. CI (GitHub Actions) builds, tests, lints and format-checks on
  Linux, macOS and Windows (`.github/workflows/ci.yml`).
- Download Tanzil Uthmani text (+ simple clean text), store in `data/quran/`, record
  SHA-256 checksums, write a loader (Python + C++) with checksum verification.
- Data model: surah → ayah → word, with stable IDs (`surah:ayah:word`).
- Check and document licenses of all sources in `docs/LICENSES.md`.

**Done when:** loader returns 114 surahs / 6236 ayahs, checksum verified, tests pass locally.

## Phase 1 — Text → expected phonemes (rule engine) (≈3–4 weeks)

Goal: convert any ayah into an annotated expected phoneme sequence.

- Python reference phonemizer implementing `TAJWEED_SPEC.md`.
- Output per phoneme: symbol, source word index, rule tags (e.g. `ikhfa`, `madd_munfasil`),
  allowed duration range in counts, and variants for continuing vs stopping (waqf).
- Validation: compare rule tags against an existing tajweed annotation dataset and a
  color-coded tajweed mushaf for Al-Fatiha, Juz' 'Amma, and Al-Baqarah 1–50.
- Port to C++; a test compares C++ and Python output for all 6236 ayahs.

**Done when:** 100% match with the annotation dataset on the validation set (every
difference is either fixed or documented as a dataset error), C++ == Python everywhere,
and a teacher has reviewed the output for the validation surahs.

## Phase 2 — Word-level tracking prototype, desktop (≈2–3 weeks)

Goal: hifz mode on a laptop with Python.

- Use an existing Quran-fine-tuned CTC model (char or phoneme level), see `DATA_AND_MODEL.md`.
- Streaming pipeline: VAD → features → model → incremental alignment against the expected
  ayah range.
- Detect: skipped word, wrong word, repeated word, added word; track current position.
- Evaluation set: correct recitations + recitations with injected word errors.

**Done when:** false alarm rate < 2% on correct recitations, word-error recall > 90%,
latency < 500 ms on a laptop.

## Phase 3 — C++ core + mutqin-cli (≈3 weeks)

Goal: Phase 2 running inside the portable C++ core, testable from a command-line tool.

- C++ core (MutqinCore): audio ring buffer, VAD (Silero via sherpa-onnx), feature extraction,
  inference, aligner, event queue, C API (`mq_` prefix). No platform-specific code.
- `mutqin-cli`: reads a 16 kHz mono WAV or live microphone input, plus a surah and ayah
  range; prints detected events and mistakes. Microphone capture lives in `cli/`.
- Builds and passes tests on Linux, macOS, Windows.
- int8 model, size budget < 100 MB, real-time factor < 0.3 on a reference desktop CPU.
- Python aligner and C++ aligner are compared on the evaluation set (parity test).

**Done when:** the CLI runs fully offline and passes the Phase 2 evaluation using the C++ core.

## Phase 3b — Mobile apps (DEFERRED, after Phase 3)

Android (Kotlin, JNI, Oboe) and iOS (Swift, AVAudioEngine) apps in `android/` and `ios/`, on top
of the unchanged C API. UI: Uthmani text with word highlighting, mistakes in color, "hidden
text" mode. Phone benchmarks (RTF < 0.3 on a mid-range phone, battery) move here. To be broken
down when started.

## Phase 4 — Harakat / letter-level errors (≈4–6 weeks)

Goal: detect wrong vowels and wrong letters.

- Train or fine-tune a **phoneme-level CTC model** on the phoneme inventory from
  `TAJWEED_SPEC.md`.
- Forced alignment + Goodness of Pronunciation (GOP) per phoneme.
- Collect learner recordings with labeled mistakes (see `DATA_AND_MODEL.md`).
- Per-phoneme confidence thresholds tuned for low false alarms.

**Done when:** harakat-error precision > 85% at false alarm rate < 3%.

## Phase 5 — Tajweed detection (≈2–3 months)

Goal: detect tajweed mistakes.

- Duration rules (madd, ghunnah) measured relative to the reciter's tempo.
- Ghunnah presence/absence (nasalization), qalqalah, idgham/ikhfa/iqlab realization,
  tafkhim/tarqiq (start with ra and lam of Allah).
- Each rule type gets its own evaluation and can be enabled independently.
- Ship rules one by one, only when they meet their quality bar.

**Done when:** each shipped rule has precision > 85% on expert-labeled data and teacher
sign-off.

## Phase 6 — Expert review, beta, release

- Review panel of qualified teachers; review sessions with real users.
- Beta (CLI testers now; TestFlight / Play internal testing once the mobile apps exist);
  collect opt-in, anonymized feedback.
- Privacy: audio never leaves the device unless the user explicitly opts in.

## Key metrics (tracked from Phase 2 onward)

| Metric | Target |
|--------|--------|
| False alarm rate on correct recitation | < 2% (most important) |
| Word-error recall | > 90% |
| Latency (speech → event) | < 300 ms (desktop CPU now; on phone in Phase 3b) |
| Real-time factor | < 0.3 (desktop CPU now; on phone in Phase 3b) |
| Model size | < 100 MB |
| Battery | < 8% per 30 min session (Phase 3b, mobile only) |
