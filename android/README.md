# Mutqin for Android (prototype)

Kotlin and Jetpack Compose over the shared C++ core (`core/`), through a thin JNI layer
(`app/src/main/cpp/jni_bridge.cpp`) that forwards to the C API of `core/include/mutqin.h`. Behaviour and decisions:
[`docs/APP_SPEC.md`](../docs/APP_SPEC.md).

## What works

- Setup: choose a surah and an ayah range (a short surah defaults to the whole surah, a long one to its first ten
  ayahs) and how many differences are reported (fewer, standard, more: the core's strictness levels).
- Recitation: the Uthmani text from the verified Tanzil files, the current word highlighted, marks under the words
  (solid line wrong, dotted skipped, double repeated, a caret for extra speech), a microphone level bar with a hint
  when the phone cannot hear you, a timer, and a summary of what was found when you stop.
- Errors are shown exactly (for example a Quran text that fails its checksum), selectable and with a copy button.
- English and Arabic strings (`res/values`, `res/values-ar`); dark and light themes.
- minSdk 21 (Android 5.0), arm64-v8a and armeabi-v7a. The release build uses R8 (code and resource shrinking).

Not yet: hifz (hidden text) mode, history, a settings screen, the first-run model download, a bundled Quran font,
automated UI tests.

## Build and run

Needs: JDK 21, the Android SDK with API 37 and NDK 27.1 (CMake 3.22), the Quran text in `data/quran/`
(`make data`), and the gated model in `models/zipformer_p-quran/` (see the main README). From the repository root:

```
make android-app      # downloads the sherpa-onnx Android libraries (checksummed) on first use, builds the debug APK
make android-run      # installs it on the connected phone and starts it
make android-model    # copies the model files into the app's folder on the phone (start the app once first)
make android-log      # the app's log lines
```

`android/local.properties` (not committed) points to your SDK; see `local.properties.example`. The Quran text is
copied into the app's assets at build time and the model is not part of the app (it is delivered separately, see
the spec), so neither is in the repository.

Debug builds also accept, for automated checks:

```
adb shell am start -n app.mutqin/.MainActivity --es wav take.wav --ei surah 5 --ei from 1 --ei to 8 --ei level 1   # replay a WAV from the app's folder (make android-replay)
adb shell am start -n app.mutqin/.MainActivity --ez corrupt_quran true                                            # shows the real checksum error screen
```

## Versions

AGP 9.4.0, Kotlin 2.4.20, Gradle 9.8.0. **Compose BOM 2025.07.00** (Compose 1.8): the newest Compose release that
still supports Android 5.0; from Compose 1.9 on the minimum is Android 6.0 (API 23). Activity Compose 1.10.1,
lifecycle 2.9.2 and core-ktx 1.16.0 are pinned for the same reason. All are Apache-2.0.

## Size

| Build | APK |
|---|---|
| debug (both CPU types) | 33.0 MB |
| release (R8, shrunk resources, both CPU types) | 21.2 MB |

Most of it is ONNX Runtime and the sherpa-onnx library; the core is under 1 MB. A phone downloads only its own
CPU type from the store, so the install is smaller than the universal APK. The release build is signed with the
debug key for trying it on a phone; a store release needs the project's own key.
