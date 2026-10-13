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

#ifdef __cplusplus
}
#endif

#endif /* MUTQIN_H */
