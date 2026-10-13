# Architecture

Monorepo: core, CLI, tools, and (later) the mobile apps all live in one repository.

## Pipeline

```
Audio source (16 kHz mono: WAV file or microphone, supplied by the CLI/app)
  → Ring buffer
  → VAD (speech / silence, pause detection for waqf)
  → Feature extraction (80-dim log-mel)
  → Acoustic model (streaming CTC, ONNX int8)  → per-frame phoneme/char posteriors
  → Aligner (against expected sequence from the rule engine)
  → Error detector (word / harakat / tajweed, confidence thresholds)
  → Event queue → C API → CLI (later: mobile UI)
```

The **rule engine** runs ahead of time per ayah range: Quran text → expected annotated
phoneme sequence (with waqf / continue variants).

## Folder layout

```
mutqin/
├── AGENTS.md, CLAUDE.md, README.md, START_PROMPT.md
├── .claude/               # rules, skills, settings for Claude Code
├── docs/
├── data/                  # gitignored except small manifests + checksums
│   └── quran/             # Tanzil text files (read-only) + checksums.txt
├── models/                # exported ONNX models (git LFS or download script)
├── core/                  # C++17 shared core, library MutqinCore, namespace mutqin
│   ├── include/mutqin.h   # public C API, prefix mq_ (only public header)
│   ├── src/
│   │   ├── text/          # Quran loader, data model
│   │   ├── tajweed/       # phonemizer + rule engine (port of Python reference)
│   │   ├── audio/         # ring buffer, resampler
│   │   ├── vad/
│   │   ├── features/      # log-mel
│   │   ├── inference/     # sherpa-onnx / ONNX Runtime wrapper
│   │   ├── align/         # streaming CTC alignment, GOP scoring
│   │   ├── detect/        # error detectors
│   │   └── api/           # C API implementation
│   ├── tests/
│   └── CMakeLists.txt
├── cli/                   # mutqin-cli: WAV / microphone input, prints events (uses the C API)
├── tools/                 # Python
│   ├── quran_text/        # download + verify text
│   ├── phonemizer/        # REFERENCE rule engine (source of truth)
│   ├── data/              # dataset prep, error injection
│   ├── train/             # model fine-tuning + ONNX export + quantization
│   ├── eval/              # metrics and evaluation sets
│   └── prototype/         # Phase 2 desktop prototype
├── android/               # DEFERRED: placeholder; later Kotlin + JNI + Oboe app
└── ios/                   # DEFERRED: placeholder; later Swift + AVAudioEngine app
```

Rules: `core/` contains no platform-specific code and builds on Linux, macOS, Windows.
Microphone capture lives in `cli/` (and later in the apps), never in `core/`; the core only
receives samples through `mq_push_audio`.

## C API (sketch — `core/include/mutqin.h`)

```c
typedef struct mq_session mq_session;

typedef struct {
    const char* model_dir;      /* ONNX models + tokens */
    const char* quran_dir;      /* verified text files */
    int   error_levels;         /* bitmask: MQ_WORD | MQ_HARAKAH | MQ_TAJWEED */
    float min_confidence;       /* report errors only above this, e.g. 0.85 */
    int   madd_munfasil_counts; /* 4 or 5, user preference; 0 = accept both */
} mq_config;

typedef enum {
    MQ_EV_POSITION,       /* current word reached */
    MQ_EV_WORD_OK,
    MQ_EV_WORD_SKIPPED,
    MQ_EV_WORD_WRONG,
    MQ_EV_WORD_REPEATED,
    MQ_EV_HARAKAH_ERROR,
    MQ_EV_TAJWEED_ERROR,
    MQ_EV_AYAH_DONE
} mq_event_type;

typedef struct {
    mq_event_type type;
    int surah, ayah, word;      /* location */
    int char_start, char_end;   /* span inside the word, for highlighting */
    const char* rule;           /* e.g. "ikhfa", "madd_munfasil" (static string) */
    float confidence;
    long long t_start_ms, t_end_ms;
} mq_event;

mq_session* mq_create(const mq_config* cfg);
int  mq_set_range(mq_session*, int surah, int ayah_from, int ayah_to);
int  mq_push_audio(mq_session*, const float* samples, int n); /* 16 kHz mono */
int  mq_poll_events(mq_session*, mq_event* out, int max);     /* non-blocking */
void mq_reset(mq_session*);
void mq_destroy(mq_session*);
const char* mq_version(void);
```

## Threading

- Audio callback (CLI/app): only copies samples into a lock-free ring buffer.
- Worker thread: VAD → features → inference → alignment → events.
- UI / CLI main thread: polls events (e.g. every 50 ms) through the C API.

## Alignment strategy

- Keep a window of expected tokens around the current position (e.g. current ayah ± 1).
- Streaming CTC alignment; when the user jumps (repeats or skips), re-anchor using a
  local search over the window.
- Pauses from VAD decide whether the waqf or continue variant of the expected sequence
  applies at word boundaries.
