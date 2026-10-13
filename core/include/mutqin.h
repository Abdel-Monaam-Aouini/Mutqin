/* Mutqin public C API. This is the only public header of MutqinCore.
 * Plain C, opaque handles, prefix mq_. No platform-specific types. */
#ifndef MUTQIN_H
#define MUTQIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Library version as a static, NUL-terminated string, e.g. "0.1.0". */
const char* mq_version(void);

#ifdef __cplusplus
}
#endif

#endif /* MUTQIN_H */
