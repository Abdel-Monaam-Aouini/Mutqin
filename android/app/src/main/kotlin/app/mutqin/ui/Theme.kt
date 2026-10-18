package app.mutqin.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.mutqin.R

/**
 * White pages and several greens: deep green for text and buttons, a fresh mid green for the ornaments, pale mint
 * for cards and soft green for borders, and one muted terracotta for every kind of problem (docs/APP_SPEC.md,
 * section 5). Dark mode is the same page at night: pale green ink on a very dark green.
 */
val LocalAttention = staticCompositionLocalOf { Color(0xFFB04A32) }

/** The accent green of the ornaments (kept under its old name): ayah rings, rules, the surah banner. Never used for text that must be read. */
val LocalGold = staticCompositionLocalOf { Color(0xFF3FA57A) }

/**
 * The font of every Quran word: Amiri Quran (SIL Open Font License 1.1, res/font/amiri_quran.ttf, bundled in the app
 * with its license text in assets/licenses), so the text and its marks look the same on every phone instead of
 * depending on the phone's own Arabic font. It has every character of the verified Tanzil text.
 */
val QuranFont = FontFamily(Font(R.font.amiri_quran))

private val LightColors =
    lightColorScheme(
        primary = Color(0xFF17694A),
        onPrimary = Color(0xFFFFFFFF),
        background = Color(0xFFFFFFFF),
        onBackground = Color(0xFF10281E),
        surface = Color(0xFFFFFFFF),
        onSurface = Color(0xFF10281E),
        surfaceVariant = Color(0xFFF2F8F4),
        onSurfaceVariant = Color(0xFF567565),
        secondaryContainer = Color(0xFFDDEFE4),
        onSecondaryContainer = Color(0xFF17694A),
        outline = Color(0xFF8AA697),
        outlineVariant = Color(0xFFCBE3D4),
    )

private val DarkColors =
    darkColorScheme(
        primary = Color(0xFF6FD3A4),
        onPrimary = Color(0xFF06241A),
        background = Color(0xFF0A1410),
        onBackground = Color(0xFFE3F2EA),
        surface = Color(0xFF0A1410),
        onSurface = Color(0xFFE3F2EA),
        surfaceVariant = Color(0xFF12201A),
        onSurfaceVariant = Color(0xFF9CB8A9),
        secondaryContainer = Color(0xFF1B3329),
        onSecondaryContainer = Color(0xFF6FD3A4),
        outline = Color(0xFF5F7E6E),
        outlineVariant = Color(0xFF25392F),
    )

private val Serif = FontFamily.Serif

/** Headings in a serif (like the title pages of a book); everything else in the system font. */
private val MutqinTypography =
    Typography(
        displaySmall = TextStyle(fontFamily = Serif, fontWeight = FontWeight.Bold, fontSize = 38.sp, lineHeight = 46.sp),
        headlineMedium = TextStyle(fontFamily = Serif, fontWeight = FontWeight.SemiBold, fontSize = 28.sp, lineHeight = 36.sp),
        headlineSmall = TextStyle(fontFamily = Serif, fontWeight = FontWeight.SemiBold, fontSize = 24.sp, lineHeight = 32.sp),
        titleLarge = TextStyle(fontFamily = Serif, fontWeight = FontWeight.SemiBold, fontSize = 22.sp, lineHeight = 28.sp),
        titleMedium = TextStyle(fontFamily = Serif, fontWeight = FontWeight.SemiBold, fontSize = 18.sp, lineHeight = 24.sp),
        titleSmall = TextStyle(fontWeight = FontWeight.SemiBold, fontSize = 15.sp, lineHeight = 21.sp),
        bodyLarge = TextStyle(fontSize = 16.sp, lineHeight = 24.sp),
        bodyMedium = TextStyle(fontSize = 14.sp, lineHeight = 21.sp),
        bodySmall = TextStyle(fontSize = 12.sp, lineHeight = 17.sp),
        labelLarge = TextStyle(fontWeight = FontWeight.SemiBold, fontSize = 13.sp, lineHeight = 18.sp),
    )

private val MutqinShapes =
    Shapes(
        small = RoundedCornerShape(10.dp),
        medium = RoundedCornerShape(14.dp),
        large = RoundedCornerShape(20.dp),
        extraLarge = RoundedCornerShape(28.dp),
    )

/** Which page the app is set on: follow the phone, or always the paper, or always the night page. */
enum class ThemeMode(
    val key: String,
) {
    System("system"),
    Light("light"),
    Dark("dark"),
    ;

    companion object {
        fun fromKey(key: String?): ThemeMode = entries.firstOrNull { it.key == key } ?: System
    }
}

/** The theme setting as the preferences screen needs it: what is shown now, what is kept, and how to change them. */
class Appearance(
    val mode: ThemeMode,
    val saved: ThemeMode,
    val preview: (ThemeMode) -> Unit,
    val save: (ThemeMode) -> Unit,
)

@Composable
fun MutqinTheme(
    mode: ThemeMode = ThemeMode.System,
    content: @Composable () -> Unit,
) {
    val dark =
        when (mode) {
            ThemeMode.System -> isSystemInDarkTheme()
            ThemeMode.Light -> false
            ThemeMode.Dark -> true
        }
    CompositionLocalProvider(
        LocalAttention provides if (dark) Color(0xFFE4896C) else Color(0xFFB04A32),
        LocalGold provides if (dark) Color(0xFF4DBE8B) else Color(0xFF3FA57A),
    ) {
        MaterialTheme(
            colorScheme = if (dark) DarkColors else LightColors,
            typography = MutqinTypography,
            shapes = MutqinShapes,
            content = content,
        )
    }
}
