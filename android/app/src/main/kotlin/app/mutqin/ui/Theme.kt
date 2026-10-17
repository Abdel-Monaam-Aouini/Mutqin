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
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/**
 * The look of a printed mushaf: warm paper, deep green ink, a little gold for ornament, and one muted terracotta for
 * every kind of problem (docs/APP_SPEC.md, section 5). Dark mode is the same page at night: cream ink on deep green.
 */
val LocalAttention = staticCompositionLocalOf { Color(0xFFB04A32) }

/** The gold of the ornaments: ayah rings, rules and the surah banner. Never used for text that must be read. */
val LocalGold = staticCompositionLocalOf { Color(0xFFA8832E) }

private val LightColors =
    lightColorScheme(
        primary = Color(0xFF1C5B4B),
        onPrimary = Color(0xFFF7F1DF),
        background = Color(0xFFF4EEDC),
        onBackground = Color(0xFF22302A),
        surface = Color(0xFFF4EEDC),
        onSurface = Color(0xFF22302A),
        surfaceVariant = Color(0xFFFBF7EA),
        onSurfaceVariant = Color(0xFF66766D),
        secondaryContainer = Color(0xFFE4E7D2),
        onSecondaryContainer = Color(0xFF1C5B4B),
        outline = Color(0xFF8E9A8F),
        outlineVariant = Color(0xFFDDD0AB),
    )

private val DarkColors =
    darkColorScheme(
        primary = Color(0xFF7DC9AC),
        onPrimary = Color(0xFF0C241C),
        background = Color(0xFF0F1613),
        onBackground = Color(0xFFECE5D0),
        surface = Color(0xFF0F1613),
        onSurface = Color(0xFFECE5D0),
        surfaceVariant = Color(0xFF17211C),
        onSurfaceVariant = Color(0xFFA2B0A6),
        secondaryContainer = Color(0xFF22332B),
        onSecondaryContainer = Color(0xFF7DC9AC),
        outline = Color(0xFF6D7C73),
        outlineVariant = Color(0xFF2B3A32),
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
        LocalGold provides if (dark) Color(0xFFD3B567) else Color(0xFFA8832E),
    ) {
        MaterialTheme(
            colorScheme = if (dark) DarkColors else LightColors,
            typography = MutqinTypography,
            shapes = MutqinShapes,
            content = content,
        )
    }
}
