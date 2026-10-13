# Tajweed Rule Engine Specification

**Scope:** Riwayat Hafs 'an 'Asim, by way of ash-Shatibiyyah.
**Status:** DRAFT — must be reviewed and signed off by a qualified teacher (with ijazah)
before any rule ships. Sources to verify against: Tuhfat al-Atfal, al-Muqaddimah
al-Jazariyyah, and a color-coded tajweed mushaf.

AI assistants: implement only what is written here. Unknown or unclear cases go to
"Open questions" at the bottom. Never guess.

## 1. Input and output

Input: Uthmani text of an ayah range (from the verified source file), plus context
(previous and next word, so rules across word boundaries work).

Output: list of expected phonemes. Each item has:

| Field | Meaning |
|-------|---------|
| `ph` | phoneme symbol (see inventory) |
| `word` | index of the source word |
| `char_span` | source characters in the Uthmani text |
| `rules` | list of rule tags (section 3) |
| `dur` | allowed duration in counts (harakat), e.g. `[2]`, `[4,5]`, `[2,4,6]` |
| `variant` | `both`, `continue_only`, or `waqf_only` |

## 2. Phoneme inventory (to finalize in Phase 1)

- 28 consonants, each with a stable ASCII symbol, plus hamza.
- Short vowels: a, i, u. Long vowels as duration on the vowel, not separate symbols.
- Markers: ghunnah (nasalized), tafkhim (heavy) vs tarqiq (light) for ra / lam / alif,
  qalqalah release.
- The model's output tokens = this inventory (possibly with heavy/light merged in early
  phases). Document the mapping in `tools/phonemizer/inventory.py`.

## 3. Rules

### 3.1 Noon sakinah and tanween

| Rule | Condition (next letter) | Realization |
|------|------------------------|-------------|
| Izhar halqi | ء ه ع ح غ خ | clear noon, no extra ghunnah |
| Idgham with ghunnah | ي ن م و | merge into next letter, ghunnah 2 counts |
| Idgham without ghunnah | ل ر | full merge, no ghunnah |
| Iqlab | ب | noon becomes meem, hidden, ghunnah 2 counts |
| Ikhfa haqiqi | ت ث ج د ذ ز س ش ص ض ط ظ ف ق ك | hidden noon, ghunnah 2 counts |

Exceptions:
- Idgham only applies **across two words**. Inside one word with ي or و after noon sakinah,
  it is izhar mutlaq (four words in the Quran — load the list from data, do not hardcode
  the Arabic from memory; to be confirmed with the teacher).
- Hafs: izhar of the noon in the openings of Surah Ya-Sin (36:1–2) and Surah al-Qalam (68:1).

### 3.2 Meem sakinah

| Rule | Condition | Realization |
|------|-----------|-------------|
| Ikhfa shafawi | next letter ب | hidden meem, ghunnah 2 counts |
| Idgham mithlayn saghir | next letter م | merge, ghunnah 2 counts |
| Izhar shafawi | all other letters | clear meem (especially careful before و and ف) |

### 3.3 Mushaddad noon and meem

Ghunnah, 2 counts.

### 3.4 Qalqalah

Letters ق ط ب ج د when sakin (with sukun, or sakin because of waqf).
- Sughra: sakin in the middle of the word or when continuing.
- Kubra: at waqf on that letter (stronger).

### 3.5 Lam

- Lam of the definite article: shamsiyyah (assimilated) vs qamariyyah (clear),
  determined by the next letter.
- Lam in the name of Allah: tafkhim after fatha or damma; tarqiq after kasra.

### 3.6 Ra — tafkhim / tarqiq

Phase 5 only. Start with the main cases (ra with fatha/damma → heavy, with kasra → light;
sakin ra depends on the previous vowel and following letter). Full case list to be written
together with the teacher.

### 3.7 Madd (durations in counts)

| Type | Condition | Allowed counts (Hafs, Shatibiyyah) |
|------|-----------|-----------------------------------|
| Tabi'i (natural) | long vowel, no hamza or sukun after | 2 |
| Muttasil (wajib) | hamza after long vowel, same word | 4, 5 (6 possible at waqf when hamza is last — confirm) |
| Munfasil (ja'iz) | hamza at start of next word | 4, 5 (user setting; consistent within a session) |
| Lazim | permanent sukun / shaddah after long vowel | 6 |
| Lazim harfi | certain disconnected letters at surah openings | 6 (list from data, confirm with teacher) |
| 'Arid lis-sukun | sukun only due to waqf | 2, 4, 6 — waqf_only |
| Leen | و / ي sakin after fatha, then waqf | 2, 4, 6 — waqf_only |
| Badal | hamza before the long vowel | 2 |
| 'Iwad | tanween fath at waqf → alif | 2 — waqf_only |
| Silah sughra | ha of the pronoun between two vowels | 2 |
| Silah kubra | same, followed by hamza | 4, 5 (like munfasil) |

Duration is measured relative to tempo (section 4).

### 3.8 Idgham of adjacent letters (mutamathilayn, mutajanisayn, mutaqaribayn)

Implement the cases listed by the teacher, including incomplete idgham where a quality
remains (e.g. ط before ت keeps its itbaq). To be written as a table in Phase 1 review.

### 3.9 Special cases in Hafs (handle individually, location-based)

- Sakt (brief pause without breath) at four places: 18:1–2, 36:52, 75:27, 83:14.
- Imalah in one word in 11:41.
- Tas-hil of the second hamza in 41:44.
- Ishmam in 12:11.
- Words with two valid readings (e.g. ضعف in 30:54; sin/sad words in 2:245, 7:69, 52:37,
  88:22): accept all valid readings.
- Hamzat al-wasl: pronounced when starting, dropped when continuing; starting vowel rules.

Store these as a data file keyed by `surah:ayah:word`, verified by the teacher. Do not
hardcode Arabic text.

## 4. Tempo and duration

- Estimate the reciter's count length from short vowels and tabi'i madd in the last
  N seconds (rolling median).
- Support three speeds: tahqiq (slow), tadwir (medium), hadr (fast).
- A madd is correct if its measured length, divided by count length, falls within the
  allowed set ± tolerance (start with ±0.75 count, tune on data).
- Consistency: if the user chose 4 counts for munfasil, flag mixing 4 and 5 only as a
  low-severity hint, never as an error.

## 5. Waqf (stopping)

- The engine produces both variants at every word boundary where stopping is possible.
- At runtime, a VAD pause > threshold at a word boundary selects the waqf variant.
- Waqf rules: final vowel becomes sukun (except tanween fath → alif), ta marbuta → ha,
  'arid lis-sukun and leen durations apply, qalqalah kubra.
- Waqf signs in the mushaf are guidance, not errors; never flag a stop as a mistake in
  Phase 1–5 (possible later as a gentle hint at forbidden stops, with teacher approval).

## 6. Validation

- Compare rule tags with an open tajweed annotation dataset over the full Quran; every
  disagreement must be resolved (engine bug, dataset bug, or open question).
- Manual check against a color-coded tajweed mushaf for Al-Fatiha, Juz' 'Amma,
  Al-Baqarah 1–50.
- Teacher review of a printed report: ayah, phonemes, rules, durations.

## Open questions (add here, do not guess)

- [x] Final phoneme symbol set and heavy/light handling in early phases. Resolved 2026-10-13: see `docs/PHONEME_INVENTORY.md` (31 symbols, heavy/light merged until Phase 5).
- [ ] Full list for idgham mutajanisayn/mutaqaribayn cases.
- [ ] Complete ra rules.
- [ ] Muttasil at waqf: allow 6?
- [ ] Meaning of the Quranic small signs as encoded in the Tanzil Uthmani text, to confirm with
  the teacher before the phonemizer interprets them (task 1.2 only parses them, losslessly):
  U+06DC, U+06DF, U+06E0, U+06E2, U+06E3, U+06E7, U+06E8, U+06EA, U+06EB, U+06EC, U+06ED
  (combining), U+06E5 small waw and U+06E6 small yeh (non-combining), U+0653 maddah above,
  U+0654 hamza above, U+0670 superscript alef, U+0640 tatweel.
- [ ] Orthography to phoneme mapping for letter variants: alef (long a vs hamza seat vs silent),
  alef maksura, teh marbuta (teh vs heh at waqf), hamza on alef / waw / yeh, alef wasla
  (U+0671), and which letters are silent. Task 1.3 needs a written table; none is in this spec.
- [ ] DRAFT answers for the two questions above are in `docs/DRAFT_ORTHOGRAPHY_TABLES.md`
  (unverified; not to be implemented before confirmation and merge into this spec).
- [ ] New special case: U+06E8 small high noon occurs once, at 21:88:7, and is not listed in
  section 3.9. Confirm what it marks.
- [ ] Lam of Allah (spec 3.5, task 1.4): tafkhim after fatha / damma, tarqiq after kasra is not
  implemented. It only changes tags (heavy / light is merged until Phase 5) and identifying the
  name of Allah needs a teacher-approved list of word ids. Deferred to Phase 5.
- [ ] Starting vowel of hamzat al-wasl for non-article words (task 1.4 implements only the article,
  fatha). The usual rule depends on whether the third letter's damma is original or transient;
  it needs the teacher. 1,488 words are reported as `wasla_start_vowel_unknown`.
- [ ] Spec change request: `variant` gets a fourth value `start_only` (phonemes that exist only when
  a word starts a recitation or follows a stop; used for hamzat al-wasl). Confirm.
- [ ] Article lam before a verb: the 10 words with alef wasla + lam sukun + teh are verbs, not the
  definite article, but are tagged `lam_qamariyyah` (the encoding cannot tell them apart).
- [ ] Task 1.5 follows 3.1 by the next letter and checks it against the text: izhar noons carry a
  sukun, ikhfa / idgham noons are bare, iqlab noons carry the meem sign (all agree over the whole
  Quran). The one exception, 75:27:2 (noon with a sukun before a letter that would merge it), is the
  sakt location from 3.9 and is reported, not handled, until the special-cases data (1.9) exists.
- [ ] Idgham with ghunnah: the doubled letter is emitted as two consonants, the second tagged
  `ghunnah` duration (2,) and marked `after_continue`. Confirm this representation.
- [ ] Task 1.7 (madd) decisions made from the data, to confirm with the teacher:
  - A long vowel followed by a sakin letter in the *next* word (for example the article lam when
    reading on) is treated as tabi'i, 2 counts. The spec table does not cover it.
  - A hamza that directly follows hamzat al-wasl in the next word does not make the previous long
    vowel munfasil: the text writes no maddah there (5 words, e.g. 2:283:15).
  - Silah: the small waw / small yeh after the pronoun heh is silah sughra (2) or, before a
    hamza, silah kubra (4, 5). It exists only when reading on.
  - Muttasil and munfasil are emitted as the allowed set (4, 5); the user's 4-or-5 setting is applied
    later. 6 for muttasil at a stop is still the open question above.
  - 'Arid lis-sukun and leen are stop-only readings of (2, 4, 6) on the long vowel or leen letter
    that comes before the last letter of the word.
  - Text anomalies: 2:72:4 has a muttasil without a written maddah; 41:44:9 (tas-hil) is a lazim
    candidate without a maddah, left to the special-cases data (1.9).
  - Lazim harfi (letter names at surah openings) is not produced yet; those 30 words are skipped.
  - Checked over the whole Quran: the written maddah appears exactly on the vowels the engine gives
    muttasil, munfasil, lazim or silah kubra (apart from the two anomalies above), and never on a
    tabi'i, badal or silah sughra vowel.
