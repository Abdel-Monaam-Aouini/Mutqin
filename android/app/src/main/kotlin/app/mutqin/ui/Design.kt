package app.mutqin.ui

import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.util.Locale

/** A card like a ruled panel of a printed page: paper, a thin tan border, no shadow. */
@Composable
fun PaperCard(
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit,
) {
    Surface(
        modifier.fillMaxWidth(),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surfaceVariant,
        contentColor = MaterialTheme.colorScheme.onBackground,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
    ) {
        Column(Modifier.padding(horizontal = 18.dp, vertical = 16.dp), content = content)
    }
}

/** A small label over a section: capitals with spacing in Latin scripts (spacing would break Arabic letters). */
@Composable
fun Eyebrow(
    text: String,
    modifier: Modifier = Modifier,
    color: Color = LocalGold.current,
) {
    val arabic = LocalLayoutDirection.current == LayoutDirection.Rtl
    Text(
        if (arabic) text else text.uppercase(Locale.getDefault()),
        modifier,
        color = color,
        fontWeight = FontWeight.SemiBold,
        fontSize = 12.sp,
        letterSpacing = if (arabic) 0.sp else 1.6.sp,
    )
}

/** A thin double rule with a small diamond in the middle, the way chapters are divided in a mushaf. */
@Composable
fun OrnamentRule(
    modifier: Modifier = Modifier,
    color: Color = LocalGold.current,
) {
    Canvas(modifier.fillMaxWidth().height(10.dp)) {
        val mid = size.height / 2
        val gap = 10.dp.toPx()
        val cx = size.width / 2
        val stroke = 1.dp.toPx()
        drawLine(color.copy(alpha = 0.55f), Offset(0f, mid), Offset(cx - gap, mid), stroke)
        drawLine(color.copy(alpha = 0.55f), Offset(cx + gap, mid), Offset(size.width, mid), stroke)
        val d = 4.dp.toPx()
        val diamond =
            Path().apply {
                moveTo(cx, mid - d)
                lineTo(cx + d, mid)
                lineTo(cx, mid + d)
                lineTo(cx - d, mid)
                close()
            }
        drawPath(diamond, color, style = Stroke(stroke))
    }
}

/** A round outlined button for the small actions of a screen (back, settings, history). */
@Composable
fun RoundAction(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val scale by animateFloatAsState(if (pressed) 0.92f else 1f, spring(Spring.DampingRatioMediumBouncy), label = "press")
    Box(
        modifier
            .scale(scale)
            .size(44.dp)
            .clip(CircleShape)
            .border(1.dp, MaterialTheme.colorScheme.outlineVariant, CircleShape)
            .clickable(interactionSource = source, indication = androidx.compose.material3.ripple(), onClick = onClick),
        contentAlignment = Alignment.Center,
    ) { content() }
}

/** The header of a sub-screen: a back button, a small label and a serif title. */
@Composable
fun HeaderBar(
    eyebrow: String,
    title: String,
    onBack: (() -> Unit)?,
    trailing: @Composable RowScope.() -> Unit = {},
    backIcon: @Composable () -> Unit = {},
) {
    Row(Modifier.fillMaxWidth().padding(top = 12.dp, bottom = 14.dp), verticalAlignment = Alignment.CenterVertically) {
        if (onBack != null) {
            RoundAction(onBack) { backIcon() }
            Spacer(Modifier.width(14.dp))
        }
        Column(Modifier.weight(1f)) {
            Eyebrow(eyebrow)
            Text(title, style = MaterialTheme.typography.headlineMedium)
        }
        trailing()
    }
}

@Composable
fun PrimaryButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    trailing: String? = null,
) {
    val source = remember { MutableInteractionSource() }
    val pressed by source.collectIsPressedAsState()
    val scale by animateFloatAsState(if (pressed) 0.97f else 1f, spring(Spring.DampingRatioMediumBouncy), label = "press")
    Button(
        onClick = onClick,
        modifier =
            modifier
                .graphicsLayer {
                    scaleX = scale
                    scaleY = scale
                }.height(54.dp),
        shape = RoundedCornerShape(14.dp),
        interactionSource = source,
        colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary, contentColor = MaterialTheme.colorScheme.onPrimary),
    ) {
        Text(text, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
        if (trailing != null) Text("  $trailing", fontSize = 16.sp)
    }
}

@Composable
fun SecondaryButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    OutlinedButton(
        onClick = onClick,
        modifier = modifier.height(48.dp),
        shape = RoundedCornerShape(14.dp),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
        colors = ButtonDefaults.outlinedButtonColors(contentColor = MaterialTheme.colorScheme.primary),
    ) { Text(text, fontWeight = FontWeight.SemiBold) }
}
