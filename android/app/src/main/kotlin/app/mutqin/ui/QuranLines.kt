package app.mutqin.ui

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.layout.Layout
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import kotlin.math.max
import kotlin.math.min

/**
 * One piece of the text that is never split between lines: a word of the Tanzil text, with the ring of the ayah
 * after the last word of it. An item of the session can be several words (the model's units join words), so an item
 * is cut into its words here and every word is laid out on its own: gaps between words are then all alike.
 */
internal data class QuranUnit(
    val item: Int,
    val ayah: Int,
    val text: String,
    val lastPiece: Boolean,
    /** The number of the ayah to draw a ring for after this word, or null. */
    val ring: Int?,
)

/** The lines of the whole range, cut once, so the text flows on from ayah to ayah like in a mushaf. */
internal class QuranFlow(
    val units: List<QuranUnit>,
    /** Unit ranges, one per line. */
    val lines: List<IntRange>,
    /** The first lines are the basmala: centered, not stretched. */
    val basmalaLines: Int,
    /** The first line that holds a word of each item. */
    val lineOfItem: Map<Int, Int>,
)

internal fun unitsOf(items: List<UiItem>): List<QuranUnit> {
    val out = ArrayList<QuranUnit>()
    for ((i, item) in items.withIndex()) {
        val pieces = item.text.split(' ').filter { it.isNotEmpty() }
        val endsAyah = i == items.lastIndex || items[i + 1].ayah != item.ayah
        for ((k, piece) in pieces.withIndex()) {
            val last = k == pieces.lastIndex
            out.add(QuranUnit(item.index, item.ayah, piece, last, if (last && endsAyah && item.ayah > 0) item.ayah else null))
        }
    }
    return out
}

/**
 * Cuts widths into lines so that the space left over on all the lines together is as small as it can be (an optimal
 * fit over the whole paragraph, not word by word); the last line costs nothing. A line may be a little too full
 * (its words are then drawn up to [minScale] smaller).
 */
internal fun breakLines(
    widths: IntArray,
    width: Int,
    gap: Int,
    minScale: Float,
): List<IntRange> {
    val n = widths.size
    val cost = DoubleArray(n + 1)
    val next = IntArray(n + 1)
    for (i in n - 1 downTo 0) {
        cost[i] = Double.MAX_VALUE
        var words = 0
        for (j in i + 1..n) {
            words += widths[j - 1]
            val gaps = gap * (j - i - 1)
            val natural = words + gaps
            val line =
                when {
                    natural <= width -> if (j == n) 0.0 else ((width - natural).toDouble() / width).let { it * it }

                    j - i == 1 -> 0.0

                    // a single unit that is wider than the line stays alone
                    words * minScale + gaps <= width -> ((natural - width).toDouble() / width).let { it * it * 8 }

                    else -> break // longer lines only get fuller
                }
            val total = line + cost[j]
            if (total < cost[i]) {
                cost[i] = total
                next[i] = j
            }
        }
    }
    val lines = ArrayList<IntRange>()
    var i = 0
    while (i < n) {
        lines.add(i until next[i])
        i = next[i]
    }
    return lines
}

internal fun flowOf(
    units: List<QuranUnit>,
    width: Int,
    gap: Int,
    minScale: Float,
    widthOf: (QuranUnit) -> Int,
): QuranFlow {
    val basmalaCount = units.takeWhile { it.ayah == 0 }.size
    val widths = IntArray(units.size) { widthOf(units[it]) }
    val basmala = breakLines(widths.copyOfRange(0, basmalaCount), width, gap, minScale)
    val rest = breakLines(widths.copyOfRange(basmalaCount, units.size), width, gap, minScale).map { (it.first + basmalaCount)..(it.last + basmalaCount) }
    val lines = basmala + rest
    val lineOfItem = HashMap<Int, Int>()
    for ((index, range) in lines.withIndex()) {
        for (u in range) lineOfItem.putIfAbsent(units[u].item, index)
    }
    return QuranFlow(units, lines, basmala.size, lineOfItem)
}

internal enum class LineMode { Justified, Start, Centered }

/**
 * One line of the Quran text. [LineMode.Justified] stretches it from edge to edge: first by a little more space between
 * the words (at most [maxExtraGap]), then by drawing the words slightly larger (at most [maxScale]); a line that is
 * too full is drawn slightly smaller (at least 0.85). The other modes keep the natural gaps.
 */
@Composable
internal fun JustifiedLine(
    mode: LineMode,
    modifier: Modifier = Modifier,
    minGap: Dp = 6.dp,
    maxExtraGap: Dp = 10.dp,
    maxScale: Float = 1.07f,
    content: @Composable () -> Unit,
) {
    Layout(content, modifier) { measurables, constraints ->
        val width = constraints.maxWidth
        val gap = minGap.roundToPx()
        val extraCap = maxExtraGap.roundToPx()
        val placeables = measurables.map { it.measure(Constraints(maxWidth = width)) }
        val words = placeables.sumOf { it.width }
        val gaps = max(0, placeables.size - 1)
        val height = placeables.maxOfOrNull { it.height } ?: 0

        var scale = 1f
        var gapUsed = gap.toFloat()
        val slack = width - words - gap * gaps
        if (slack < 0 && words > 0) {
            scale = max(0.85f, (width - gap * gaps).toFloat() / words) // too full
            if (mode == LineMode.Justified && gaps > 0) gapUsed = max(0f, (width - words * scale) / gaps)
        } else if (mode == LineMode.Justified && gaps > 0) {
            val extra = min(slack.toFloat() / gaps, extraCap.toFloat())
            val left = slack - extra * gaps
            scale = if (left > 0) min(maxScale, 1f + left / words) else 1f
            gapUsed = max(0f, (width - words * scale) / gaps)
        }
        val total = words * scale + gapUsed * gaps
        val rtl = layoutDirection == LayoutDirection.Rtl
        layout(width, height) {
            var offset = if (mode == LineMode.Centered) max(0f, (width - total) / 2) else 0f
            for (p in placeables) {
                val w = p.width * scale
                val x = if (rtl) width - offset - w else offset
                p.placeWithLayer(x.toInt(), (height - p.height) / 2) {
                    scaleX = scale
                    scaleY = scale
                    transformOrigin = TransformOrigin(0f, 0.5f)
                }
                offset += w + gapUsed
            }
        }
    }
}
