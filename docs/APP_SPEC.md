# App specification (draft for review)

Status: **proposal**. Nothing here is built; the Android and iOS apps are deferred (`TASKS.md`, 3.5a).
Anything marked **Open question** needs a decision from the project owner (see the end). Everything that
refers to the core describes what exists today (`core/include/mutqin.h`, `ARCHITECTURE.md`).

## 1. Purpose and principles

A free, offline helper for learning and memorizing the Quran: the user chooses an ayah range, recites, and the
app shows, while they recite, where their recitation differs from the text.

- **Free, forever.** No ads, payments, subscriptions, accounts or tracking. This is also a condition of the
  recognition model's license (NPL-1.1).
- **Offline and private.** Audio is processed on the device and never stored or sent anywhere unless the user
  explicitly saves a recording. No network permission is needed at run time.
- **Prefer missing a mistake over a false alarm.** The app never claims a recitation is correct: a quiet screen
  means "nothing was detected", not "perfect".
- **Not a teacher.** The app says so on first start and in the about screen; tajweed review by a qualified
  teacher stays the authority. Rules beyond word level are not reported yet.
- Riwayah: Hafs 'an 'Asim. Text: the verified Tanzil Uthmani text, shipped unmodified.

## 2. Modes

1. **Follow mode** (default): the text of the range is shown; the current word is highlighted as the user
   recites; mistakes are colored.
2. **Hifz mode** (memorization): the text is hidden (or shown as placeholders per word). Each word appears as
   it is recognized; a skipped or wrong word is revealed with its mark so the user sees what they missed.
   A "peek" control shows the next word on demand.

Same engine in both; only the presentation differs.

## 3. Screens

1. **Welcome / first run:** what the app does, the "not a teacher" and privacy statements, microphone
   permission request (with the reason), one-time preparation of the model files (a progress bar).
2. **Home:** continue the last range, choose surah and ayah range, choose the mode.
3. **Range picker:** surah list; ayah from-to. The range is limited to about 900 words
   (`MQ_MAX_EXPECTED_PHONEMES`, more than a mushaf page); a longer choice is refused with an explanation.
4. **Recitation screen:** the text (or placeholders), a live status line, a record/stop control, a retry
   control. Details in section 4.
5. **Result screen** (after stop): the whole range with every judged word marked, a short list of what was
   found ("skipped: ayah 2 word 3"), "try again" and "next range".
6. **Settings:** text size, mode default, theme, microphone choice if several exist, about, licenses.
7. **About and licenses:** project license (Apache-2.0), Tanzil attribution, the model's NPL-1.1 text,
   font licenses, privacy statement, the "not a teacher" statement.

## 4. Live feedback: from the core's data to the screen

The app asks the core every 100-200 ms (UI thread; allowed while the worker thread pushes audio):

| Core call | Used for |
|---|---|
| `mq_session_position` | the item being recited now (highlight, scroll) |
| `mq_session_item` | which Tanzil words an item covers: `word` to `word_last` (an item can join several words where the reading merges them; highlight them together) |
| `mq_session_word_status` | per item: not judged yet / ok / wrong / skipped |
| `mq_session_event_count`, `mq_session_event` | skipped, wrong, repeated, added (with the item they refer to) |

Visual states (final colors and shapes are a design decision, **Open question** 5):

| State | Meaning | Shown as |
|---|---|---|
| upcoming | not recited yet | normal text |
| current | being recited now (not judged yet) | highlight |
| ok | judged, no difference found | normal or soft confirmation |
| wrong | heard something else for this word | red mark |
| skipped | the word was not heard | amber mark |
| repeated | the previous words were recited again | blue mark on the repeated words |
| added | extra speech after this word | grey marker between words |

Behaviour that the app must respect:

- **A word is judged only after the next one starts** (the core holds back the word being spoken). So a mark
  appears about **1-2 seconds** after the mistake. This is by design, to avoid flashing false marks.
- **Marks can be taken back.** Later audio can correct a verdict, so the app redraws from the core's current
  state on every poll (not from a history of events). A short "cleared" animation is enough.
- **The last word is judged at stop.** On stop the app calls `mq_session_finish_audio` and then shows the
  final state; this is what the result screen uses.
- **Starting in the middle** is fine: the core accepts that the user begins at any word of the range;
  earlier words simply stay not judged. For surahs other than 1 and 9 an optional basmala before ayah 1 is
  accepted and is reported as ayah 0 (do not show it as a mistake if it is missing).
- Pauses, hesitations and restarts of a word are tolerated; repeating words is reported as "repeated", which
  the user may do on purpose (the mark is informational, not an error count).
- Hifz mode reveals a word when its item becomes judged; wrong and skipped words are revealed with their mark.

## 5. Audio, threads and lifecycle

- Capture mono audio from the microphone at the device's rate (44.1 or 48 kHz is normal) and tell the core
  with `mq_session_set_input_rate`; the core resamples.
- Push from a **worker thread** in chunks of about 100 ms. Never push from the real-time audio callback: copy
  samples to a queue there. Poll from the UI thread.
- Use one engine for the app, one session per attempt. "Retry" calls `mq_session_reset` (keeps the loaded
  model); a new range creates a new session.
- The app handles: permission denied (explain, link to settings), a phone call or other app taking the
  microphone (stop and finish the session), headset plug/unplug, screen off (stop), low battery (no special
  handling in v1), device too slow (see section 8).
- Foreground use only in v1; no background recitation.

## 6. Data the app ships and where it comes from

| Item | Size | Notes |
|---|---|---|
| Recognition model (int8) | about 73 MB | NPL-1.1; must be copied out of the app package to a file once (sherpa-onnx needs a file path) |
| Expected-units table | about 2.3 MB | derived from the model files, so also NPL-1.1 |
| Quran text (Tanzil Uthmani + simple) and checksums | about 2.3 MB | handed to the core from memory (`mq_engine_create_from_memory`) or as files; verified by checksum |
| sherpa-onnx / ONNX Runtime libraries | roughly 20-35 MB per CPU type | split per CPU architecture in the store package |

Total near the 100 MB budget (`PLAN.md`). No downloads at run time: everything is in the install package
(**Open question** 7 on store size limits and on-demand delivery of the model).

## 7. Storage, history and privacy

- Settings and the last chosen range are stored locally.
- **History** (which ranges were practiced, when) is optional and local only. **Open question** 8.
- **Recordings** are not kept by default. If the user chooses to save a take, it stays on the device and can be
  deleted in the app.
- No analytics, no crash reporting service, no network calls. Diagnostics, if any, are copied to the clipboard by the user.

## 8. Errors and limits

| Situation | App behaviour |
|---|---|
| model files missing or damaged | explain, offer to prepare them again; never show results |
| no speech heard for a while | gentle prompt ("I cannot hear you"), no marks |
| very noisy room | quality hint; marks may be less reliable |
| range too long (`MQ_ERR_LIMIT`) | ask for a shorter range |
| the session heard far more than the range (`MQ_ERR_LIMIT` on push) | stop and offer a restart |
| device too slow (real-time factor above 1 measured at run time) | warn and fall back to "check after you finish" (push everything at stop) |
| unsupported OS or CPU | explain at install or first start |

## 9. Out of scope for the first version

Letter- and vowel-level feedback, tajweed rules, other qira'at, translations or tafsir, listening to reciters
(audio licenses), accounts, sharing and social features, cloud sync, payments of any kind, notifications,
widgets and wearables, languages of the UI beyond those chosen in **Open question** 4.

## 10. Technical sequence (for both platforms)

1. First run: copy the model files to app storage; build `mutqin_model.txt` pointing at them.
2. `mq_engine_create_from_memory(...)` (or `mq_engine_create(dir)`) once.
3. For a range: `mq_session_create`, `mq_session_set_input_rate`, `mq_session_load_model`.
4. Recording: worker thread calls `mq_session_push_audio` per chunk; the UI polls position, statuses and events.
5. Stop: `mq_session_finish_audio`, final poll, show the result screen.
6. Retry: `mq_session_reset`. New range or leaving: `mq_session_destroy`. App exit: `mq_engine_destroy`.

Platform layers: Android: Kotlin + JNI over the shared library, audio with Oboe or `AudioRecord`; iOS: Swift
over the C API (static library or xcframework), audio with `AVAudioEngine`. Packaging steps are in
`ARCHITECTURE.md`.

## 11. Milestones (order, no dates)

1. Android prototype: record, push, highlight the current word, show marks; range picker; follow mode.
2. Hifz mode, result screen, settings, first-run flow, licenses screen.
3. iOS app with the same behaviour on the same core.
4. Test with real learners (recordings and feedback with consent), tune, fix the false alarms they show.
5. Store submission (policies, descriptions, screenshots, privacy forms).

## 12. Open questions for the owner

1. **UI technology.** Recommended: native UI on each platform (Jetpack Compose on Android, SwiftUI on iOS) over the
   shared C++ core, because the core already has a plain C API and native text rendering of Arabic is the
   critical part. A cross-platform UI toolkit would save work but adds a second layer between the core and the
   screen.
2. **Name, icon, branding, store accounts and the developer identity** shown in the stores.
3. **Minimum OS versions** and whether tablets are supported.
4. **UI languages** (Arabic and English?) and right-to-left layout rules.
5. **Look of the marks** (colors, accessibility for color blindness, whether to show a score or count).
6. **Quran font and mushaf style** (the text is Tanzil's; a font must be chosen and its license checked, for example the
   King Fahd Complex fonts).
7. **Store package size:** keep the model in the install package (about 100 MB total) or deliver it on demand
   after install (needs a download, which breaks "no network", or the store's asset delivery).
8. **History and progress:** none, local only, or local with export. Recommended: local only, simple.
9. **Strictness setting:** expose a choice (more or fewer reports) once the core offers thresholds, or keep one fixed profile.
10. **Latency:** is a mistake showing 1-2 seconds after it is spoken acceptable? (The model itself looks ahead
    about 1 second.)
11. **Beta distribution:** who tests, how recordings are collected with consent, how feedback comes back.
12. **Disclaimers:** the wording of the "not a teacher" statement and who reviews it.
