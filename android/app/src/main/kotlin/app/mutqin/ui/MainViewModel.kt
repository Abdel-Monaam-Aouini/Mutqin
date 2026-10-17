package app.mutqin.ui

import android.app.Application
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import app.mutqin.AppPreferences
import app.mutqin.BuildConfig
import app.mutqin.HistoryEntry
import app.mutqin.HistoryStore
import app.mutqin.core.Engine
import app.mutqin.core.EventKind
import app.mutqin.core.Item
import app.mutqin.core.MutqinException
import app.mutqin.core.Session
import app.mutqin.core.Strictness
import app.mutqin.core.TajweedRule
import app.mutqin.core.WordStatus
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.File

enum class Phase { Loading, NeedModel, Setup, Starting, Reciting, Result, Error }

enum class ItemState { Upcoming, Current, Ok, Wrong, Skipped }

/** One item of the text as the screen needs it. */
data class UiItem(
    val index: Int,
    val ayah: Int,
    val word: Int,
    val text: String,
    val state: ItemState,
    val repeated: Boolean,
    val addedAfter: Boolean,
    val tajweedRules: List<TajweedRule>,
)

/** What the core can say about one flagged word: the expected units and, for a wrong word, the units heard. */
data class WordDetail(
    val index: Int,
    val ayah: Int,
    val word: Int,
    val text: String,
    val state: ItemState,
    val expected: List<String>,
    val heard: List<String>,
)

data class Finding(
    val kind: EventKind,
    val ayah: Int,
    val word: Int,
)

data class UiState(
    val phase: Phase = Phase.Loading,
    val message: String? = null,
    val surah: Int = 1,
    val fromAyah: Int = 1,
    val toAyah: Int = 7,
    val ayahCount: Int = 7,
    val strictness: Int = Strictness.STANDARD,
    val items: List<UiItem> = emptyList(),
    val findings: List<Finding> = emptyList(),
    val modelFolder: String = "",
    /** Microphone level, 0 (silent) to 1 (loud), for the level bar. */
    val level: Float = 0f,
    /** The exact error text when the Quran text could not be read ([Phase.Error]). */
    val error: String? = null,
    /** When listening started (uptime milliseconds), for the timer. */
    val startedAt: Long = 0L,
    /** When listening was paused (uptime milliseconds), or 0 while it runs; the timer stands still meanwhile. */
    val pausedAt: Long = 0L,
    /** The word whose detail is open (tapped on the recitation screen), if any. */
    val detail: WordDetail? = null,
    /** Memorization mode: the text stays hidden until it has been recited. */
    val memorization: Boolean = false,
    /** This attempt finds the surah and ayah by itself; the items appear once it has found them. */
    val auto: Boolean = false,
    val history: List<HistoryEntry> = emptyList(),
) {
    val paused: Boolean get() = pausedAt != 0L
}

private const val ITEMS_BEHIND = 100 // above the 40 words the core keeps in its window behind the reciter
private const val ITEMS_AHEAD = 3

class MainViewModel(
    app: Application,
) : AndroidViewModel(app) {
    private val _state = MutableStateFlow(UiState())
    val state: StateFlow<UiState> = _state.asStateFlow()

    private var engine: Engine? = null
    private var shown: List<UiItem> = emptyList() // the last list, whose unchanged items are reused
    private var shownPosition = -1
    private var session: Session? = null
    private var recorder: MicRecorder? = null
    private var meta: List<Item> = emptyList()
    private var pollJob: Job? = null

    @Volatile
    private var rawLevel = 0f

    /** Debug builds only: flip one bit of the Quran text before loading it, to see the real checksum error on screen. */
    var debugCorruptQuran = false

    private val modelDir = File(app.getExternalFilesDir(null), "model")
    private val modelFiles = listOf("quran_phoneme_zipformer.int8.onnx", "tokens.txt", "expected_units.tsv")

    init {
        _state.update {
            it.copy(modelFolder = modelDir.absolutePath, strictness = AppPreferences.strictness(app), memorization = AppPreferences.memorization(app))
        }
        prepare()
    }

    /** Loads the Quran text from the assets (once) and checks that the model files are in place. */
    fun prepare() {
        viewModelScope.launch(Dispatchers.IO) {
            _state.update { it.copy(phase = Phase.Loading, message = null, error = null) }
            var stage = "reading the Quran text from the app"
            try {
                if (BuildConfig.DEBUG && debugCorruptQuran) {
                    engine?.close()
                    engine = null
                }
                if (engine == null) {
                    val assets = getApplication<Application>().assets

                    fun asset(name: String): ByteArray {
                        stage = "reading the Quran text from the app (assets/quran/$name)"
                        return assets.open("quran/$name").use { it.readBytes() }
                    }
                    val uthmani = asset("quran-uthmani.txt")
                    if (BuildConfig.DEBUG && debugCorruptQuran) uthmani[uthmani.size / 2] = (uthmani[uthmani.size / 2].toInt() xor 1).toByte()
                    val simple = asset("quran-simple-clean.txt")
                    val checksums = asset("checksums.txt")
                    stage = "verifying the Quran text (checksums and structure)"
                    engine = Engine.fromMemory(uthmani, simple, checksums)
                }
                stage = "checking the model files"
                installBundledModelIfPresent()
                if (modelFiles.any { !File(modelDir, it).isFile }) {
                    modelDir.mkdirs()
                    _state.update { it.copy(phase = Phase.NeedModel) }
                } else {
                    val count = engine!!.ayahCount(_state.value.surah)
                    _state.update { it.copy(phase = Phase.Setup, ayahCount = count, toAyah = minOf(it.toAyah, count)) }
                }
            } catch (e: Throwable) {
                Log.e(TAG, "prepare failed while $stage", e)
                _state.update { it.copy(phase = Phase.Error, error = exactError(stage, e)) }
            }
        }
    }

    /**
     * A shareable APK may carry the gated model in assets/model. Extract it to the same directory
     * used by the development adb workflow. The temporary file prevents a killed first launch from
     * leaving a partial ONNX file that looks present on the next launch.
     */
    private fun installBundledModelIfPresent() {
        val assets = getApplication<Application>().assets
        val bundled = modelFiles + "LICENSE"
        val names = assets.list("model")?.toSet().orEmpty()
        if (!bundled.all { it in names }) return
        modelDir.mkdirs()
        for (name in bundled) {
            val destination = File(modelDir, name)
            val temporary = File(modelDir, "$name.part")
            if (destination.isFile && destination.length() == assets.open("model/$name").use { it.available().toLong() }) continue
            assets.open("model/$name").use { input ->
                BufferedInputStream(input).use { source ->
                    BufferedOutputStream(temporary.outputStream()).use { output -> source.copyTo(output) }
                }
            }
            if (destination.exists()) check(destination.delete()) { "could not replace bundled model file $name" }
            check(temporary.renameTo(destination)) { "could not install bundled model file $name" }
        }
    }

    /** The error as it is, with where it happened: for the error screen, and to copy into a bug report. */
    private fun exactError(
        stage: String,
        e: Throwable,
    ): String =
        buildString {
            append("While ").append(stage).append(":\n")
            append(e.javaClass.name)
            e.message?.let { append(": ").append(it) }
            if (e is MutqinException) append("\n(core error code ").append(e.code).append(")")
            var cause = e.cause
            while (cause != null) {
                append("\ncaused by ").append(cause.javaClass.name)
                cause.message?.let { append(": ").append(it) }
                cause = cause.cause
            }
        }

    fun setSurah(surah: Int) {
        val e = engine ?: return
        val s = surah.coerceIn(1, 114)
        val count = e.ayahCount(s)
        // a short surah is taken whole; a long one starts with its first ten ayahs
        val to = if (count <= 30) count else 10
        _state.update { it.copy(surah = s, ayahCount = count, fromAyah = 1, toAyah = to, message = null) }
    }

    fun setFrom(ayah: Int) =
        _state.update {
            val from = ayah.coerceIn(1, it.ayahCount)
            it.copy(fromAyah = from, toAyah = maxOf(it.toAyah, from), message = null)
        }

    fun setTo(ayah: Int) =
        _state.update {
            val to = ayah.coerceIn(1, it.ayahCount)
            it.copy(toAyah = to, fromAyah = minOf(it.fromAyah, to), message = null)
        }

    fun setStrictness(level: Int) {
        AppPreferences.saveStrictness(getApplication(), level)
        _state.update { it.copy(strictness = level) }
    }

    fun setMemorization(on: Boolean) {
        AppPreferences.saveMemorization(getApplication(), on)
        _state.update { it.copy(memorization = on) }
    }

    /** Reads the history from the phone (for the history screen). */
    fun loadHistory() {
        viewModelScope.launch(Dispatchers.IO) {
            val all = HistoryStore.load(getApplication()).reversed()
            _state.update { it.copy(history = all) }
        }
    }

    fun clearHistory() {
        viewModelScope.launch(Dispatchers.IO) {
            HistoryStore.clear(getApplication())
            _state.update { it.copy(history = emptyList()) }
        }
    }

    /** Keeps the attempt in the history (not an attempt where nothing was recited). */
    private fun remember(
        st: UiState,
        durationSeconds: Int,
    ) {
        if (durationSeconds < 3 || st.items.none { it.state != ItemState.Upcoming }) return
        val kinds = st.findings.groupingBy { it.kind }.eachCount()
        HistoryStore.append(
            getApplication(),
            HistoryEntry(
                System.currentTimeMillis(),
                st.surah,
                st.fromAyah,
                st.toAyah,
                durationSeconds,
                kinds[EventKind.Wrong] ?: 0,
                kinds[EventKind.Skipped] ?: 0,
                kinds[EventKind.Repeated] ?: 0,
                kinds[EventKind.Added] ?: 0,
                st.memorization,
            ),
        )
    }

    /** The model manifest with absolute paths (the core resolves relative paths against the manifest's folder). */
    private fun writeManifest(): String {
        val file = File(getApplication<Application>().filesDir, "mutqin_model.txt")
        file.writeText(
            "model=${File(modelDir, modelFiles[0]).absolutePath}\n" +
                "tokens=${File(modelDir, modelFiles[1]).absolutePath}\n" +
                "units=${File(modelDir, modelFiles[2]).absolutePath}\n" +
                "collapse_runs=2\nthreads=2\n",
        )
        return file.absolutePath
    }

    /** Opens a session for the chosen range and loads the model; the caller starts the audio. */
    private fun openSession(): Session {
        val st = _state.value
        val s = if (st.auto) engine!!.createAutoSession() else engine!!.createSession(st.surah, st.fromAyah, st.toAyah)
        try {
            s.setStrictness(st.strictness)
            s.loadModel(writeManifest())
        } catch (e: Exception) {
            s.close()
            throw e
        }
        session = s
        meta = (0 until s.itemCount()).map { s.item(it) }
        shown = emptyList()
        shownPosition = -1
        return s
    }

    /** Starts listening to the microphone (the permission must be granted). */
    fun start(auto: Boolean = false) {
        if (_state.value.phase != Phase.Setup) return
        _state.update { it.copy(phase = Phase.Starting, message = null, items = emptyList(), findings = emptyList(), auto = auto) }
        viewModelScope.launch(Dispatchers.IO) {
            try {
                val s = openSession()
                val mic = newMic(s)
                if (!mic.open()) error("cannot open the microphone")
                s.setInputRate(mic.sampleRate)
                recorder = mic
                refresh(s, finished = false)
                _state.update { it.copy(phase = Phase.Reciting, startedAt = android.os.SystemClock.elapsedRealtime(), pausedAt = 0L) }
                mic.start()
                startPolling(s)
            } catch (e: Exception) {
                closeSession()
                _state.update { it.copy(phase = Phase.Setup, message = describe(e)) }
            }
        }
    }

    private fun newMic(s: Session) =
        MicRecorder { samples, n ->
            var sum = 0.0
            for (i in 0 until n) sum += samples[i] * samples[i]
            val db = 20.0 * kotlin.math.log10(kotlin.math.sqrt(sum / n) + 1e-9)
            rawLevel = ((db + 60.0) / 50.0).coerceIn(0.0, 1.0).toFloat() // -60 dBFS is silent, -10 dBFS loud
            try {
                s.pushAudio(samples, n)
            } catch (e: MutqinException) {
                Log.w(TAG, "push failed: ${e.message}")
            }
        }

    /** Opens the detail of a flagged word. */
    fun showDetail(item: UiItem) {
        val s = session ?: return
        viewModelScope.launch(Dispatchers.IO) {
            val detail =
                try {
                    WordDetail(item.index, item.ayah, item.word, item.text, item.state, s.expectedUnits(item.index), s.heardUnits(item.index))
                } catch (e: MutqinException) {
                    Log.w(TAG, "detail failed: ${e.message}")
                    null
                }
            _state.update { it.copy(detail = detail) }
        }
    }

    fun closeDetail() = _state.update { it.copy(detail = null) }

    /** Pauses listening: the microphone is released (the phone shows it is no longer in use); the session keeps its place. */
    fun pause() {
        if (_state.value.phase != Phase.Reciting || _state.value.paused) return
        _state.update { it.copy(pausedAt = android.os.SystemClock.elapsedRealtime()) } // at once, so the screen answers the tap
        viewModelScope.launch(Dispatchers.IO) {
            recorder?.stop()
            recorder = null
            rawLevel = 0f
        }
    }

    /** Continues after [pause]: the microphone is opened again and the new audio follows the old. */
    fun resume() {
        val s = session ?: return
        val st = _state.value
        if (st.phase != Phase.Reciting || !st.paused) return
        viewModelScope.launch(Dispatchers.IO) {
            try {
                val mic = newMic(s)
                if (!mic.open()) error("cannot open the microphone")
                recorder = mic
                val now = android.os.SystemClock.elapsedRealtime()
                _state.update { it.copy(startedAt = it.startedAt + (now - it.pausedAt), pausedAt = 0L) }
                mic.start()
            } catch (e: Exception) {
                Log.w(TAG, "resume failed: ${e.message}")
                _state.update { it.copy(message = describe(e)) }
            }
        }
    }

    /** Stops listening, judges the last word and shows the result. */
    fun stop() {
        if (_state.value.phase != Phase.Reciting) return
        viewModelScope.launch(Dispatchers.IO) { finishSession() }
    }

    private suspend fun finishSession() {
        pollJob?.cancel()
        recorder?.stop()
        recorder = null
        val s = session ?: return
        try {
            val before = _state.value
            s.finish()
            refresh(s, finished = true)
            if (before.auto && meta.isEmpty()) { // it never found out what was recited
                closeSession()
                _state.update { it.copy(phase = Phase.Setup, message = "not_found", items = emptyList(), findings = emptyList(), pausedAt = 0L) }
                return
            }
            _state.update { it.copy(phase = Phase.Result, pausedAt = 0L) }
            val end = if (before.paused) before.pausedAt else android.os.SystemClock.elapsedRealtime()
            remember(_state.value, ((end - before.startedAt) / 1000).toInt())
        } catch (e: Exception) {
            _state.update { it.copy(phase = Phase.Setup, message = describe(e)) }
        }
    }

    /** Another attempt at the same range. */
    fun retry() {
        val auto = _state.value.auto
        back()
        start(auto)
    }

    /** Back to the range chooser. */
    fun back() {
        pollJob?.cancel()
        recorder?.stop()
        recorder = null
        closeSession()
        _state.update { it.copy(phase = Phase.Setup, items = emptyList(), findings = emptyList(), level = 0f, pausedAt = 0L, detail = null) }
    }

    private fun closeSession() {
        session?.close()
        session = null
    }

    private fun startPolling(s: Session) {
        pollJob?.cancel()
        pollJob =
            viewModelScope.launch(Dispatchers.IO) {
                while (true) {
                    refresh(s, finished = false)
                    _state.update { it.copy(level = rawLevel) }
                    delay(150)
                }
            }
    }

    /**
     * Builds the screen's items from what the core says now (marks can be taken back, so always from the state).
     * A long range has thousands of items, so while listening only the items around the reciter are asked
     * again: what lies far behind has been judged for good and what lies ahead is not judged yet.
     */
    private fun refresh(
        s: Session,
        finished: Boolean,
    ) {
        if (_state.value.auto && meta.isEmpty()) {
            // an automatic session builds its items once it has found the place
            val place = s.located() ?: return
            meta = (0 until s.itemCount()).map { s.item(it) }
            shown = emptyList()
            shownPosition = -1
            val count = engine?.ayahCount(place.first) ?: meta.last().ayah
            _state.update { it.copy(surah = place.first, fromAyah = place.second, toAyah = count, ayahCount = count) }
        }
        val position = s.position()
        val events = s.events()
        val repeated = events.filter { it.kind == EventKind.Repeated }.map { it.itemIndex }.toSet()
        val added = events.filter { it.kind == EventKind.Added }.map { it.itemIndex }.toSet()
        val full = finished || shown.size != meta.size
        val low = if (full) 0 else maxOf(0, minOf(position, shownPosition) - ITEMS_BEHIND)
        val high = if (full) meta.size else minOf(meta.size, maxOf(position, shownPosition) + ITEMS_AHEAD)
        val items = if (full) ArrayList<UiItem?>(meta.size) else ArrayList<UiItem?>(shown)
        if (full) repeat(meta.size) { items.add(null) }
        for (i in low until high) {
            val item = meta[i]
            val state =
                when (s.status(i)) {
                    WordStatus.Wrong -> ItemState.Wrong
                    WordStatus.Skipped -> ItemState.Skipped
                    WordStatus.Ok -> ItemState.Ok
                    WordStatus.NotJudged -> if (!finished && i == position) ItemState.Current else ItemState.Upcoming
                }
            val old = items[i]
            if (old == null || old.state != state || old.repeated != (i in repeated) || old.addedAfter != (i in added)) {
                items[i] = UiItem(i, item.ayah, item.word, item.text, state, i in repeated, i in added, item.tajweedRules)
            }
        }
        val list = items.map { it ?: error("an item was never built") }
        shown = list
        shownPosition = position
        val findings =
            events
                .mapNotNull { e ->
                    meta.getOrNull(e.itemIndex)?.let { Finding(e.kind, it.ayah, it.word) }
                }.sortedWith(compareBy({ it.ayah }, { it.word }))
        _state.update { if (it.items == list && it.findings == findings) it else it.copy(items = list, findings = findings) }
    }

    /**
     * Debug aid for automated checks: pushes a WAV file from the app's external files folder (as fast as possible)
     * instead of the microphone and writes the verdict to logcat (tag Mutqin).
     */
    fun debugReplay(
        wavName: String,
        surah: Int,
        from: Int,
        to: Int,
        level: Int,
    ) {
        viewModelScope.launch(Dispatchers.IO) {
            while (engine == null || _state.value.phase == Phase.Loading) delay(100)
            if (_state.value.phase != Phase.Setup) {
                Log.e(TAG, "REPLAY not ready: phase ${_state.value.phase}")
                return@launch
            }
            setSurah(surah)
            setFrom(from)
            setTo(to)
            _state.update { it.copy(strictness = level) } // a debug replay does not change the saved setting
            try {
                val t0 = System.nanoTime()
                val s = openSession()
                val wav = WavReader.read(File(getApplication<Application>().getExternalFilesDir(null), wavName))
                s.setInputRate(wav.sampleRate)
                val loaded = System.nanoTime()
                _state.update { it.copy(phase = Phase.Reciting) }
                val chunk = wav.sampleRate / 10
                val buffer = FloatArray(chunk)
                var at = 0
                while (at < wav.samples.size) {
                    val n = minOf(chunk, wav.samples.size - at)
                    System.arraycopy(wav.samples, at, buffer, 0, n)
                    s.pushAudio(buffer, n)
                    at += n
                    if ((at / chunk) % 10 == 0) refresh(s, finished = false)
                }
                s.finish()
                refresh(s, finished = true)
                val done = System.nanoTime()
                _state.update { it.copy(phase = Phase.Result) }
                val st = _state.value
                Log.i(
                    TAG,
                    "REPLAY ${wav.samples.size / wav.sampleRate.toDouble()} s of audio at ${wav.sampleRate} Hz; " +
                        "load ${(loaded - t0) / 1_000_000} ms, process ${(done - loaded) / 1_000_000} ms",
                )
                if (st.findings.isEmpty()) Log.i(TAG, "REPLAY no mistakes found")
                for (f in st.findings) Log.i(TAG, "REPLAY ${f.kind.name.lowercase()} ${st.surah}:${f.ayah}:${f.word}")
                Log.i(TAG, "REPLAY done")
            } catch (ex: Exception) {
                Log.e(TAG, "REPLAY failed: ${ex.message}")
                _state.update { it.copy(phase = Phase.Setup, message = describe(ex)) }
            }
        }
    }

    private fun describe(e: Exception): String =
        if (e is MutqinException && e.isLimit) {
            "range"
        } else {
            exactError("starting the recitation", e)
        }

    override fun onCleared() {
        pollJob?.cancel()
        recorder?.stop()
        closeSession()
        engine?.close()
    }

    private companion object {
        const val TAG = "Mutqin"
    }
}
