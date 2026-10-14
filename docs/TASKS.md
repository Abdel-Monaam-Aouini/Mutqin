# Task Checklist

AI assistants: do one task at a time, run tests, then tick it here.

## Phase 0 — Foundation
- [x] 0.1 Create folder layout from ARCHITECTURE.md (incl. `cli/`, placeholder `android/` and `ios/`), `.gitignore`, `.editorconfig`
- [x] 0.2 CMake project for `core/` with GoogleTest; one passing dummy test
- [x] 0.3 CI (GitHub Actions): build + test (Linux gcc/clang, ASan+UBSan, macOS, Windows), Python tests, lint and format checks
- [x] 0.4 `tools/quran_text/download.py`: fetch Tanzil Uthmani + simple text, write SHA-256 checksums
- [x] 0.5 Python loader + data model (surah/ayah/word IDs) with checksum check; tests: 114 surahs, 6236 ayahs
- [x] 0.6 C++ loader with the same behavior; test compares with Python output
- [x] 0.7 `docs/LICENSES.md` with every source and its license

## Phase 1 — Rule engine
- [x] 1.1 Phoneme inventory (`tools/phonemizer/inventory.py`) + docs
- [x] 1.2 Uthmani text normalization → letters + diacritics per word (no text changes, only parsing)
- [x] 1.3 Basic letters + short vowels + sukun + shaddah → phonemes
- [x] 1.4 Hamzat al-wasl, lam shamsiyyah/qamariyyah, lam of Allah (lam of Allah tags deferred to Phase 5, see Open questions)
- [x] 1.5 Noon sakinah / tanween rules (3.1)
- [x] 1.6 Meem sakinah, mushaddad ghunnah, qalqalah (3.2–3.4)
- [x] 1.7 Madd types and durations (3.7)
- [x] 1.8 Waqf variants (section 5)
- [x] 1.9 Special cases data file (3.9) — empty structure first, filled with teacher (structure, loader, candidates report done; contents wait for the teacher)
- [x] 1.10 Validation against annotation dataset; report of all differences (see docs/VALIDATION_REPORT.md)
- [x] 1.11 Human-readable report generator for teacher review (HTML/PDF)
- [x] 1.12 C++ port + full-Quran equality test with Python (phonemizer ported to core/src/tajweed; tools such as report, validate and show stay in Python)

## Phase 2 — Desktop prototype
- [x] 2.1 Evaluate 2–3 candidate CTC models on EveryAyah subset; pick one — Zipformer chosen (Whisper 10.2% word errors, wav2vec2 word clips only); 1.8% flagged words untuned on 49 min of correct recitation
- [x] 2.2 Synthetic error generator (text-side and audio-side)
- [x] 2.3 Streaming pipeline: VAD → model → posteriors — sherpa-onnx streaming backend, real-time factor about 0.03
- [x] 2.4 Incremental aligner with re-anchoring — incremental aligner in C++; re-anchoring by skip/repeat arcs; tuned on real output
- [x] 2.5 Word error detector + confidence thresholds — tuned on one reciter: 0.9% flagged words and 2.5% events on a correct recitation, recall 94-100%; no per-token confidences from sherpa-onnx; more reciters and learners pending
- [x] 2.6 Evaluation script + first results in EVAL_RESULTS.md

## Phase 3 — C++ core + mutqin-cli
- [x] 3.1 C++ audio ring buffer, VAD, features — ring buffer, log-mel (80 bins, 25/10 ms) and energy VAD done; Silero VAD and exact model feature settings follow with task 2.1
- [x] 3.2 sherpa-onnx integration in core — sherpa-onnx backend with the real Zipformer through the C API (model-native units); CI runs it with a public test model
- [x] 3.3 Port aligner + detector to C++, with Python-vs-C++ parity test — 180 generated cases match the Python prototype
- [ ] 3.4 C API (`mq_`) + tests — expected phonemes, posteriors in, word statuses and events out are done and tested; only the inference backend and live audio are missing
- [ ] 3.5 (DEFERRED to Phase 3b) Android demo app (Oboe + JNI)
- [ ] 3.6 (DEFERRED to Phase 3b) iOS demo app (AVAudioEngine + Swift bridge)
- [ ] 3.5a (DEFERRED, first step of Phase 3b) Mobile readiness of the core, before any app code:
  cross-compile `core/` for Android (NDK, arm64-v8a and x86_64; about 1 GB of tools, ask the owner first)
  and iOS (arm64 device and simulator, needs macOS/Xcode); CMake install rules and a shared library (`.so`)
  / `xcframework` target; Quran text delivery (iOS bundle path, Android: copy from the APK on first run or
  add a memory loader); sherpa-onnx mobile builds (check they work with our backend); measure speed, memory
  and battery on a phone
- [ ] 3.7 Desktop benchmarks (RTF, latency); phone benchmarks deferred to Phase 3b — front end, aligner and a stand-in model measured on desktop (docs/EVAL_RESULTS.md); the real model and latency to an event are pending
- [x] 3.8 `mutqin-cli`: WAV (16 kHz mono) and live microphone input, surah + ayah range, prints events and mistakes — expected phonemes, WAV, live microphone (--mic), posterior replay, --model for the real model

## Phase 4–6
Tasks will be broken down when Phase 3 is done.
