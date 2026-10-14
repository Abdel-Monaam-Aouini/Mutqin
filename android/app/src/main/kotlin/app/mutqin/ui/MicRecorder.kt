package app.mutqin.ui

import android.annotation.SuppressLint
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import kotlin.math.max

/**
 * Reads the microphone on its own thread in chunks of about 100 ms and hands them to [onAudio] as floats in [-1, 1].
 * The device's own rate is used (16 kHz if it offers it); the core resamples. [onAudio] runs on the worker thread:
 * it is where the audio is pushed to the core, never the platform's real-time callback.
 */
class MicRecorder(private val onAudio: (FloatArray, Int) -> Unit) {
    private var record: AudioRecord? = null
    private var thread: Thread? = null

    @Volatile
    private var running = false

    /** The sample rate of the opened device, valid after a successful [open]. */
    var sampleRate: Int = 16000
        private set

    /** Opens the microphone; returns false if no rate worked (or the permission is missing). */
    @SuppressLint("MissingPermission")
    fun open(): Boolean {
        for (rate in intArrayOf(16000, 44100, 48000)) {
            val minBytes = AudioRecord.getMinBufferSize(rate, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
            if (minBytes <= 0) continue
            val rec = try {
                AudioRecord(
                    MediaRecorder.AudioSource.VOICE_RECOGNITION,
                    rate,
                    AudioFormat.CHANNEL_IN_MONO,
                    AudioFormat.ENCODING_PCM_16BIT,
                    max(minBytes, rate / 10 * 2) * 4,
                )
            } catch (e: Exception) {
                continue
            }
            if (rec.state == AudioRecord.STATE_INITIALIZED) {
                record = rec
                sampleRate = rate
                return true
            }
            rec.release()
        }
        return false
    }

    fun start() {
        val rec = record ?: return
        running = true
        rec.startRecording()
        thread = Thread({
            val chunk = sampleRate / 10
            val shorts = ShortArray(chunk)
            val floats = FloatArray(chunk)
            while (running) {
                val n = rec.read(shorts, 0, chunk)
                if (n <= 0) continue
                for (i in 0 until n) floats[i] = shorts[i] / 32768f
                onAudio(floats, n)
            }
        }, "mutqin-mic").also { it.start() }
    }

    /** Stops reading and waits for the worker thread, so no audio is pushed after this returns. */
    fun stop() {
        running = false
        thread?.join()
        thread = null
        record?.let {
            try {
                it.stop()
            } catch (_: IllegalStateException) {
            }
            it.release()
        }
        record = null
    }
}
