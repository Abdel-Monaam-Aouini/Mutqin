# DRAFT: orthography to phoneme tables (NOT part of the spec yet)

**Status: DRAFT, UNVERIFIED. Owner asked for it to be implemented for testing on 2026-10-13 ("implement, I'll test"); `tools/phonemizer/phonemize.py` follows these tables and is provisional until a teacher confirms them.** Proposed 2026-10-13 to answer the two open questions in
`docs/TAJWEED_SPEC.md` (meaning of the small signs; letter-variant mapping). AGENTS.md says to
implement only what the spec contains, so **nothing here may be implemented until the owner or
the teacher confirms a row and it is moved into the spec.**

## Sources and how far they go

| Evidence | What it gives | Limit |
|----------|---------------|-------|
| Unicode 18 Arabic chart (U0600.pdf) | official names of every sign | names only, almost no semantics |
| Unicode L2/2005/05150 (Azhar 1924 Quran marks) | U+06DF means "ignore skeleton letter" (letter written, not pronounced); its Unicode name is a literal translation of the mushaf legend | covers only a few signs |
| Unicode L2/2016/16268 (Al-Dani marks) | U+06E2 and U+06ED are the meem used on tanween "in case of iqlab" | Al-Dani/Libya context, not Hafs-specific |
| Statistics computed from `data/quran/` by `tools/phonemizer/marks_report.py` | where each sign occurs (host letter, vowel, position, neighbours) | shows patterns, not rules |
| Spec section 3.9 special-case locations | matches three unique signs exactly (below) | |
| **al-Jazariyyah and ash-Shatibiyyah** (requested as the authority) | **not yet consulted.** I could not retrieve a usable digital text online and will not quote verses from memory | the teacher must verify every row against them, or put a digital copy in `docs/sources/` so rows can cite lines |

Confidence: **High** = Unicode name or spec text plus a consistent data pattern. **Medium** =
consistent data pattern, meaning inferred. **Verify** = needs the teacher before use.

## A. Base characters

| Char | Draft mapping | Evidence | Conf. |
|------|---------------|----------|-------|
| U+0621 hamza | `hz` | letter | High |
| U+0623, U+0625 alef + hamza above / below; U+0624 waw + hamza; U+0626 yeh + hamza | `hz` (the seat letter adds no phoneme); the vowel comes from the unit's own mark | Unicode: canonical decomposition = base + hamza mark | High |
| U+0627 alef, no sign | long `a`: extends the preceding fatha, `dur` (2,) as tabi'i unless a madd rule in spec 3.7 applies | 18,450 unmarked alefs, nearly always after a fatha | Medium |
| U+0627 alef carrying U+06DF | silent (not pronounced) | Unicode proposal: "ignore skeleton letter"; 3,722 cases, mostly word-final | High |
| U+0627 alef carrying U+06E0 | silent when continuing; pronounced at waqf (variant `waqf_only`) | 66 cases, all word-final. **Meaning of the rectangular zero is not in any source I could read** | Verify |
| U+0627 alef after tanween fatha | tanween alef: nothing when continuing; long `a` at waqf (`waqf_only`, dur (2,), spec 3.7 'iwad) | 87 cases of tanween + U+06E2 followed by a plain alef, plus the tanween pattern | Medium |
| U+0671 alef wasla | when continuing: silent. When starting: `hz` + starting vowel | spec 3.9 (hamzat al-wasl). 10,772 word-initial, 12k before lam (definite article) | High for the shape, **Verify** for the starting vowel (not in spec) |
| U+0649 alef maksura | after fatha (with or without U+0670): long `a`. After kasra: long `i` (yeh-shaped). With sukun or its own vowel: consonant `y` | Unicode: "yeh-shaped letter with no dots"; previous-unit stats | Medium |
| U+0629 teh marbuta | continuing: `t` + its vowel (or tanween). At waqf: `h`, vowel dropped | spec section 5 | High |
| U+0640 tatweel | no phoneme; only a carrier for marks (U+0670, U+0654, U+0653, U+06E7) | Unicode: "carry tashkil with no base letter"; 6,736, every one carries marks | High |
| U+06E5 small waw | long `u`; when it has U+0653 on it and a hamza follows: silah kubra (spec 3.7) | 1,228 after heh + damma, 27 after waw + damma; 165 carry maddah, next letter hamza ~165 | Medium |
| U+06E6 small yeh | long `i`, same logic as small waw | 935 after heh + kasra; 162 with maddah | Medium |
| waw with no sign, after damma | long `u` (madd letter) | 9,196 bare waws, 8,749 after damma | Medium |
| yeh with no sign, after kasra | long `i` (madd letter) | 6,932 bare yehs, 6,780 after kasra | Medium |
| waw / yeh with no sign, after fatha | **unknown** (leen? silent?) | only 22 and 1 cases | Verify |
| lam with no sign, before a letter with shaddah | assimilated (shamsiyyah): `l` not pronounced, next consonant doubled | 5,393 bare lams, all followed by shaddah, 5,151 after alef wasla | High (matches spec 3.5) |
| noon / meem with no sign | the encoding marks assimilation or hiding by the absence of sukun; **the rule must still come from the next letter per spec 3.1 / 3.2**, with the encoding used only as a cross-check | 5,140 bare noons, next letters are ikhfa / idgham letters | Medium |

## B. Combining marks

| Mark | Draft mapping | Evidence | Conf. |
|------|---------------|----------|-------|
| fatha, kasra, damma (064E, 0650, 064F) | vowel `a`, `i`, `u` after the consonant | inventory | High |
| fathatan, kasratan, dammatan (064B-064D) | vowel `a`, `i`, `u` + `n`; at waqf see 'iwad / spec section 5 | spec 3.7, section 5 | High |
| shaddah (0651) | consonant emitted twice | inventory convention | High |
| sukun (0652) | no vowel | inventory convention | High |
| U+0670 superscript alef | long `a`. When its host is waw and the waw has no vowel of its own, the waw is silent and only the long `a` is pronounced | Unicode: "actually a vowel sign"; 9,726 cases | Medium; the waw case **Verify** |
| U+0653 maddah above | marks a lengthening beyond 2 counts. The madd type (muttasil, munfasil, lazim, silah kubra) must be decided from context per spec 3.7, using the sign only as a cross-check | 5,376 cases, next letter is hamza in about 4,500 | Medium |
| U+0654 hamza above | `hz`; on tatweel it is a hamza with no letter seat | 772 of 773 on tatweel | High |
| U+06DF | silent letter (see A) | Unicode proposal | High |
| U+06E2 (above) and U+06ED (below) | iqlab: the tanween or noon becomes meem, hidden, ghunnah 2 counts (spec 3.1) | Al-Dani proposal; 510 + 99 cases, next letter is beh in about 80% of them; the rest are before a tanween alef or end of ayah | High (matches spec 3.1) |
| U+06E7 small high yeh (on tatweel) | long `i` | 38 cases, all on tatweel inside a word | Verify |

## C. Signs that mark special cases (location based, spec 3.9)

These occur once or twice and **line up exactly with the locations the spec already lists**, which
supports reading them as the marks for those cases. Data goes in the special-cases file (task
1.9), keyed by `surah:ayah:word`, never hardcoded.

| Sign | Location found | Spec 3.9 entry |
|------|----------------|----------------|
| U+06EA empty centre low stop | 11:41:6 | imalah, 11:41 |
| U+06EB empty centre high stop | 12:11:6 | ishmam, 12:11 |
| U+06EC rounded high stop with filled centre | 41:44:9 | tas-hil of the second hamza, 41:44 |
| U+06DC small high seen | 2:245:14, 7:69:22 | sin / sad words, 2:245 and 7:69 |
| U+06E3 small low seen | 52:37:7 | sin / sad words, 52:37 |
| **U+06E8 small high noon** | **21:88:7** | **not in spec 3.9: new case, added to Open questions** |

## D. Standalone signs (already split off as `Word.marks` by the loader)

U+06D6-U+06DB are waqf guidance signs (spec section 5: never an error), U+06E9 is the place of
sajdah. U+06DD (end of ayah) and U+06DE (rub el hizb) are not present in our download options.

## What I need from you or the teacher

1. Confirm or correct every **Medium** and **Verify** row, checking against al-Jazariyyah,
   ash-Shatibiyyah and a colour-coded mushaf. A digital copy in `docs/sources/` would let me cite lines.
2. Starting vowel of hamzat al-wasl (only the definite article is clear from the data).
3. The meaning of U+06E0 and U+06E7, and what a waw / yeh with no sign after a fatha is.
4. The new special case at 21:88:7.

## Additions found while implementing task 1.3 (all from the Tanzil data; Verify)

| Case | Implemented as | Evidence |
|------|----------------|----------|
| Yeh-shaped letter (U+0649) with its own vowel, shaddah or sukun | consonant `y` (+ vowel) | 843 cases, mostly after kasra / fatha, final or before hamza |
| Alef maksura after tanween fatha | tanween alef (same as alef after tanween fatha) | 93 cases, previous unit is a tanween |
| Small waw / small yeh with its own vowel | consonant `w` / `y` + vowel | 1 case (small yeh + fatha) |
| Tatweel carrying small high yeh and its own kasra | consonant `y` + `i` | 4 cases |
| Hamza above on a non-tatweel host | `hz` emitted in mark order after the long a | 1 case (2:72:4) |
| Bare waw after fatha before a silent alef (U+06DF) | consonant `w` with tag `implicit_sukun` | 22 of 23 cases |
| Word with no vowel, tanween, sukun or shaddah on any letter | disconnected letters (surah openings): skipped, letter names come from task 1.9 data | finds exactly 30 words: ayah 1 word 1 of 29 surahs and 42:2:1 |
| 11:41:6, 68:6:1, 83:3:3 | not handled; reported as diagnostics for task 1.9 | one case each |
