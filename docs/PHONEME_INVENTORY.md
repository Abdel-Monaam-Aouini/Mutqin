# Phoneme inventory

Source of truth: `tools/phonemizer/inventory.py` (this page documents it). Approved by the
project owner on 2026-10-13. Applies to the Python reference phonemizer and, later, the C++ port
and the acoustic model tokens.

## Symbols (31)

| Symbol | Letter | Unicode | | Symbol | Letter | Unicode |
|--------|--------|---------|-|--------|--------|---------|
| hz | hamza | U+0621 | | S | sad | U+0635 |
| b | beh | U+0628 | | D | dad | U+0636 |
| t | teh | U+062A | | T | tah | U+0637 |
| th | theh | U+062B | | Z | zah | U+0638 |
| j | jeem | U+062C | | E | ain | U+0639 |
| H | hah | U+062D | | gh | ghain | U+063A |
| kh | khah | U+062E | | f | feh | U+0641 |
| d | dal | U+062F | | q | qaf | U+0642 |
| dh | thal | U+0630 | | k | kaf | U+0643 |
| r | reh | U+0631 | | l | lam | U+0644 |
| z | zain | U+0632 | | m | meem | U+0645 |
| s | seen | U+0633 | | n | noon | U+0646 |
| sh | sheen | U+0634 | | h | heh | U+0647 |
| a | short vowel (fatha) | | | w | waw | U+0648 |
| i | short vowel (kasra) | | | y | yeh | U+064A |
| u | short vowel (damma) | | | | | |

27 consonants + hamza + 3 short vowels = 31 symbols. The CTC vocabulary adds `<blank>` at id 0
(32 tokens); token ids follow the order in `inventory.py`, which is part of the contract.

Alif is not a consonant phoneme: it is a long /a/ or a hamza seat, handled in task 1.3.
Letter variants (hamza on alif / waw / yeh, alif maqsura, ta marbuta, ...) are mapped to these
symbols in tasks 1.2 and 1.3, following `docs/TAJWEED_SPEC.md`.

## Conventions

- **Long vowels**: the short-vowel symbol plus a longer allowed duration in `dur` (counts).
  There are no separate long-vowel symbols.
- **Shaddah**: the consonant is emitted twice. **Sukun**: no vowel is emitted.
- **Rules are tags, not tokens**: ghunnah, qalqalah, ikhfa, idgham, iqlab, madd types... are
  strings in `ExpectedPhoneme.rules`. Phase 4 may split tokens (e.g. nasalized variants) if the
  data shows they are needed.
- **Heavy / light (tafkhim / tarqiq)** is merged in early phases: `r` and `l` have no variants
  until Phase 5.

## Output record (`ExpectedPhoneme`)

| Field | Type | Meaning |
|-------|------|---------|
| `ph` | str | a symbol from the table |
| `word` | int | 1-based word index in the ayah (same as the word id) |
| `char_span` | (int, int) | `[start, end)` Unicode code point offsets inside the Uthmani word text |
| `rules` | tuple[str] | rule tags (defined with each rule, `TAJWEED_SPEC.md` section 3) |
| `dur` | tuple[int] | allowed durations in counts, e.g. `(2,)`, `(4, 5)`, `(2, 4, 6)` |
| `variant` | enum | `both`, `continue_only`, `waqf_only` |
