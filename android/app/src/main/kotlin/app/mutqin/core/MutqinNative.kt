package app.mutqin.core

/** The JNI functions of `jni_bridge.cpp`; use [Engine] and [Session] instead of calling these directly. */
internal object MutqinNative {
    init {
        System.loadLibrary("mutqin_jni")
    }

    external fun engineCreateFromMemory(
        uthmani: ByteArray,
        simple: ByteArray,
        checksums: ByteArray,
    ): Long

    external fun engineDestroy(engine: Long)

    external fun engineAyahCount(
        engine: Long,
        surah: Int,
    ): Int

    external fun sessionCreate(
        engine: Long,
        surah: Int,
        from: Int,
        to: Int,
    ): Long

    external fun sessionDestroy(session: Long)

    external fun sessionSetInputRate(
        session: Long,
        hz: Int,
    )

    external fun sessionSetStrictness(
        session: Long,
        level: Int,
    )

    external fun sessionLoadModel(
        session: Long,
        manifest: String,
    )

    external fun sessionPushAudio(
        session: Long,
        samples: FloatArray,
        count: Int,
    )

    external fun sessionFinishAudio(session: Long)

    external fun sessionReset(session: Long)

    external fun sessionPosition(session: Long): Int

    external fun sessionItemCount(session: Long): Int

    external fun sessionItem(
        session: Long,
        index: Int,
        out: IntArray,
    )

    external fun sessionItemRules(
        session: Long,
        index: Int,
    ): String

    external fun sessionItemText(
        session: Long,
        index: Int,
    ): String

    external fun sessionItemUnits(
        session: Long,
        index: Int,
    ): String

    external fun sessionItemHeard(
        session: Long,
        index: Int,
    ): String

    external fun sessionWordStatus(
        session: Long,
        index: Int,
    ): Int

    external fun sessionEventCount(session: Long): Int

    external fun sessionEvent(
        session: Long,
        index: Int,
        out: IntArray,
    )
}
