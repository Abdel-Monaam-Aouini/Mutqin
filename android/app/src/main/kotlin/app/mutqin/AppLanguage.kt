package app.mutqin

/**
 * The languages of the app. To add one: an entry here (its tag, its own name for itself, its English name, whether it is
 * written right to left), a `res/values-<tag>/strings.xml` and `arrays.xml` with the same names as `res/values/`,
 * and nothing else: the preferences list and the layout direction follow from this list. The selected tag is
 * stored in the app's private preferences.
 */
enum class AppLanguage(
    val tag: String,
    /** The language's name in that language (never translated, so it can always be found). */
    val nativeName: String,
    val englishName: String,
    val rtl: Boolean = false,
) {
    ENGLISH("en", "English", "English"),
    ARABIC("ar", "العربية", "Arabic", rtl = true),
    ;

    companion object {
        fun fromTag(tag: String?): AppLanguage = entries.firstOrNull { it.tag == tag } ?: ENGLISH
    }
}
