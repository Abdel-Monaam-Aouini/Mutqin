package app.mutqin.core

/** An error returned by the core (the codes MQ_ERR_* of mutqin.h). */
class MutqinException(
    val code: Int,
    message: String,
) : RuntimeException(message) {
    val isLimit: Boolean get() = code == ERR_LIMIT

    companion object {
        const val ERR_LIMIT = -5
    }
}

/** How many differences are reported (MQ_STRICTNESS_* of mutqin.h). */
object Strictness {
    const val RELAXED = 0
    const val STANDARD = 1
    const val STRICT = 2
}

/** The state of an item (a word, or a group of words that the model's reading joins). */
enum class WordStatus { NotJudged, Ok, Wrong, Skipped }

enum class EventKind { Skipped, Wrong, Repeated, Added }

/** Which Tanzil words an item covers; ayah 0 is the basmala preface. */
data class Item(
    val surah: Int,
    val ayah: Int,
    val word: Int,
    val wordLast: Int,
    val text: String,
)

data class Event(
    val kind: EventKind,
    val itemIndex: Int,
)

/** The verified Quran text and the rule data, loaded once. Thread-safe to share between sessions. */
class Engine private constructor(
    private var handle: Long,
) : AutoCloseable {
    fun ayahCount(surah: Int): Int = MutqinNative.engineAyahCount(handle, surah)

    fun createSession(
        surah: Int,
        fromAyah: Int,
        toAyah: Int,
    ): Session {
        val engineHandle = handle
        return Session(MutqinNative.sessionCreate(engineHandle, surah, fromAyah, toAyah))
    }

    override fun close() {
        if (handle != 0L) MutqinNative.engineDestroy(handle)
        handle = 0
    }

    companion object {
        /** From the contents of quran-uthmani.txt, quran-simple-clean.txt and checksums.txt (verified). */
        fun fromMemory(
            uthmani: ByteArray,
            simple: ByteArray,
            checksums: ByteArray,
        ): Engine = Engine(MutqinNative.engineCreateFromMemory(uthmani, simple, checksums))
    }
}

/** One recitation attempt on an ayah range. Calls may come from different threads (the core serialises them). */
class Session internal constructor(
    private var handle: Long,
) : AutoCloseable {
    private val eventBuffer = IntArray(6)
    private val itemBuffer = IntArray(4)

    fun setInputRate(hz: Int) = MutqinNative.sessionSetInputRate(handle, hz)

    fun setStrictness(level: Int) = MutqinNative.sessionSetStrictness(handle, level)

    fun loadModel(manifestPath: String) = MutqinNative.sessionLoadModel(handle, manifestPath)

    /** Mono audio in [-1, 1] at the input rate; call from a worker thread, not from the audio callback. */
    fun pushAudio(
        samples: FloatArray,
        count: Int,
    ) = MutqinNative.sessionPushAudio(handle, samples, count)

    fun finish() = MutqinNative.sessionFinishAudio(handle)

    fun reset() = MutqinNative.sessionReset(handle)

    /** Index of the item being recited now, or -1 before anything was recognised. */
    fun position(): Int = MutqinNative.sessionPosition(handle)

    fun itemCount(): Int = MutqinNative.sessionItemCount(handle)

    fun item(index: Int): Item {
        MutqinNative.sessionItem(handle, index, itemBuffer)
        return Item(itemBuffer[0], itemBuffer[1], itemBuffer[2], itemBuffer[3], MutqinNative.sessionItemText(handle, index))
    }

    fun status(index: Int): WordStatus =
        when (MutqinNative.sessionWordStatus(handle, index)) {
            1 -> WordStatus.Ok
            2 -> WordStatus.Wrong
            3 -> WordStatus.Skipped
            else -> WordStatus.NotJudged
        }

    fun events(): List<Event> {
        val count = MutqinNative.sessionEventCount(handle)
        val out = ArrayList<Event>(count)
        for (i in 0 until count) {
            MutqinNative.sessionEvent(handle, i, eventBuffer)
            val kind = EventKind.entries.getOrNull(eventBuffer[0]) ?: continue
            out.add(Event(kind, eventBuffer[4]))
        }
        return out
    }

    override fun close() {
        if (handle != 0L) MutqinNative.sessionDestroy(handle)
        handle = 0
    }
}
