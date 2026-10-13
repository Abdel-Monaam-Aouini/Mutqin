---
paths:
  - "data/**"
  - "tools/quran_text/**"
  - "core/src/text/**"
  - "**/tests/**"
---

# Quran text rules

- Arabic Quran text only ever comes from the verified Tanzil files in `data/quran/`.
- Do not type Arabic Quran words in code, tests, fixtures, comments, or docs examples.
  Tests address text by `(surah, ayah[, word])` and read it through the loader.
- Do not normalize, re-encode, strip, or "fix" the source files. Parsing happens in memory only.
- Verify SHA-256 against `data/quran/checksums.txt` on every load; fail loudly on mismatch.
- Stable word ID format: `surah:ayah:word` (1-based).
- Expected counts: 114 surahs, 6236 ayahs.
- Non-Quran Arabic (UI strings, letter names) is allowed only if clearly not a Quran quote, and
  prefer ASCII/English identifiers anyway.
