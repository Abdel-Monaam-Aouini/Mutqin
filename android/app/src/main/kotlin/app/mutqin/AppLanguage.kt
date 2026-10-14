package app.mutqin

/** Languages available in the app. The selected tag is stored in private app preferences. */
enum class AppLanguage(
    val tag: String,
) {
    ENGLISH("en"),
    ARABIC("ar"),
    ;

    companion object {
        fun fromTag(tag: String?): AppLanguage =
            entries.firstOrNull { it.tag == tag } ?: ENGLISH
    }
}
