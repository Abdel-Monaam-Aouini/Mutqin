# Licenses

Every third-party source, dependency, dataset, font, and model used by Mutqin, with its license.
Update this file **before** a new item is added (see the `add-dependency` skill). Verified means
the license text was read from the upstream source, not from memory.

## Verified

| Item | Used for | License | Verified from | Notes |
|------|----------|---------|---------------|-------|
| Tanzil Quran text, v1.1 (Uthmani + Simple Clean) | Verified Quran text in `data/quran/` | Creative Commons Attribution 3.0, plus Tanzil Terms of Use | https://tanzil.net/docs/Text_License (read 2026-10-13) | See below |
| Mutqin itself (this repository) | The project code | Apache-2.0 (`LICENSE`, `NOTICE`), chosen by the owner 2026-10-14 | official text from apache.org/licenses/LICENSE-2.0.txt | The Zipformer model and other third-party items below keep their own licenses and are not part of the repository |
| GoogleTest v1.18.0 | C++ tests only (fetched by CMake, never linked into MutqinCore or the CLI) | BSD-3-Clause | `LICENSE` in the fetched source | |
| sherpa-onnx v1.13.8 (k2-fsa/sherpa-onnx; bundles ONNX Runtime, MIT) | Optional recognition backend (`-DMUTQIN_WITH_SHERPA=ON`), linked through its C API; release archive downloaded by the developer, not committed | Apache-2.0 (GitHub API license field, read 2026-10-13) | approved by the owner 2026-10-13 ("work on sherpa-onnx backend"); default builds and CI do not use it |
| sherpa-onnx-streaming-zipformer-ctc-multi-zh-hans-int8-2023-12-13 (public test model) | Plumbing test of the backend only (Chinese; units mapped to phonemes arbitrarily); not shipped | model card not read for the license: test use only, never redistributed | downloaded 2026-10-13 into the scratchpad, not in the repo |
| miniaudio 0.11.25 (single header, mackron/miniaudio) | Live microphone capture in `mutqin-cli` only (never in `core/`); downloaded at CMake configure time with a pinned SHA-256, not committed | Public domain (Unlicense) or MIT No Attribution, at the user's choice | `LICENSE` in the 0.11.25 tag, read 2026-10-13; approved by the owner 2026-10-13 |
| pytest 8.x | Python tests only (`tools/requirements.txt`) | MIT | package metadata of the installed pytest 8.4.2 | |

### Tanzil Quran text: required notice and terms

The following notice applies to, and must be included in, all copies of the Tanzil Quran text:

> Tanzil Quran Text
> Copyright (C) 2007-2021 Tanzil Project
> License: Creative Commons Attribution 3.0
>
> This copy of the Quran text is carefully produced, highly verified and continuously monitored
> by a group of specialists in Tanzil Project.

Terms of use (summarized from the page above):

- Verbatim copies may be copied and distributed; **changing the text is not allowed**.
- Usable in any website or application, provided the source (**Tanzil Project**) is clearly
  indicated and a link to https://tanzil.net is made so users can track changes.
- The copyright notice must be included in all verbatim copies and reproduced appropriately in
  all files derived from or containing a substantial portion of the text.
- Updates: http://tanzil.net/updates/

How the project complies:

- Files in `data/quran/` are stored exactly as downloaded and are checksum-verified on load;
  no code modifies them.
- Any app or CLI output that shows the text must credit Tanzil Project and link tanzil.net
  (to be added to the CLI `--about` output and later to the app credits screen).
- Open question for the owner before any commercial release: the page combines "CC BY 3.0" with
  "changing it is not allowed". Confirm with Tanzil / a lawyer that using the text for matching
  and display (no edits) is covered, and whether the derived phoneme data may be distributed.

## Developer and CI tools (not shipped)

| Item | Used for | License | Verified from |
|------|----------|---------|---------------|
| ruff 0.17.0 | Python lint and format (`tools/requirements-dev.txt`, CI) | MIT | PyPI metadata 2026-10-13 |
| clang-format 23.1.3 | C++ format (`tools/requirements-dev.txt`, CI) | Apache-2.0 with LLVM exception | PyPI metadata 2026-10-13 |
| actions/checkout, actions/setup-python, actions/cache (v4 / v5) | GitHub Actions steps in `.github/workflows/ci.yml` | MIT | GitHub Marketplace listings, not re-verified here |

## To verify before use (not yet used)

Names come from `docs/DATA_AND_MODEL.md` and are starting points only.

| Item | Intended use | License | Status |
|------|--------------|---------|--------|
| King Fahd Complex (QPC) fonts / text | Mushaf rendering in the UI (apps, later) | unknown | not checked |
| tarekeldeeb/quran-tajweed-embedded (fork of cpfair/quran-tajweed) | Candidate to validate the rule engine (Phase 1, task 1.10) | README states CC BY 4.0 for the JSON data file; no code license found; upstream license not stated | used for validation only on 2026-10-13 (task 1.10), not redistributed or copied into the repo; the repo has no LICENSE file, the README states CC BY 4.0 for the JSON data file; attribute it in `docs/VALIDATION_REPORT.md`. Annotations are generated by rule decision trees, not by hand, so it is a cross-check, not independent ground truth. Offsets are code points into the Tanzil "uthmani-pause-sajdah" text; marked unmaintained |
| Quran.com API tajweed text | Validation (Phase 1) | unknown | not checked |
| Quranic Arabic Corpus | Word segmentation / morphology | unknown | not checked |
| EveryAyah.com reciter audio | Evaluation / training audio | **no license stated** on everyayah.com (home page and recitations list read 2026-10-13); the recordings are professional reciters' work, so their copyright and any training or redistribution terms are unknown | not downloaded; ask the owner before training or redistributing anything derived from it |
| facebook/wav2vec2-xls-r-300m (pretrained speech encoder, 300M, 128 languages) | Starting point to fine-tune a 32-token phoneme CTC model (teacher) | Apache-2.0 (model card API read 2026-10-13), not gated | chosen; rejected facebook/mms-300m (CC-BY-NC-4.0, non-commercial) and wav2vec2-base (English-only pretraining) |
| tarteel-ai/everyayah (Hugging Face copy of EveryAyah, 187,785 train clips, 117 GB) | Training / evaluation audio | **no license stated** in the dataset card (API read 2026-10-13); same upstream recordings as EveryAyah | not downloaded |
| Buraaq/quran-md-words (word clips) | Training data of the wav2vec2 candidate | **no license stated** (dataset API read 2026-10-13) | not downloaded |
| Muno459/zipformer_p-quran (streaming Zipformer2 CTC, 250 phoneme units, 65.5M params, int8 ONNX 73 MB, runs with sherpa-onnx) | Acoustic model candidate (Phase 2-3) | NPL-1.1 (Quran-Lab No-Profit License): gated; use, fine-tuning and redistribution only in apps that are free to end users; no selling, paywall, subscription or ads | read from its model card 2026-10-13; downloaded 2026-10-14 by the owner after access was granted (models/zipformer_p-quran, not committed). The unmodified license text is kept in `licenses/NPL-1.1-zipformer.txt`. Section 7 (share-alike): files produced from the model, such as `expected_units.tsv`, are Derivatives and must stay under NPL-1.1, so they are never committed; the code that produces them is ours (Apache-2.0). A few example units appear in docs/EVAL_RESULTS.md as short excerpts of results. Hafs; reported PER 11.63%, board WER 5.83. The owner confirmed on 2026-10-13 that Mutqin stays free to end users, so the terms can be met; the project must never add ads, payments or subscriptions while this model is used |
| tarteel-ai/whisper-base-ar-quran (Whisper fine-tune, text output) | Acoustic model candidate (word level only) | Apache-2.0 | read from its model card 2026-10-13; not downloaded; training data not named; Whisper's language model tends to output the expected text even for mistakes |
| TBOGamer22/wav2vec2-quran-phonetics (wav2vec2-base CTC, 94.4M params, phonetic text output) | Acoustic model candidate | Apache-2.0 (model); training data Buraaq/quran-md-words, license not stated | read from its model card 2026-10-13; not downloaded; trained on single-word clips, card says not for tajweed grading |
| Silero VAD | Possible neural VAD in the core (Phase 3; the core has an energy VAD today) | unknown | not checked. sherpa-onnx and its bundled ONNX Runtime are recorded above |
