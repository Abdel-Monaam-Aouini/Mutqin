package app.mutqin

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/** One finished recitation attempt, as kept in the history. */
data class HistoryEntry(
    val time: Long,
    val surah: Int,
    val fromAyah: Int,
    val toAyah: Int,
    val seconds: Int,
    val wrong: Int,
    val skipped: Int,
    val repeated: Int,
    val added: Int,
    val memorization: Boolean,
) {
    val differences: Int get() = wrong + skipped + repeated + added
}

/**
 * The history of attempts, a small JSON file in the app's private folder. It never leaves the phone (the app has no
 * network permission) and keeps the last [LIMIT] attempts.
 */
object HistoryStore {
    private const val FILE = "history.json"
    private const val LIMIT = 200

    private fun file(context: Context) = File(context.filesDir, FILE)

    @Synchronized
    fun load(context: Context): List<HistoryEntry> =
        try {
            val f = file(context)
            if (!f.isFile) {
                emptyList()
            } else {
                val array = JSONArray(f.readText())
                (0 until array.length()).map { i ->
                    val o = array.getJSONObject(i)
                    HistoryEntry(
                        o.getLong("time"),
                        o.getInt("surah"),
                        o.getInt("from"),
                        o.getInt("to"),
                        o.getInt("seconds"),
                        o.getInt("wrong"),
                        o.getInt("skipped"),
                        o.getInt("repeated"),
                        o.getInt("added"),
                        o.optBoolean("memorization", false),
                    )
                }
            }
        } catch (_: Exception) {
            emptyList() // a damaged file is treated as an empty history
        }

    @Synchronized
    fun append(
        context: Context,
        entry: HistoryEntry,
    ) {
        val all = (load(context) + entry).takeLast(LIMIT)
        val array = JSONArray()
        for (e in all) {
            array.put(
                JSONObject()
                    .put("time", e.time)
                    .put("surah", e.surah)
                    .put("from", e.fromAyah)
                    .put("to", e.toAyah)
                    .put("seconds", e.seconds)
                    .put("wrong", e.wrong)
                    .put("skipped", e.skipped)
                    .put("repeated", e.repeated)
                    .put("added", e.added)
                    .put("memorization", e.memorization),
            )
        }
        val temporary = File(context.filesDir, "$FILE.part")
        temporary.writeText(array.toString())
        if (!temporary.renameTo(file(context))) temporary.delete()
    }

    @Synchronized
    fun clear(context: Context) {
        file(context).delete()
    }
}
