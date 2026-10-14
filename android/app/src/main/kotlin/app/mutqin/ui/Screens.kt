package app.mutqin.ui

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.mutqin.R
import app.mutqin.core.EventKind
import app.mutqin.core.Strictness
import kotlinx.coroutines.delay
import kotlinx.coroutines.withTimeoutOrNull

/** The screen's root: a surface in the theme colors that keeps clear of the status and navigation bars. */
@Composable
fun MutqinApp(state: UiState, vm: MainViewModel, onStart: () -> Unit) {
    Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background, contentColor = MaterialTheme.colorScheme.onBackground) {
        Box(Modifier.fillMaxSize().safeDrawingPadding().padding(horizontal = 20.dp)) {
            when (state.phase) {
                Phase.Loading -> Centered { CircularProgressIndicator(); Spacer(Modifier.height(14.dp)); Text(stringResource(R.string.loading)) }
                Phase.NeedModel -> NeedModelScreen(state, onCheck = vm::prepare)
                Phase.Error -> ErrorScreen(state.error.orEmpty(), onRetry = vm::prepare)
                Phase.Setup -> SetupScreen(state, vm, onStart)
                Phase.Starting -> Centered { CircularProgressIndicator(); Spacer(Modifier.height(14.dp)); Text(stringResource(R.string.preparing_session)) }
                Phase.Reciting, Phase.Result -> ReciteScreen(state, vm)
            }
        }
    }
}

@Composable
private fun Centered(content: @Composable () -> Unit) {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) { content() }
}

@Composable
private fun Card(modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Surface(modifier.fillMaxWidth(), shape = RoundedCornerShape(20.dp), color = MaterialTheme.colorScheme.surfaceVariant) {
        Column(Modifier.padding(18.dp)) { content() }
    }
}

/** The exact error, selectable, with a button to copy it (for a bug report). */
@Composable
private fun ErrorScreen(error: String, onRetry: () -> Unit) {
    val clipboard = LocalClipboardManager.current
    var copied by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxSize().padding(vertical = 24.dp)) {
        Text(stringResource(R.string.error_quran_title), style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.SemiBold, color = LocalAttention.current)
        Spacer(Modifier.height(8.dp))
        Text(stringResource(R.string.error_quran_body), color = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.height(14.dp))
        Card(Modifier.weight(1f, fill = false)) {
            SelectionContainer {
                Text(
                    error,
                    Modifier.verticalScroll(rememberScrollState()),
                    fontFamily = FontFamily.Monospace,
                    fontSize = 13.sp,
                    lineHeight = 19.sp,
                )
            }
        }
        Spacer(Modifier.weight(1f))
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            OutlinedButton(
                onClick = { clipboard.setText(AnnotatedString(error)); copied = true },
                modifier = Modifier.weight(1f).height(56.dp),
                shape = RoundedCornerShape(16.dp),
            ) { Text(stringResource(if (copied) R.string.error_copied else R.string.error_copy)) }
            Button(onClick = onRetry, modifier = Modifier.weight(1f).height(56.dp), shape = RoundedCornerShape(16.dp)) {
                Text(stringResource(R.string.retry_check))
            }
        }
    }
}

@Composable
private fun NeedModelScreen(state: UiState, onCheck: () -> Unit) {
    Column(Modifier.fillMaxSize().padding(vertical = 24.dp), verticalArrangement = Arrangement.Center) {
        Text(stringResource(R.string.need_model_title), style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.SemiBold)
        Spacer(Modifier.height(12.dp))
        Text(stringResource(R.string.need_model_body))
        Spacer(Modifier.height(12.dp))
        Card {
            Text(state.modelFolder, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.height(8.dp))
            Text(stringResource(R.string.need_model_files), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Spacer(Modifier.height(24.dp))
        Button(onClick = onCheck, modifier = Modifier.fillMaxWidth().height(56.dp), shape = RoundedCornerShape(16.dp)) {
            Text(stringResource(R.string.retry_check), fontSize = 17.sp)
        }
    }
}

// ---------------------------------------------------------------------------------------------- setup

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SetupScreen(state: UiState, vm: MainViewModel, onStart: () -> Unit) {
    Column(Modifier.fillMaxSize()) {
        Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            Column(Modifier.padding(top = 12.dp, bottom = 4.dp)) {
                Text(stringResource(R.string.app_name), style = MaterialTheme.typography.displaySmall, fontWeight = FontWeight.Bold, color = MaterialTheme.colorScheme.primary)
                Text(stringResource(R.string.app_tagline), style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }

            Card {
                Text(stringResource(R.string.surah), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Row(Modifier.fillMaxWidth().padding(top = 6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(state.surah.toString(), style = MaterialTheme.typography.displayMedium, fontWeight = FontWeight.SemiBold)
                        Text(stringResource(R.string.ayahs_in_surah, state.ayahCount), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    StepButton("−", state.surah > 1) { vm.setSurah(state.surah - 1) }
                    Spacer(Modifier.width(12.dp))
                    StepButton("+", state.surah < 114) { vm.setSurah(state.surah + 1) }
                }
            }

            Card {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(R.string.range_summary, state.fromAyah, state.toAyah), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                    }
                    TextButton(onClick = { vm.setFrom(1); vm.setTo(state.ayahCount) }) { Text(stringResource(R.string.whole_surah)) }
                }
                Spacer(Modifier.height(10.dp))
                RangeRow(stringResource(R.string.ayah_from), state.fromAyah, state.fromAyah > 1, state.fromAyah < state.ayahCount,
                    { vm.setFrom(state.fromAyah - 1) }, { vm.setFrom(state.fromAyah + 1) })
                Spacer(Modifier.height(8.dp))
                RangeRow(stringResource(R.string.ayah_to), state.toAyah, state.toAyah > 1, state.toAyah < state.ayahCount,
                    { vm.setTo(state.toAyah - 1) }, { vm.setTo(state.toAyah + 1) })
            }

            Card {
                Text(stringResource(R.string.strictness), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Spacer(Modifier.height(10.dp))
                val levels = listOf(
                    Triple(Strictness.RELAXED, R.string.strictness_relaxed, R.string.strictness_relaxed_hint),
                    Triple(Strictness.STANDARD, R.string.strictness_standard, R.string.strictness_standard_hint),
                    Triple(Strictness.STRICT, R.string.strictness_strict, R.string.strictness_strict_hint),
                )
                SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                    levels.forEachIndexed { i, (level, label, _) ->
                        SegmentedButton(
                            selected = state.strictness == level,
                            onClick = { vm.setStrictness(level) },
                            shape = SegmentedButtonDefaults.itemShape(i, levels.size),
                        ) { Text(stringResource(label)) }
                    }
                }
                Spacer(Modifier.height(10.dp))
                Text(stringResource(levels.first { it.first == state.strictness }.third), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }

            state.message?.let {
                Card {
                    if (it == "range") {
                        Text(stringResource(R.string.range_too_long), color = LocalAttention.current)
                    } else {
                        SelectionContainer {
                            Text(it, color = LocalAttention.current, fontFamily = FontFamily.Monospace, fontSize = 13.sp, lineHeight = 19.sp)
                        }
                    }
                }
            }
        }
        Spacer(Modifier.height(12.dp))
        Button(onClick = onStart, modifier = Modifier.fillMaxWidth().height(60.dp), shape = RoundedCornerShape(18.dp)) {
            Text(stringResource(R.string.start), fontSize = 18.sp, fontWeight = FontWeight.SemiBold)
        }
        Spacer(Modifier.height(8.dp))
    }
}

@Composable
private fun RangeRow(label: String, value: Int, canMinus: Boolean, canPlus: Boolean, onMinus: () -> Unit, onPlus: () -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyLarge)
        StepButton("−", canMinus, onMinus)
        Text(value.toString(), Modifier.width(56.dp), textAlign = TextAlign.Center, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold)
        StepButton("+", canPlus, onPlus)
    }
}

/** A round − or + button; holding it repeats, so a surah number far away can be reached quickly. */
@Composable
private fun StepButton(label: String, enabled: Boolean, onStep: () -> Unit) {
    val step by rememberUpdatedState(onStep)
    val accent = MaterialTheme.colorScheme.primary
    Box(
        Modifier
            .size(52.dp)
            .clip(CircleShape)
            .background(accent.copy(alpha = if (enabled) 0.16f else 0.06f))
            .pointerInput(enabled) {
                if (!enabled) return@pointerInput
                awaitEachGesture {
                    awaitFirstDown()
                    step()
                    // The repeat follows the finger: it starts only if the finger is still down after a moment,
                    // and stops the instant it lifts (a timer alone could keep stepping after a quick tap).
                    var released = false
                    withTimeoutOrNull(450L) {
                        waitForUpOrCancellation()
                        released = true
                    }
                    while (!released) {
                        step()
                        withTimeoutOrNull(80L) {
                            waitForUpOrCancellation()
                            released = true
                        }
                    }
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        Text(label, fontSize = 26.sp, color = accent.copy(alpha = if (enabled) 1f else 0.35f))
    }
}

// ----------------------------------------------------------------------------------------- recitation

@Composable
private fun ReciteScreen(state: UiState, vm: MainViewModel) {
    val groups = remember(state.items) { state.items.groupBy { it.ayah }.toList() }
    val listState = rememberLazyListState()
    val currentGroup = groups.indexOfFirst { (_, words) -> words.any { it.state == ItemState.Current } }
    LaunchedEffect(currentGroup) { if (currentGroup >= 0) listState.animateScrollToItem(currentGroup) }
    val reciting = state.phase == Phase.Reciting

    Column(Modifier.fillMaxSize()) {
        TopBar(state, reciting, onClose = vm::back)
        // the Quran text is always right to left, whatever the language of the app
        CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
            LazyColumn(
                Modifier.weight(1f).fillMaxWidth(),
                state = listState,
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                itemsIndexed(groups) { _, (ayah, words) -> AyahBlock(ayah, words) }
            }
        }
        if (reciting) ListeningPanel(state, onStop = vm::stop) else ResultPanel(state, onBack = vm::back, onRetry = vm::retry)
    }
}

@Composable
private fun TopBar(state: UiState, reciting: Boolean, onClose: () -> Unit) {
    Row(Modifier.fillMaxWidth().padding(vertical = 10.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(
            Modifier.size(44.dp).clip(CircleShape).background(MaterialTheme.colorScheme.surfaceVariant).clickable(onClick = onClose),
            contentAlignment = Alignment.Center,
        ) { Text("✕", fontSize = 18.sp, color = MaterialTheme.colorScheme.onSurfaceVariant) }
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(stringResource(R.string.range_title, stringResource(R.string.surah), state.surah), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            Text(stringResource(R.string.range_summary, state.fromAyah, state.toAyah), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        if (reciting) RecordingBadge(state.startedAt)
    }
}

@Composable
private fun RecordingBadge(startedAt: Long) {
    val pulse = rememberInfiniteTransition(label = "pulse").animateFloat(
        initialValue = 0.35f, targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(800), RepeatMode.Reverse), label = "dot",
    )
    var now by remember { mutableLongStateOf(android.os.SystemClock.elapsedRealtime()) }
    LaunchedEffect(Unit) {
        while (true) {
            now = android.os.SystemClock.elapsedRealtime()
            delay(500)
        }
    }
    val seconds = ((now - startedAt) / 1000).coerceAtLeast(0)
    Row(
        Modifier.clip(RoundedCornerShape(50)).background(LocalAttention.current.copy(alpha = 0.14f)).padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(10.dp).alpha(pulse.value).clip(CircleShape).background(LocalAttention.current))
        Spacer(Modifier.width(8.dp))
        Text("%d:%02d".format(seconds / 60, seconds % 60), style = MaterialTheme.typography.labelLarge, color = LocalAttention.current)
    }
}

@Composable
private fun ListeningPanel(state: UiState, onStop: () -> Unit) {
    // a long stretch of near silence means the phone cannot hear the reciter: say so
    var quietSince by remember { mutableLongStateOf(0L) }
    var quiet by remember { mutableStateOf(false) }
    LaunchedEffect(state.level) {
        val now = android.os.SystemClock.elapsedRealtime()
        if (state.level > 0.2f) {
            quietSince = 0L
            quiet = false
        } else {
            if (quietSince == 0L) quietSince = now
            quiet = now - quietSince > 4000
        }
    }
    Column(Modifier.fillMaxWidth().padding(top = 8.dp)) {
        LinearProgressIndicator(
            progress = { state.level },
            modifier = Modifier.fillMaxWidth().height(8.dp).clip(RoundedCornerShape(4.dp)),
            color = if (quiet) LocalAttention.current else MaterialTheme.colorScheme.primary,
            trackColor = MaterialTheme.colorScheme.surfaceVariant,
            drawStopIndicator = {},
        )
        Spacer(Modifier.height(10.dp))
        Text(
            stringResource(if (quiet) R.string.hint_quiet else R.string.hint_ok),
            style = MaterialTheme.typography.bodyMedium,
            color = if (quiet) LocalAttention.current else MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(Modifier.height(12.dp))
        Button(onClick = onStop, modifier = Modifier.fillMaxWidth().height(60.dp), shape = RoundedCornerShape(18.dp)) {
            Text(stringResource(R.string.stop), fontSize = 18.sp, fontWeight = FontWeight.SemiBold)
        }
        Spacer(Modifier.height(8.dp))
    }
}

@Composable
private fun ResultPanel(state: UiState, onBack: () -> Unit, onRetry: () -> Unit) {
    Column(Modifier.fillMaxWidth().padding(top = 8.dp)) {
        Card {
            Text(stringResource(R.string.result_title), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(6.dp))
            if (state.findings.isEmpty()) {
                Text(stringResource(R.string.nothing_found), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                Column(Modifier.heightIn(max = 160.dp).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    state.findings.forEach { f ->
                        val kind = when (f.kind) {
                            EventKind.Wrong -> R.string.kind_wrong
                            EventKind.Skipped -> R.string.kind_skipped
                            EventKind.Repeated -> R.string.kind_repeated
                            EventKind.Added -> R.string.kind_added
                        }
                        val where = if (f.ayah == 0) stringResource(R.string.basmala) else stringResource(R.string.where_format, f.ayah, f.word)
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            MarkSample(f.kind)
                            Spacer(Modifier.width(10.dp))
                            Text("${stringResource(kind)} · $where", style = MaterialTheme.typography.bodyMedium)
                        }
                    }
                }
            }
        }
        Spacer(Modifier.height(12.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            OutlinedButton(onClick = onBack, modifier = Modifier.weight(1f).height(56.dp), shape = RoundedCornerShape(16.dp)) { Text(stringResource(R.string.back)) }
            Button(onClick = onRetry, modifier = Modifier.weight(1f).height(56.dp), shape = RoundedCornerShape(16.dp)) { Text(stringResource(R.string.try_again)) }
        }
        Spacer(Modifier.height(8.dp))
    }
}

/** The small picture of a mark, shown next to a finding so the list reads like the text above. */
@Composable
private fun MarkSample(kind: EventKind) {
    val attention = LocalAttention.current
    Box(
        Modifier.size(30.dp, 16.dp).drawBehind {
            val y = size.height / 2
            val stroke = 2.5.dp.toPx()
            when (kind) {
                EventKind.Wrong -> drawLine(attention, Offset(0f, y), Offset(size.width, y), strokeWidth = stroke)
                EventKind.Skipped -> drawLine(attention, Offset(0f, y), Offset(size.width, y), strokeWidth = stroke, pathEffect = PathEffect.dashPathEffect(floatArrayOf(6f, 6f)))
                EventKind.Repeated -> {
                    drawLine(attention, Offset(0f, y - 3.dp.toPx()), Offset(size.width, y - 3.dp.toPx()), strokeWidth = stroke / 1.5f)
                    drawLine(attention, Offset(0f, y + 3.dp.toPx()), Offset(size.width, y + 3.dp.toPx()), strokeWidth = stroke / 1.5f)
                }
                EventKind.Added -> {
                    drawLine(attention, Offset(size.width / 2 - 6.dp.toPx(), y + 4.dp.toPx()), Offset(size.width / 2, y - 4.dp.toPx()), strokeWidth = stroke)
                    drawLine(attention, Offset(size.width / 2, y - 4.dp.toPx()), Offset(size.width / 2 + 6.dp.toPx(), y + 4.dp.toPx()), strokeWidth = stroke)
                }
            }
        },
    )
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun AyahBlock(ayah: Int, words: List<UiItem>) {
    FlowRow(
        Modifier.fillMaxWidth().padding(vertical = 2.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalArrangement = Arrangement.spacedBy(0.dp),
        itemVerticalAlignment = Alignment.CenterVertically,
    ) {
        if (ayah > 0 && words.isNotEmpty()) {
            words.dropLast(1).forEach { WordText(it) }
            // the ring stays with the last word instead of wrapping onto a line of its own
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                WordText(words.last())
                AyahMarker(ayah)
            }
        } else {
            words.forEach { WordText(it) }
        }
    }
}

/** The ayah number in a ring (the ornamental brackets of the Quran fonts are missing from the system font). */
@Composable
private fun AyahMarker(ayah: Int) {
    Box(
        Modifier.padding(horizontal = 2.dp).size(34.dp).border(1.5.dp, MaterialTheme.colorScheme.primary, CircleShape),
        contentAlignment = Alignment.Center,
    ) {
        Text(arabicDigits(ayah), fontSize = if (ayah > 99) 13.sp else 16.sp, color = MaterialTheme.colorScheme.primary)
    }
}

/** One item with its mark, drawn just under the letters (shapes, not colors, tell the problems apart). */
@Composable
private fun WordText(item: UiItem) {
    val attention = LocalAttention.current
    val accent = MaterialTheme.colorScheme.primary
    var baseline by remember { mutableFloatStateOf(0f) }
    val modifier = Modifier
        .then(if (item.state == ItemState.Current) Modifier.clip(RoundedCornerShape(10.dp)).background(accent.copy(alpha = 0.18f)) else Modifier)
        .padding(horizontal = 3.dp)
        .drawBehind {
            val y = (baseline + 9.dp.toPx()).coerceAtMost(size.height - 1.dp.toPx())
            val stroke = 2.dp.toPx()
            when {
                item.state == ItemState.Wrong ->
                    drawLine(attention, Offset(0f, y), Offset(size.width, y), strokeWidth = stroke)
                item.state == ItemState.Skipped ->
                    drawLine(attention, Offset(0f, y), Offset(size.width, y), strokeWidth = stroke, pathEffect = PathEffect.dashPathEffect(floatArrayOf(8f, 7f)))
                item.repeated -> {
                    drawLine(attention, Offset(0f, y), Offset(size.width, y), strokeWidth = stroke / 1.5f)
                    drawLine(attention, Offset(0f, y + 5.dp.toPx()), Offset(size.width, y + 5.dp.toPx()), strokeWidth = stroke / 1.5f)
                }
            }
        }
    Text(
        item.text, modifier,
        fontSize = 30.sp, lineHeight = 50.sp,
        color = MaterialTheme.colorScheme.onBackground,
        onTextLayout = { baseline = it.getLineBaseline(0) },
    )
    if (item.addedAfter) Text("‸", fontSize = 30.sp, color = attention) // a small caret between the words
}

private fun arabicDigits(n: Int): String =
    n.toString().map { '٠' + (it - '0') }.joinToString("")
