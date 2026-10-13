---
name: verify-quran-data
description: Verify the integrity of the Quran source files in data/quran/ (SHA-256 checksums, 114 surahs, 6236 ayahs). Use before tasks that read Quran text and when results look suspicious.
---

# verify-quran-data

1. Check `data/quran/checksums.txt` exists; run `sha256sum -c checksums.txt` from `data/quran/`.
2. Run the loader tests (Python and C++) and confirm 114 surahs / 6236 ayahs.
3. Report PASS/FAIL per file. On any mismatch: stop, do not "repair" files, tell the user.
   Never print or paste Quran text into the report; refer to (surah, ayah) only.
