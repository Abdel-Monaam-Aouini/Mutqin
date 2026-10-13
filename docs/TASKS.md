# Task Checklist

AI assistants: do one task at a time, run tests, then tick it here.

## Phase 0 — Foundation
- [x] 0.1 Create folder layout from ARCHITECTURE.md (incl. `cli/`, placeholder `android/` and `ios/`), `.gitignore`, `.editorconfig`
- [x] 0.2 CMake project for `core/` with GoogleTest; one passing dummy test
- [ ] 0.3 (DEFERRED) CI (GitHub Actions): build core + run C++ and Python tests
- [x] 0.4 `tools/quran_text/download.py`: fetch Tanzil Uthmani + simple text, write SHA-256 checksums
- [ ] 0.5 Python loader + data model (surah/ayah/word IDs) with checksum check; tests: 114 surahs, 6236 ayahs
- [ ] 0.6 C++ loader with the same behavior; test compares with Python output
- [x] 0.7 `docs/LICENSES.md` with every source and its license

## Phase 1 — Rule engine
- [ ] 1.1 Phoneme inventory (`tools/phonemizer/inventory.py`) + docs
- [ ] 1.2 Uthmani text normalization → letters + diacritics per word (no text changes, only parsing)
- [ ] 1.3 Basic letters + short vowels + sukun + shaddah → phonemes
- [ ] 1.4 Hamzat al-wasl, lam shamsiyyah/qamariyyah, lam of Allah
- [ ] 1.5 Noon sakinah / tanween rules (3.1)
- [ ] 1.6 Meem sakinah, mushaddad ghunnah, qalqalah (3.2–3.4)
- [ ] 1.7 Madd types and durations (3.7)
- [ ] 1.8 Waqf variants (section 5)
- [ ] 1.9 Special cases data file (3.9) — empty structure first, filled with teacher
- [ ] 1.10 Validation against annotation dataset; report of all differences
- [ ] 1.11 Human-readable report generator for teacher review (HTML/PDF)
- [ ] 1.12 C++ port + full-Quran equality test with Python

## Phase 2 — Desktop prototype
- [ ] 2.1 Evaluate 2–3 candidate CTC models on EveryAyah subset; pick one
- [ ] 2.2 Synthetic error generator (text-side and audio-side)
- [ ] 2.3 Streaming pipeline: VAD → model → posteriors
- [ ] 2.4 Incremental aligner with re-anchoring
- [ ] 2.5 Word error detector + confidence thresholds
- [ ] 2.6 Evaluation script + first results in EVAL_RESULTS.md

## Phase 3 — C++ core + mutqin-cli
- [ ] 3.1 C++ audio ring buffer, VAD, features
- [ ] 3.2 sherpa-onnx integration in core
- [ ] 3.3 Port aligner + detector to C++, with Python-vs-C++ parity test
- [ ] 3.4 C API (`mq_`) + tests
- [ ] 3.5 (DEFERRED to Phase 3b) Android demo app (Oboe + JNI)
- [ ] 3.6 (DEFERRED to Phase 3b) iOS demo app (AVAudioEngine + Swift bridge)
- [ ] 3.7 Desktop benchmarks (RTF, latency); phone benchmarks deferred to Phase 3b
- [ ] 3.8 `mutqin-cli`: WAV (16 kHz mono) and live microphone input, surah + ayah range, prints events and mistakes

## Phase 4–6
Tasks will be broken down when Phase 3 is done.
