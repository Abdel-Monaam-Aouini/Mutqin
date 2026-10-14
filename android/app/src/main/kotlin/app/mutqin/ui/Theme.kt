package app.mutqin.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color

/** Few colors (docs/APP_SPEC.md, section 5): two neutrals, a green accent, and one muted terracotta for every kind of problem. */
val LocalAttention = staticCompositionLocalOf { Color(0xFFB4533A) }

private val LightColors =
    lightColorScheme(
        primary = Color(0xFF0F5C4D),
        onPrimary = Color(0xFFF4F1EA),
        background = Color(0xFFF4F1EA),
        onBackground = Color(0xFF1B1F1E),
        surface = Color(0xFFF4F1EA),
        onSurface = Color(0xFF1B1F1E),
        secondaryContainer = Color(0xFFD9E6E1),
        onSecondaryContainer = Color(0xFF0F5C4D),
        surfaceVariant = Color(0xFFEAE5DA),
        onSurfaceVariant = Color(0xFF4A504E),
        outline = Color(0xFF8A908E),
    )

private val DarkColors =
    darkColorScheme(
        primary = Color(0xFF5FBFA6),
        onPrimary = Color(0xFF14181A),
        background = Color(0xFF14181A),
        onBackground = Color(0xFFECE8DF),
        surface = Color(0xFF14181A),
        onSurface = Color(0xFFECE8DF),
        secondaryContainer = Color(0xFF244139),
        onSecondaryContainer = Color(0xFF5FBFA6),
        surfaceVariant = Color(0xFF1E2528),
        onSurfaceVariant = Color(0xFFB4BAB7),
        outline = Color(0xFF7D8583),
    )

@Composable
fun MutqinTheme(content: @Composable () -> Unit) {
    val dark = isSystemInDarkTheme()
    CompositionLocalProvider(LocalAttention provides if (dark) Color(0xFFE0866C) else Color(0xFFB4533A)) {
        MaterialTheme(colorScheme = if (dark) DarkColors else LightColors, content = content)
    }
}
