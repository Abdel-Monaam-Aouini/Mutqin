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
 * Lays its children out as lines of a justified paragraph, the way the lines of a mushaf are set.
 *
 * Plain flowing with stretched gaps leaves huge holes when only two or three long words fit on a line. Here the
 * line breaks are chosen so that the space left over on all the lines together is as small as it can be (an optimal
 * fit over the whole paragraph, not word by word). What is left on a line is then taken up by a bit more space
 * between the words (at most [maxExtraGap]) and, if that is not enough, by drawing the words slightly larger
 * (at most [maxScale]); a line that is a little too full is drawn slightly smaller (at least [minScale]). The last
 * line is not stretched. Each child is one unbreakable unit.
 */
@Composable
fun JustifiedFlow(
    modifier: Modifier = Modifier,
    minGap: Dp = 6.dp,
    maxExtraGap: Dp = 10.dp,
    maxScale: Float = 1.05f,
    minScale: Float = 0.95f,
    content: @Composable () -> Unit,
) {
    Layout(content, modifier) { measurables, constraints ->
        val width = constraints.maxWidth
        val gap = minGap.roundToPx()
        val extraCap = maxExtraGap.roundToPx()
        val placeables = measurables.map { it.measure(Constraints(maxWidth = width)) }
        val n = placeables.size

        // the best way to cut the units into lines: the smallest total of (leftover space / line width) squared
        val cost = DoubleArray(n + 1)
        val next = IntArray(n + 1)
        for (i in n - 1 downTo 0) {
            cost[i] = Double.MAX_VALUE
            var words = 0
            for (j in i + 1..n) {
                words += placeables[j - 1].width
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

        val rtl = layoutDirection == LayoutDirection.Rtl
        var totalHeight = 0

        class Line(
            val from: Int,
            val to: Int,
            val top: Int,
            val height: Int,
        )
        val lines = mutableListOf<Line>()
        var i = 0
        while (i < n) {
            val j = next[i]
            val height = (i until j).maxOf { placeables[it].height }
            lines.add(Line(i, j, totalHeight, height))
            totalHeight += height
            i = j
        }

        layout(width, totalHeight) {
            for ((lineIndex, line) in lines.withIndex()) {
                val count = line.to - line.from
                val words = (line.from until line.to).sumOf { placeables[it].width }
                val gaps = count - 1
                val last = lineIndex == lines.lastIndex
                var scale = 1f
                var gapUsed = gap.toFloat()
                if (!last && gaps > 0) {
                    val slack = width - words - gap * gaps
                    if (slack >= 0) {
                        val extra = min(slack.toFloat() / gaps, extraCap.toFloat())
                        val left = slack - extra * gaps
                        scale = if (left > 0) min(maxScale, 1f + left / words) else 1f
                    } else {
                        scale = max(minScale, (width - gap * gaps).toFloat() / words)
                    }
                    gapUsed = max(0f, (width - words * scale) / gaps)
                }
                var offset = 0f
                for (k in line.from until line.to) {
                    val p = placeables[k]
                    val w = p.width * scale
                    val x = if (rtl) width - offset - w else offset
                    val y = line.top + (line.height - p.height) / 2
                    p.placeWithLayer(x.toInt(), y) {
                        scaleX = scale
                        scaleY = scale
                        transformOrigin = TransformOrigin(0f, 0.5f)
                    }
                    offset += w + gapUsed
                }
            }
        }
    }
}
