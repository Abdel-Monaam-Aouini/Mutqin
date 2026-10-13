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

- [ ] Final phoneme symbol set and heavy/light handling in early phases.
- [ ] Full list for idgham mutajanisayn/mutaqaribayn cases.
- [ ] Complete ra rules.
- [ ] Muttasil at waqf: allow 6?
