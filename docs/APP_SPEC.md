# App specification

Status: **decisions taken, nothing built yet.** The Android and iOS apps are deferred (`TASKS.md`, 3.5a); the
core is prepared for them. The owner answered the open questions on 2026-10-14; they are recorded in section 12.
Everything that refers to the core describes what exists today (`core/include/mutqin.h`, `ARCHITECTURE.md`).

## 1. Purpose and principles

A free, offline helper for learning and memorizing the Quran: the user chooses an ayah range, recites, and the
app shows, while they recite, where their recitation differs from the text.

- **Free, forever.** No ads, payments, subscriptions, accounts or tracking. This is also a condition of the
  recognition model's license (NPL-1.1).
- **Private and offline.** Audio is processed on the device and never stored or sent anywhere unless the user
  explicitly shares a recording. After the one-time download of the model (section 6) the app needs no network.
- **Prefer missing a mistake over a false alarm.** A quiet screen means "nothing was detected", not "perfect".
- Riwayah: Hafs 'an 'Asim. Text: the verified Tanzil Uthmani text, shipped unmodified.

## 2. Modes

1. **Follow mode** (default): the text of the range is shown; the current word is highlighted as the user
   recites; mistakes are marked.
2. **Hifz mode** (memorization): the text is hidden (or shown as placeholders per word). Each word appears as
   it is recognized; a skipped or wrong word is revealed with its mark so the user sees what they missed.
   A "peek" control shows the next word on demand.

Same engine in both; only the presentation differs.

## 3. Screens

1. **First run:** what the app does and the privacy statement, microphone permission request (with the reason),
   download and preparation of the model (section 6) with a progress bar.
2. **Home:** continue the last range, choose surah and ayah range, choose the mode.
3. **Range picker:** surah list; ayah from-to. The range is limited to about 900 words
   (`MQ_MAX_EXPECTED_PHONEMES`, more than a mushaf page); a longer choice is refused with an explanation.
4. **Recitation screen:** the text (or placeholders), a live status line, a record/stop control, a retry
   control. Details in section 4.
5. **Result screen** (after stop): the whole range with every judged word marked, a short list of what was
   found ("skipped: ayah 2, word 3"), "try again" and "next range".
6. **History:** the ranges practiced, when, and what was found, stored on the phone (section 7).
7. **Settings:** language, text size, theme (light, dark, system), default mode, strictness (section 8),
   microphone choice if several exist, storage (model, recordings), about and licenses.
8. **About and licenses:** project license (Apache-2.0), Tanzil attribution, the model's NPL-1.1 text, font
   license, privacy statement.

## 4. Live feedback: from the core's data to the screen

The app asks the core every 100-200 ms (UI thread; allowed while the worker thread pushes audio):

| Core call | Used for |
|---|---|
| `mq_session_position` | the item being recited now (highlight, scroll) |
| `mq_session_item` | which Tanzil words an item covers: `word` to `word_last` (an item can join several words where the reading merges them; mark them together) |
| `mq_session_word_status` | per item: not judged yet / ok / wrong / skipped |
| `mq_session_event_count`, `mq_session_event` | skipped, wrong, repeated, added (with the item they refer to) |

Behaviour that the app must respect:

- **A word is judged only after the next one starts** (the core holds back the word being spoken). So a mark
  appears about **1-2 seconds** after the mistake. The owner accepts this for now and wants it improved later.
- **Marks can be taken back.** Later audio can correct a verdict, so the app redraws from the core's current
  state on every poll, not from a history of events. A short fade is enough.
- **The last word is judged at stop.** On stop the app calls `mq_session_finish_audio` and shows the final
  state; the result screen uses it.
- **Starting in the middle** is fine: earlier words simply stay not judged. For surahs other than 1 and 9 an
  optional basmala before ayah 1 is accepted and reported as ayah 0 (do not show it as a mistake if missing).
- Pauses, hesitations and restarts of a word are tolerated; repeating words is reported as "repeated", which
  the user may do on purpose (the mark is informational, not an error count).
- Hifz mode reveals a word when its item becomes judged; wrong and skipped words are revealed with their mark.

## 5. Visual design (decided: professional, few colors)

**Few colors.** Two neutrals and two hues, no rainbow. Problems are told apart by **shape**, not by color, which
also works for color-blind users and in grayscale.

| Role | Light theme | Dark theme |
|---|---|---|
| Background / text | `#F4F1EA` / `#1B1F1E` | `#14181A` / `#ECE8DF` |
| Accent (brand, current word, ok) | deep green `#0F5C4D` | `#5FBFA6` |
| Attention (every kind of problem) | muted terracotta `#B4533A` | `#E0866C` |

| State | Marking |
|---|---|
| upcoming | normal text |
| current | accent background tint on the word |
| ok | normal text (optionally a faint accent underline when the word is confirmed) |
| wrong | attention color, solid underline |
| skipped | attention color, dotted underline under the word's place and a small gap marker (in hifz mode the word is shown with this mark) |
| repeated | attention color, double underline under the repeated words |
| added | small attention-colored caret between the words |

Typography: the Quran text in the Uthmani script (section 12, item 6), large, right to left, generous line
spacing; UI text in the platform's system font. Motion: short, quiet fades; no sounds by default.
Icon and name: section 11.

## 6. Data the app ships and how the model arrives

| Item | Size | Notes |
|---|---|---|
| Recognition model (int8) | about 73 MB | NPL-1.1; **delivered after install** (below) |
| Expected-units table | about 2.3 MB | derived from the model files, so also NPL-1.1; delivered with the model |
| Quran text (Tanzil Uthmani + simple) and checksums | about 2.3 MB | in the install package; handed to the core from memory (`mq_engine_create_from_memory`) or as files; verified by checksum |
| sherpa-onnx / ONNX Runtime libraries | roughly 20-35 MB per CPU type | in the install package, split per CPU architecture |

**Model delivery** (decided: after install; the design is not the focus now). For development, the model files
are placed on the phone by hand (adb / Xcode) into the app's storage. For release one of these will be chosen
later: the store's own asset delivery (Play Asset Delivery, App Store on-demand resources), or a one-time
download over HTTPS from a static host with a SHA-256 check, resume, a Wi-Fi hint and a storage check. The
NPL-1.1 license lets the model be redistributed only under the same license, free of charge, with its license
file kept. The network is used for this download only; recitation stays offline.

## 7. Storage, history, recordings and the beta

- Settings, the last range and the **history** are stored **locally on the phone** (local database or files),
  never uploaded. The user can clear them.
- **Recordings** are not kept by default. The user may save a take; it stays on the device and can be deleted.
- **Beta sharing** (decided: yes): for learner testing the user can choose to share a saved take and what was
  detected, with an explicit consent screen, through the system share sheet (nothing is uploaded in the
  background). Shared takes are used only to measure and tune the checker.
- No analytics, no crash-reporting service, no ads SDK. Diagnostics, if any, are copied by the user.

## 8. Strictness, errors and limits

**Strictness** (decided: yes). Three levels, default "standard": relaxed (fewer reports), standard, strict (more
reports). The core offers it: `mq_session_set_strictness` (any time; what was heard so far is judged again). The
three profiles were measured on real recordings (`EVAL_RESULTS.md`): relaxed raises the fewest false alarms but
finds fewer wrong and added words; strict finds almost everything with more stray reports. Explain the choice in
the settings screen in plain words ("fewer reports" / "more reports"), not with numbers.

| Situation | App behaviour |
|---|---|
| model files missing, damaged or still downloading | explain, offer to prepare or download them again; never show results |
| no speech heard for a while | gentle prompt ("I cannot hear you"), no marks |
| very noisy room | quality hint; marks may be less reliable |
| range too long (`MQ_ERR_LIMIT`) | ask for a shorter range |
| the session heard far more than the range (`MQ_ERR_LIMIT` on push) | stop and offer a restart |
| device too slow (real-time factor above 1 measured at run time) | warn and fall back to "check after you finish" (push everything at stop) |
| unsupported OS or CPU | explain at install or first start |

## 9. Audio, threads and lifecycle

- Capture mono audio from the microphone at the device's rate (44.1 or 48 kHz is normal) and tell the core
  with `mq_session_set_input_rate`; the core resamples.
- Push from a **worker thread** in chunks of about 100 ms. Never push from the real-time audio callback: copy
  samples to a queue there. Poll from the UI thread (the core serialises calls on a session).
- One engine for the app, one session per attempt. "Retry" calls `mq_session_reset` (keeps the loaded model);
  a new range creates a new session.
- The app handles: permission denied (explain, link to settings), a phone call or another app taking the
  microphone (stop and finish the session), headset plug/unplug, screen off (stop).
- Foreground use only in v1.

Technical sequence (both platforms):

1. First run: get the model files into app storage; write `mutqin_model.txt` pointing at them.
2. `mq_engine_create_from_memory(...)` (or `mq_engine_create(dir)`) once.
3. For a range: `mq_session_create`, `mq_session_set_input_rate`, `mq_session_load_model`.
4. Recording: the worker thread calls `mq_session_push_audio` per chunk; the UI polls position, statuses, events.
5. Stop: `mq_session_finish_audio`, final poll, result screen.
6. Retry: `mq_session_reset`. New range or leaving: `mq_session_destroy`. App exit: `mq_engine_destroy`.

## 9a. Older phones (decided: supported)

Minimum Android 5.0 (API 21) and iOS 13; target the latest stable releases. What this means:

- **CPU types.** Ship `arm64-v8a` and `armeabi-v7a` (32-bit phones) for Android; iOS devices are 64-bit.
  Each adds roughly 20-35 MB of runtime libraries, delivered per CPU type by the store.
- **Speed.** Old CPUs may run the model slower than real time. At first start the app measures the real-time
  factor on a short built-in sample (a mid-range phone measured 0.14, so only genuinely slow devices need the fallback). Above about 0.8 it switches to "check after you finish" (the audio is
  pushed when the user stops, the result comes some seconds later); between 0.3 and 0.8 it warns that marks
  will lag. The thread count for the model follows the number of cores.
- **Memory.** The model needs about 73 MB on disk plus ONNX Runtime working memory; on phones with little RAM
  the app checks available memory before starting and explains if it cannot run. Measured on a mid-range phone (Galaxy A15): 194 MB peak for the whole process; old devices still to be measured.
- **Arabic text.** Old Android and iOS versions shape Arabic and the small Quranic marks less reliably: the
  bundled Uthmani font must be rendered and checked on the oldest supported systems, not only on new ones
  (section 12, item 6). If a mark cannot be rendered correctly on a very old version, that version's minimum is
  raised rather than showing wrong text.
- **UI toolkits.** Only APIs available on the minimum versions are used (SwiftUI on iOS 13 lacks several later
  conveniences, for example `NavigationStack`; use `NavigationView` and conditional newer features).
- **Network.** The one-time model download uses HTTPS (TLS 1.2); the store delivery option avoids it.
- **Testing.** At least one physical phone near each minimum (an Android 5-7 and an iOS 13-14 device, or the
  oldest the owner has) before a release; emulators are not enough for speed.

## 10. Localization (decided: yes)

- Every user-visible string lives in a language file from the start, never in code: Android
  `res/values/strings.xml` plus `values-<lang>/`, iOS `Localizable.xcstrings`. Start with English and Arabic;
  other languages (French, ...) are added later as new files without code changes.
- Arabic is right-to-left; layouts mirror automatically (leading/trailing, not left/right). The Quran text is
  always right-to-left and is never translated or reflowed by the UI language.
- Plurals and number/date formats follow the platform's rules. Error codes of the core are mapped to
  localized messages in the app (the core's messages are English and for developers only).

## 11. Name, icon, packaging identity (proposal)

- **Name:** Mutqin, written together with the Arabic name (متقن). Tagline idea: "Recite with itqan."
- **Icon:** `docs/branding/icon.svg` (draft): a closed ring (mastery) around three sound bars (the voice) on
  deep green, two colors only; a single-color version for monochrome uses. To be refined by a designer if wanted.
- **Identifiers:** Android `applicationId` and iOS bundle identifier `app.mutqin` (change if the project gets a
  domain of its own). These cannot be changed after the first store release.
- **Store accounts** must be created by the owner: a Google Play developer account (one-time fee) and an Apple
  Developer Program membership (yearly fee; Apple offers fee waivers for qualifying non-profit organizations).
  Check the current prices and eligibility on the stores' sites. Developer name and contact e-mail are shown
  publicly in the stores.

## 12. Decisions (2026-10-14)

| # | Question | Decision |
|---|---|---|
| 1 | UI technology | **Native**: Jetpack Compose on Android, SwiftUI on iOS, over the shared C++ core |
| 2 | Name, icon, branding | proposal in section 11; store accounts to be created by the owner |
| 3 | Minimum OS | **Old phones are supported** (owner, 2026-10-14): Android 5.0 (API 21) and iOS 13, the lowest that sherpa-onnx's own Android demos (`minSdk 21`) and iOS build (deployment target 13.0), Jetpack Compose (API 21) and SwiftUI (iOS 13) allow. Raised only if tests on old devices show a problem (section 9a). Phones first; tablets work through adaptive layouts |
| 4 | UI languages | **Localization from the start** (section 10); English and Arabic first, others as language files later |
| 5 | Look of the marks | left to the project: **professional, few colors**, shapes instead of colors (section 5) |
| 6 | Quran script and font | **Uthmani script** (the Tanzil Uthmani text). Font candidates, both under the SIL Open Font License 1.1 (checked on GitHub): Amiri (Quran variant) and Scheherazade New. The final choice needs a rendering test on a device with the real text, including the small high marks, and the font's license text goes into the app's licenses screen |
| 7 | Model delivery | **After install** (section 6); the design is not the focus now |
| 8 | History | **Local, on the phone's storage** (section 7) |
| 9 | Strictness setting | **Yes**: three levels; implemented in the core (section 8) |
| 10 | Latency of 1-2 s | **Acceptable for now**, to be improved later |
| 11 | Beta and consent | **Yes**: opt-in sharing of a saved take through the share sheet (section 7) |
| 12 | Disclaimer wording | **Dropped**; no required statement in the app for now |

## 13. Out of scope for the first version

Letter- and vowel-level feedback, tajweed rules, other qira'at, translations or tafsir, listening to reciters
(audio licenses), accounts, sharing and social features beyond the beta sharing above, cloud sync, payments of any
kind, notifications, widgets and wearables, background recitation.

## 14. Milestones (order, no dates)

1. Core additions the app needs: cross-compiled builds for Android (and later iOS) and phone measurements
   (the strictness level API is done).
2. Android prototype (Compose): record, push, highlight the current word, show marks; range picker; follow
   mode; model files placed by hand.
3. Hifz mode, result screen, history, settings, localization (en, ar), licenses screen, first-run flow with the
   model download.
4. iOS app (SwiftUI) with the same behaviour on the same core.
5. Learner testing with consent; tune with what they show.
6. Store submission: policies, descriptions, screenshots, privacy forms.
