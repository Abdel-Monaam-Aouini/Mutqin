// JNI layer: each function forwards to the C API of mutqin.h. Errors become a Kotlin MutqinException.
#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

#include "mutqin.h"

namespace {

jlong ToHandle(const void* p) {
    return static_cast<jlong>(reinterpret_cast<intptr_t>(p));
}
template <typename T>
T* FromHandle(jlong h) {
    return reinterpret_cast<T*>(static_cast<intptr_t>(h));
}

void Throw(JNIEnv* env, int code, const char* message) {
    jclass cls = env->FindClass("app/mutqin/core/MutqinException");
    if (cls == nullptr)
        return;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(ILjava/lang/String;)V");
    jobject ex = env->NewObject(cls, ctor, static_cast<jint>(code), env->NewStringUTF(message));
    env->Throw(static_cast<jthrowable>(ex));
}

std::vector<char> Bytes(JNIEnv* env, jbyteArray array) {
    const jsize n = env->GetArrayLength(array);
    std::vector<char> out(static_cast<size_t>(n));
    env->GetByteArrayRegion(array, 0, n, reinterpret_cast<jbyte*>(out.data()));
    return out;
}

// returns true if rc is OK; otherwise throws
bool Check(JNIEnv* env, int rc, const char* what) {
    if (rc == MQ_OK)
        return true;
    Throw(env, rc, what);
    return false;
}

}  // namespace

#define JNI(ret, name) extern "C" JNIEXPORT ret JNICALL Java_app_mutqin_core_MutqinNative_##name

JNI(jlong, engineCreateFromMemory)(JNIEnv* env, jobject, jbyteArray uthmani, jbyteArray simple,
                                   jbyteArray checksums) {
    const std::vector<char> a = Bytes(env, uthmani), b = Bytes(env, simple), c = Bytes(env, checksums);
    mq_engine* engine = nullptr;
    char err[256] = {0};
    const int rc = mq_engine_create_from_memory(a.data(), a.size(), b.data(), b.size(), c.data(), c.size(),
                                                &engine, err, sizeof err);
    if (rc != MQ_OK) {
        Throw(env, rc, err);
        return 0;
    }
    return ToHandle(engine);
}

JNI(void, engineDestroy)(JNIEnv*, jobject, jlong engine) {
    mq_engine_destroy(FromHandle<mq_engine>(engine));
}

JNI(jint, engineAyahCount)(JNIEnv*, jobject, jlong engine, jint surah) {
    return mq_engine_ayah_count(FromHandle<mq_engine>(engine), surah);
}

JNI(jlong, sessionCreate)(JNIEnv* env, jobject, jlong engine, jint surah, jint from, jint to) {
    mq_session* session = nullptr;
    char err[256] = {0};
    const int rc =
        mq_session_create(FromHandle<mq_engine>(engine), surah, from, to, &session, err, sizeof err);
    if (rc != MQ_OK) {
        Throw(env, rc, err);
        return 0;
    }
    return ToHandle(session);
}

JNI(void, sessionDestroy)(JNIEnv*, jobject, jlong session) {
    mq_session_destroy(FromHandle<mq_session>(session));
}

JNI(void, sessionSetInputRate)(JNIEnv* env, jobject, jlong session, jint hz) {
    Check(env, mq_session_set_input_rate(FromHandle<mq_session>(session), hz), "unsupported sample rate");
}

JNI(void, sessionSetStrictness)(JNIEnv* env, jobject, jlong session, jint level) {
    Check(env, mq_session_set_strictness(FromHandle<mq_session>(session), level), "bad strictness level");
}

JNI(void, sessionLoadModel)(JNIEnv* env, jobject, jlong session, jstring manifest) {
    const char* path = env->GetStringUTFChars(manifest, nullptr);
    char err[256] = {0};
    const int rc = mq_session_load_model(FromHandle<mq_session>(session), path, err, sizeof err);
    env->ReleaseStringUTFChars(manifest, path);
    if (rc != MQ_OK)
        Throw(env, rc, err);
}

JNI(void, sessionPushAudio)(JNIEnv* env, jobject, jlong session, jfloatArray samples, jint count) {
    jfloat* data = env->GetFloatArrayElements(samples, nullptr);
    const int rc = mq_session_push_audio(FromHandle<mq_session>(session), data, static_cast<size_t>(count));
    env->ReleaseFloatArrayElements(samples, data, JNI_ABORT);
    Check(env, rc, "could not process the audio");
}

JNI(void, sessionFinishAudio)(JNIEnv* env, jobject, jlong session) {
    Check(env, mq_session_finish_audio(FromHandle<mq_session>(session)), "could not finish the audio");
}

JNI(void, sessionReset)(JNIEnv* env, jobject, jlong session) {
    Check(env, mq_session_reset(FromHandle<mq_session>(session)), "could not reset");
}

JNI(jint, sessionPosition)(JNIEnv*, jobject, jlong session) {
    return mq_session_position(FromHandle<mq_session>(session));
}

JNI(jint, sessionItemCount)(JNIEnv*, jobject, jlong session) {
    return static_cast<jint>(mq_session_word_count(FromHandle<mq_session>(session)));
}

// out: surah, ayah, word, wordLast
JNI(void, sessionItem)(JNIEnv* env, jobject, jlong session, jint index, jintArray out) {
    mq_item item{};
    if (!Check(env, mq_session_item(FromHandle<mq_session>(session), static_cast<size_t>(index), &item),
               "bad item"))
        return;
    const jint values[4] = {item.surah, item.ayah, item.word, item.word_last};
    env->SetIntArrayRegion(out, 0, 4, values);
}

JNI(jstring, sessionItemRules)(JNIEnv* env, jobject, jlong session, jint index) {
    mq_session* s = FromHandle<mq_session>(session);
    const int need = mq_session_item_rules(s, static_cast<size_t>(index), nullptr, 0);
    if (need < 0) {
        Throw(env, need, "bad item");
        return nullptr;
    }
    std::string rules(static_cast<size_t>(need) + 1, '\0');
    mq_session_item_rules(s, static_cast<size_t>(index), rules.data(), rules.size());
    return env->NewStringUTF(rules.c_str());
}

JNI(jstring, sessionItemText)(JNIEnv* env, jobject, jlong session, jint index) {
    mq_session* s = FromHandle<mq_session>(session);
    const int need = mq_session_item_text(s, static_cast<size_t>(index), nullptr, 0);
    if (need < 0) {
        Throw(env, need, "bad item");
        return nullptr;
    }
    std::string text(static_cast<size_t>(need) + 1, '\0');
    mq_session_item_text(s, static_cast<size_t>(index), text.data(), text.size());
    text.resize(static_cast<size_t>(need));
    return env->NewStringUTF(text.c_str());
}

// The two unit strings of an item (expected and heard) are read the same way as the text.
jstring UnitString(JNIEnv* env, jlong session, jint index, int (*read)(const mq_session*, size_t, char*, size_t)) {
    mq_session* s = FromHandle<mq_session>(session);
    const int need = read(s, static_cast<size_t>(index), nullptr, 0);
    if (need < 0) {
        Throw(env, need, "bad item");
        return nullptr;
    }
    std::string text(static_cast<size_t>(need) + 1, '\0');
    read(s, static_cast<size_t>(index), text.data(), text.size());
    text.resize(static_cast<size_t>(need));
    return env->NewStringUTF(text.c_str());
}

JNI(jstring, sessionItemUnits)(JNIEnv* env, jobject, jlong session, jint index) {
    return UnitString(env, session, index, mq_session_item_units);
}

JNI(jstring, sessionItemHeard)(JNIEnv* env, jobject, jlong session, jint index) {
    return UnitString(env, session, index, mq_session_item_heard);
}

JNI(jint, sessionWordStatus)(JNIEnv*, jobject, jlong session, jint index) {
    return mq_session_word_status(FromHandle<mq_session>(session), static_cast<size_t>(index));
}

JNI(jint, sessionEventCount)(JNIEnv*, jobject, jlong session) {
    return static_cast<jint>(mq_session_event_count(FromHandle<mq_session>(session)));
}

// out: kind, surah, ayah, word, wordIndex, wordLast
JNI(void, sessionEvent)(JNIEnv* env, jobject, jlong session, jint index, jintArray out) {
    mq_event e{};
    if (!Check(env, mq_session_event(FromHandle<mq_session>(session), static_cast<size_t>(index), &e),
               "bad event"))
        return;
    const jint values[6] = {e.kind, e.surah, e.ayah, e.word, static_cast<jint>(e.word_index), e.word_last};
    env->SetIntArrayRegion(out, 0, 6, values);
}
