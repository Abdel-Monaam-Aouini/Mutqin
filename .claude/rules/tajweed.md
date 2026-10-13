---
paths:
  - "tools/phonemizer/**"
  - "core/src/tajweed/**"
  - "docs/TAJWEED_SPEC.md"
---

# Tajweed rules

- Implement only what `docs/TAJWEED_SPEC.md` states. Nothing from general knowledge.
- Gap or ambiguity → append to "Open questions" in the spec and ask the user. Do not guess.
- Special cases (sakt, imalah, ishmam, tas-hil, izhar mutlaq word list, lazim harfi list) live in a
  data file keyed by `surah:ayah:word`, filled with the teacher. Never hardcode Arabic.
- Where several durations/readings are valid, accept all.
- The spec is DRAFT until a qualified teacher signs it off; nothing ships before that.
