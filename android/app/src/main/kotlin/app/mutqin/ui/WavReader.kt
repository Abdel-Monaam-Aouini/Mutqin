package app.mutqin.ui

import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** A decoded WAV file: mono samples in [-1, 1] and the sample rate. Used by the debug replay only. */
class WavData(val samples: FloatArray, val sampleRate: Int)

/** Minimal WAV reader: 16-bit PCM or 32-bit float, mono or stereo (averaged). */
object WavReader {
    fun read(file: File): WavData {
        val bytes = ByteBuffer.wrap(file.readBytes()).order(ByteOrder.LITTLE_ENDIAN)
        require(bytes.limit() > 44 && bytes.getInt(0) == 0x46464952 && bytes.getInt(8) == 0x45564157) { "not a WAV file" }
        var pos = 12
        var format = 0
        var channels = 1
        var rate = 16000
        var bits = 16
        while (pos + 8 <= bytes.limit()) {
            val id = bytes.getInt(pos)
            val size = bytes.getInt(pos + 4)
            val body = pos + 8
            if (id == 0x20746d66) { // "fmt "
                format = bytes.getShort(body).toInt()
                channels = bytes.getShort(body + 2).toInt()
                rate = bytes.getInt(body + 4)
                bits = bytes.getShort(body + 14).toInt()
            } else if (id == 0x61746164) { // "data"
                val available = minOf(size, bytes.limit() - body)
                val frames = available / (bits / 8 * channels)
                val out = FloatArray(frames)
                for (i in 0 until frames) {
                    var sum = 0f
                    for (c in 0 until channels) {
                        val at = body + (i * channels + c) * (bits / 8)
                        sum += if (format == 3) bytes.getFloat(at) else bytes.getShort(at) / 32768f
                    }
                    out[i] = sum / channels
                }
                return WavData(out, rate)
            }
            pos = body + size + (size and 1)
        }
        error("no data chunk")
    }
}
