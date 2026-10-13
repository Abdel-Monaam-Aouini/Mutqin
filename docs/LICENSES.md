# Licenses

Every third-party source, dependency, dataset, font, and model used by Mutqin, with its license.
Update this file **before** a new item is added (see the `add-dependency` skill). Verified means
the license text was read from the upstream source, not from memory.

## Verified

| Item | Used for | License | Verified from | Notes |
|------|----------|---------|---------------|-------|
| Tanzil Quran text, v1.1 (Uthmani + Simple Clean) | Verified Quran text in `data/quran/` | Creative Commons Attribution 3.0, plus Tanzil Terms of Use | https://tanzil.net/docs/Text_License (read 2026-10-13) | See below |
| GoogleTest v1.14.0 | C++ tests only (fetched by CMake, never linked into MutqinCore or the CLI) | BSD-3-Clause | `LICENSE` in the fetched source | |
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

## To verify before use (not yet used)

Names come from `docs/DATA_AND_MODEL.md` and are starting points only.

| Item | Intended use | License | Status |
|------|--------------|---------|--------|
| King Fahd Complex (QPC) fonts / text | Mushaf rendering in the UI (apps, later) | unknown | not checked |
| Tajweed annotation datasets (GitHub) | Validate the rule engine (Phase 1) | unknown | not checked |
| Quran.com API tajweed text | Validation (Phase 1) | unknown | not checked |
| Quranic Arabic Corpus | Word segmentation / morphology | unknown | not checked |
| EveryAyah.com reciter audio | Evaluation audio (Phase 2) | unknown | not checked |
| Tarteel AI datasets (Hugging Face) | Training / evaluation audio | unknown | not checked |
| Quran-fine-tuned CTC model (to be chosen) | Acoustic model (Phase 2) | unknown | not chosen |
| sherpa-onnx / ONNX Runtime / Silero VAD | Inference and VAD in the core (Phase 3) | unknown | not checked |
