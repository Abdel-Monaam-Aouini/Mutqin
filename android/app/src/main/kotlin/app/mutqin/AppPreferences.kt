package app.mutqin

import android.content.Context
import app.mutqin.core.Strictness

/** The few settings kept on the phone, in the app's private preferences file. */
object AppPreferences {
    private const val FILE = "mutqin_preferences"
    private const val LANGUAGE = "language"
    private const val STRICTNESS = "strictness"
    private const val MEMORIZATION = "memorization"

    private fun Context.prefs() = getSharedPreferences(FILE, Context.MODE_PRIVATE)

    fun language(context: Context): AppLanguage = AppLanguage.fromTag(context.prefs().getString(LANGUAGE, null))

    fun saveLanguage(
        context: Context,
        language: AppLanguage,
    ) = context
        .prefs()
        .edit()
        .putString(LANGUAGE, language.tag)
        .apply()

    fun memorization(context: Context): Boolean = context.prefs().getBoolean(MEMORIZATION, false)

    fun saveMemorization(
        context: Context,
        on: Boolean,
    ) = context
        .prefs()
        .edit()
        .putBoolean(MEMORIZATION, on)
        .apply()

    fun strictness(context: Context): Int =
        context.prefs().getInt(STRICTNESS, Strictness.STANDARD).takeIf { it in Strictness.RELAXED..Strictness.STRICT } ?: Strictness.STANDARD

    fun saveStrictness(
        context: Context,
        level: Int,
    ) = context
        .prefs()
        .edit()
        .putInt(STRICTNESS, level)
        .apply()
}
