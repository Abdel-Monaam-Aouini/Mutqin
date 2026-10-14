/* Mutqin public C API. This is the only public header of MutqinCore.
 * Plain C, opaque handles, prefix mq_. No platform-specific types, no exceptions across the
 * boundary: functions return MQ_OK (0) or a negative MQ_ERR_* code and fill an error buffer. */
#ifndef MUTQIN_H
#define MUTQIN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQ_OK 0
#define MQ_ERR_ARGUMENT (-1) /* null pointer, bad range or index */
#define MQ_ERR_DATA (-2)     /* Quran text missing or failed its checksum */
#define MQ_ERR_INTERNAL (-3)
#define MQ_ERR_NO_MODEL (-4) /* no recognition model is attached to the session */
#define MQ_ERR_LIMIT (-5)    /* range or session too long: memory is bounded, see "Limits" below */

/* Library version as a static, NUL-terminated string, e.g. "0.1.0". */
const char* mq_version(void);

/* Verified Quran text plus the phonemizer. Cheap to share between sessions. */
typedef struct mq_engine mq_engine;

/* One recitation exercise: an ayah range and its expected phonemes. */
typedef struct mq_session mq_session;

/* Reading variants of an expected phoneme (spec section 1). */
enum {
    MQ_VARIANT_BOTH = 0,
    MQ_VARIANT_CONTINUE_ONLY = 1,
    MQ_VARIANT_WAQF_ONLY = 2,
    MQ_VARIANT_START_ONLY = 3,
    MQ_VARIANT_AFTER_CONTINUE = 4
};

/* One expected phoneme. Pointers stay valid until the session is destroyed. */
typedef struct mq_phoneme {
    const char* symbol; /* ASCII symbol from docs/PHONEME_INVENTORY.md */
    int surah;
    int ayah;
    int word;          /* 1-based word index inside the ayah */
    int variant;       /* MQ_VARIANT_* */
    const char* rules; /* comma-separated rule tags, "" if none */
    int dur_min;       /* allowed duration in counts, 0 and 0 if not constrained */
    int dur_max;
} mq_phoneme;

/* Threading contract
 *  - A session is not thread-safe: call all functions of one session from one thread at a time
 *    (any thread, but never concurrently). Different sessions are independent.
 *  - An engine is read-only after creation and may be shared by sessions on different threads.
 *  - mq_session_push_audio / push_posteriors allocate memory and run the aligner: call them from a
 *    worker thread, never from a real-time audio callback. Copy the callback's samples into your own
 *    queue and push from the worker. The core starts no threads of its own.
 *
 * Limits (memory is bounded so a phone cannot run out of it)
 *  - A range may have at most MQ_MAX_EXPECTED_PHONEMES expected phonemes (about 900 words, more than
 *    a mushaf page); mq_session_create returns MQ_ERR_LIMIT for a longer range.
 *  - A session accepts at most 2 x (expected phonemes) + 500 heard phonemes; beyond that
 *    push functions return MQ_ERR_LIMIT and the app should end the session. Memory stays below about
 *    60 MB at the maximum range. */
#define MQ_MAX_EXPECTED_PHONEMES 5000

/* Size of the posterior vocabulary the core expects: token 0 is the CTC blank, tokens 1..31 are
 * the phonemes of docs/PHONEME_INVENTORY.md in contract order. An inference backend maps its own
 * output units to these. */
#define MQ_VOCAB_SIZE 32

/* Kinds of word-level events. */
enum { MQ_EVENT_SKIPPED = 0, MQ_EVENT_WRONG = 1, MQ_EVENT_REPEATED = 2, MQ_EVENT_ADDED = 3 };

/* Status of a word of the range. */
enum { MQ_WORD_NOT_JUDGED = 0, MQ_WORD_OK = 1, MQ_WORD_WRONG = 2, MQ_WORD_SKIPPED = 3 };

/* Ayah 0 of a surah (other than 1 and 9) is its basmala: with a model that supplies expected units,
 * a range that starts at ayah 1 begins with these four optional items (reported as ayah 0).
 *
 * A word-level event. word_index counts the items of the session range from 0; an item is one Tanzil
 * word, or a group of consecutive words that the model's reading joins (word..word_last). For
 * MQ_EVENT_ADDED it is the item the extra speech follows. */
typedef struct mq_event {
    int kind;
    int surah;
    int ayah;
    int word; /* 1-based word index inside the ayah */
    size_t word_index;
    double confidence;
    int word_last; /* last word of the item (== word unless the model's reading joins words) */
} mq_event;

/* Loads and verifies the Quran text in data_dir (data/quran). On failure *out is NULL and
 * err (if not NULL) receives a message of at most err_len bytes. */
int mq_engine_create(const char* data_dir, mq_engine** out, char* err, size_t err_len);
void mq_engine_destroy(mq_engine* engine);

/* Ayahs ayah_first..ayah_last (inclusive) of a surah. */
int mq_session_create(const mq_engine* engine, int surah, int ayah_first, int ayah_last, mq_session** out,
                      char* err, size_t err_len);
void mq_session_destroy(mq_session* session);

size_t mq_session_phoneme_count(const mq_session* session);
int mq_session_phoneme(const mq_session* session, size_t index, mq_phoneme* out);

/* Feeds num_frames rows of MQ_VOCAB_SIZE probabilities (row-major, each row sums to about 1).
 * The core decodes and aligns everything heard so far. Not real-time safe yet. */
int mq_session_push_posteriors(mq_session* session, const float* probs, size_t num_frames);

/* Sample rate of the audio that is pushed (default 16000). Other rates between 8000 and 192000 Hz are
 * resampled to 16 kHz inside; the audio must be mono. Call it before the first push (it restarts the
 * conversion). */
int mq_session_set_input_rate(mq_session* session, int sample_rate_hz);

/* Feeds mono float samples in [-1, 1] at the input rate (any chunk size). Features and the recognition model
 * run inside; returns MQ_ERR_NO_MODEL if the session has no model. Not real-time safe yet. */
int mq_session_push_audio(mq_session* session, const float* samples, size_t num_samples);

/* Signals the end of the audio so the last words are decoded (token recognizers only). */
int mq_session_finish_audio(mq_session* session);

/* Loads a recognition model from a manifest file (key=value lines: model=, tokens=, token_map=,
 * threads=; paths relative to the manifest). Returns MQ_ERR_NO_MODEL with a message if this build
 * has no inference backend (-DMUTQIN_WITH_SHERPA=ON). */
int mq_session_load_model(mq_session* session, const char* manifest_path, char* err, size_t err_len);

/* Words of the range, in recitation order. */
size_t mq_session_word_count(const mq_session* session);
int mq_session_word_status(const mq_session* session, size_t word_index); /* MQ_WORD_*, -1 if bad index */

/* Snapshot of the current events (replaced by every push). While audio is being pushed the last word
 * heard so far is not judged yet (it is only partly in), so an event about a word appears about one
 * word after it was spoken; after mq_session_finish_audio every word is judged. Posteriors pushed with
 * mq_session_push_posteriors are not streaming and are judged at once. */
size_t mq_session_event_count(const mq_session* session);
int mq_session_event(const mq_session* session, size_t index, mq_event* out);

#ifdef __cplusplus
}
#endif

#endif /* MUTQIN_H */
