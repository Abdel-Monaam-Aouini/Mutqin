# Project Plan

Each phase ends with something working and testable. Do not start a phase before the
previous one meets its acceptance criteria.

## Status (2026-10-14)

| Phase | State |
|---|---|
| 0 Foundation | done |
| 1 Rule engine | done in Python and C++ (identical over the whole Quran); teacher review of the rules still open |
| 2 Word-level prototype | done with the real model (Muno459/zipformer_p-quran); latency target not met, see "Open decisions" |
| 3 C++ core + CLI | done: C API, CLI with WAV, microphone and replay, sherpa-onnx backend, packaging; needs the gated model for real audio |
| 3b Mobile apps | not started; core side prepared, app specification drafted (`APP_SPEC.md`) |
| 4 Harakat / letter level | not started |
| 5 Tajweed | not started |
| 6 Review, beta, release | not started; the repository is being prepared for a public release |

Measured so far (details in `EVAL_RESULTS.md`): 0.9% of words flagged and 2.5% events on 49 minutes of a
correct recitation (one reciter), 0.1% flagged on 339 single-ayah clips of a second reciter; recall on
mistakes injected into real model output 94-100%; a mistake is reported 1-2 s after it is spoken; real-time
factor about 0.03 on a desktop CPU. Not measured yet: learners' recitation, phone microphones, phones.

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

*Result:* false alarms and recall are met on real model output (two reciters). Latency from speech to event is
1-2 s, not under 500 ms: the model looks ahead about 1 s and the core does not judge the word being spoken
until the next one starts. Whether that is acceptable is an open decision.

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

*Result:* done, with these differences from the plan: the VAD is a simple energy detector (a neural VAD was
not needed), inference goes through the optional sherpa-onnx backend, any WAV sample rate is accepted
(resampled to 16 kHz inside), and the C API gained session items, position, reset, an internal lock and
bounded memory. The gated model cannot run in CI, so CI exercises the backend with a public test model.

## Phase 3b — Mobile apps (DEFERRED, after Phase 3)

Android (Kotlin, JNI, Oboe) and iOS (Swift, AVAudioEngine) apps in `android/` and `ios/`, on top
of the unchanged C API. UI: Uthmani text with word highlighting, mistakes in color, "hidden
text" mode. Phone benchmarks (RTF < 0.3 on a mid-range phone, battery) move here. The behaviour of the
apps is drafted in `APP_SPEC.md`, whose open questions need the owner's decisions first.

Steps, in order:

1. **Core readiness** (task 3.5a). Done: resampler for any input rate, session items/position/reset,
   engine from memory, install rules, a shared library that exports only the C API, a plain-C header test,
   threading contract with an internal lock (checked with ThreadSanitizer), bounded memory. Android cross-compiling is done (NDK r27.1,
   arm64-v8a, armeabi-v7a, x86_64; the real model runs on an emulator with results identical to the desktop).
   Open: iOS (device and simulator, needs macOS and Xcode; xcframework; no prebuilt iOS archive of sherpa-onnx
   was found), and a real ARM phone and an old Android version (speed, memory, battery, heat, Arabic text).
2. **Decisions** are taken (`APP_SPEC.md`, section 12). Remaining before app code: the font rendering test,
   store accounts and identifiers (the strictness level API is done).
3. **Android prototype** (Jetpack Compose): record, push on a worker thread, highlight the current word, show
   marks, range picker, follow mode; model files placed by hand.
4. **Hifz mode, result screen, history, settings, localization (English, Arabic), first-run flow with the model
   download, licenses screen.**
5. **iOS app** (SwiftUI) with the same behaviour on the same core.
6. **Learner testing** with consent: recordings with known mistakes, tuning against the false alarms they
   show (see "Open decisions").
7. **Store submission**: policies, descriptions, privacy forms, in-app license texts (project license,
   Tanzil attribution, the model's NPL-1.1 text, font licenses).

**Done when:** both apps run fully offline on a mid-range phone with RTF < 0.3 and meet the Phase 2 false alarm
and recall numbers on recordings made on phones.

## Phase 4 — Harakat / letter-level errors (≈4–6 weeks)

Goal: detect wrong vowels and wrong letters.

- Train or fine-tune a **phoneme-level CTC model** on the phoneme inventory from
  `TAJWEED_SPEC.md`.
- Forced alignment + Goodness of Pronunciation (GOP) per phoneme.
- Collect learner recordings with labeled mistakes (see `DATA_AND_MODEL.md`).
- Per-phoneme confidence thresholds tuned for low false alarms.

**Done when:** harakat-error precision > 85% at false alarm rate < 3%.

*Update:* the Zipformer already hears vowels and letters (its units are consonant + vowel), so the first step
needs no training: report which unit inside a word differs from the canonical one. Before building that,
measure how often the model disagrees with the canonical units on correct recitation (two reciters now,
learners later) and set strict thresholds from it. Training an own model is on hold (no GPU); the pipeline is
kept in `tools/train/` (`TRAINING.md`).

## Phase 5 — Tajweed detection (≈2–3 months)

Goal: detect tajweed mistakes.

- Duration rules (madd, ghunnah) measured relative to the reciter's tempo.
- Ghunnah presence/absence (nasalization), qalqalah, idgham/ikhfa/iqlab realization,
  tafkhim/tarqiq (start with ra and lam of Allah).
- Each rule type gets its own evaluation and can be enabled independently.
- Ship rules one by one, only when they meet their quality bar.

**Done when:** each shipped rule has precision > 85% on expert-labeled data and teacher
sign-off.

*Note:* the model's canonical units use fixed madd lengths, so duration rules need the reciter's tempo and
are ignored at word level today (runs of one unit are cut to two). The rule engine (`TAJWEED_SPEC.md`) stays the
reference for these phases and still needs a teacher's review.

## Phase 6 — Expert review, beta, release

- Review panel of qualified teachers; review sessions with real users.
- Beta (CLI testers now; TestFlight / Play internal testing once the mobile apps exist);
  collect opt-in, anonymized feedback.
- Privacy: audio never leaves the device unless the user explicitly opts in.

## Decisions and open items

Decided on 2026-10-14 (details in `APP_SPEC.md`, section 12): native UI (Jetpack Compose, SwiftUI); support for old phones
(Android 5.0 and iOS 13 as the minimum, the lowest the stack allows); localization from the start (English and
Arabic first); a professional look with few colors and shapes instead of colors for the marks; Uthmani script
with an OFL-1.1 font (final choice after a rendering test on a device); the model is delivered after install;
history stored locally on the phone; a strictness setting with three levels; a latency of 1-2 s is acceptable
for now; opt-in sharing of saved takes for beta testing; no required disclaimer text for now.

| # | Open item | Who or what it waits for |
|---|-----------|--------------------------|
| 1 | ~~Strictness levels~~ done: `mq_session_set_strictness` with three measured profiles (`EVAL_RESULTS.md`) | - |
| 2 | Accuracy on **learners'** voices and **phone microphones** is unmeasured: record takes (correct and with deliberate mistakes), score them, tune | owner's recordings |
| 3 | The tajweed rules and the phoneme tables are provisional until a qualified teacher reviews them | a teacher |
| 4 | The word-level check depends on a **gated third-party model** (Muno459/zipformer_p-quran, NPL-1.1: free apps only, derived files stay under the same license, no commercial license exists). Keep a local copy; if it disappeared or the terms changed, the fallback is the paused own-model training | owner |
| 5 | Font: render the real text with Amiri (Quran variant) and Scheherazade New on a device, pick one, add its license to the app | device test |
| 6 | Model delivery for release (store asset delivery or a one-time download with SHA-256 check); development builds place the files by hand | later |
| 7 | Store accounts (Google Play, Apple Developer Program), developer name, identifiers (`app.mutqin` proposed), final name and icon (draft in `docs/branding/`) | owner |
| 8 | ~~Android NDK~~ available and used; Xcode (macOS) for iOS, and phones for measurements, **including old devices** (speed, memory and Arabic text rendering near Android 5 and iOS 13) | owner (tools, devices) |
| 9 | Before the repository is made public: the commit history shows the author's e-mail address, the assistant files (`CLAUDE.md`, `AGENTS.md`, `.claude/`) stay visible by choice, a few example units appear in `EVAL_RESULTS.md`, and GitHub Actions billing currently blocks CI (free for public repositories) | owner |
| 10 | Training audio from EveryAyah has no stated license: research use only, nothing trained on it is published | owner, if training resumes |
| 11 | Letter- and tajweed-level feedback (Phases 4-5) after the mobile apps, or in parallel; improving the 1-2 s latency | owner |

## Key metrics (tracked from Phase 2 onward)

| Metric | Target | Measured (2026-10-14) |
|--------|--------|------------------------|
| False alarm rate on correct recitation | < 2% (most important) | 0.9% of words flagged (surah 5, one reciter, 49 min); 0.1% (339 clips, second reciter) |
| Word-error recall | > 90% | skipped 100%, wrong 97%, repeated 99%, added 94% (mistakes injected into real model output) |
| Latency (speech → event) | < 300 ms (desktop CPU now; on phone in Phase 3b) | 1-2 s |
| Real-time factor | < 0.3 (desktop CPU now; on phone in Phase 3b) | 0.03 (49 min in 90 s, 4 threads, desktop CPU) |
| Model size | < 100 MB | 73 MB model; about 100 MB with data and libraries |
| Battery | < 8% per 30 min session (Phase 3b, mobile only) | not measured |
