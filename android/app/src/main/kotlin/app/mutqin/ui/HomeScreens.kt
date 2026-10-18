package app.mutqin.ui

import android.content.ClipData
import androidx.activity.compose.BackHandler
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.List
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.ClipEntry
import androidx.compose.ui.platform.LocalClipboard
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringArrayResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.mutqin.AppLanguage
import app.mutqin.R
import app.mutqin.core.Strictness
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import java.text.DateFormat
import java.util.Date
import java.util.Locale

internal enum class SetupPage { Home, Preferences, History }

/** The mark of the app: a closed ring (itqan, mastery) around three sound bars, in a thin gold outer ring. */
@Composable
internal fun MutqinMark(
    size: Dp,
    modifier: Modifier = Modifier,
) {
    val ink = MaterialTheme.colorScheme.primary
    val gold = LocalGold.current
    Canvas(modifier.size(size)) {
        val u = this.size.minDimension / 1024f // the icon is designed on a 1024 grid (docs/branding/icon.svg)
        val c = Offset(512f * u, 512f * u)
        drawCircle(gold.copy(alpha = 0.7f), radius = 470f * u, center = c, style = Stroke(width = 10f * u))
        drawCircle(gold.copy(alpha = 0.35f), radius = 436f * u, center = c, style = Stroke(width = 6f * u))
        drawCircle(ink, radius = 330f * u, center = c, style = Stroke(width = 46f * u))
        for ((x, y1, y2) in listOf(Triple(420f, 452f, 572f), Triple(512f, 382f, 642f), Triple(604f, 432f, 592f))) {
            drawLine(ink, Offset(x * u, y1 * u), Offset(x * u, y2 * u), strokeWidth = 46f * u, cap = StrokeCap.Round)
        }
    }
}

/** The mark, softly pulsing: shown while the model loads or a session starts (no text, no spinner). */
@Composable
internal fun LogoLoader() {
    val pulse = rememberInfiniteTransition(label = "logo")
    val alpha by pulse.animateFloat(
        initialValue = 0.5f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(1000), RepeatMode.Reverse),
        label = "logo alpha",
    )
    MutqinMark(132.dp, Modifier.alpha(alpha))
}

@Composable
internal fun Centered(content: @Composable () -> Unit) {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) { content() }
}

/** The exact error, selectable, with a button to copy it (for a bug report). */
@Composable
internal fun ErrorScreen(
    error: String,
    onRetry: () -> Unit,
) {
    val clipboard = LocalClipboard.current
    val scope = rememberCoroutineScope()
    var copied by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxSize().padding(vertical = 24.dp)) {
        Eyebrow(stringResource(R.string.app_name), color = LocalAttention.current)
        Text(stringResource(R.string.error_quran_title), style = MaterialTheme.typography.headlineSmall, color = LocalAttention.current)
        Spacer(Modifier.height(8.dp))
        Text(stringResource(R.string.error_quran_body), color = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.height(14.dp))
        PaperCard(Modifier.weight(1f, fill = false)) {
            SelectionContainer {
                Text(error, Modifier.verticalScroll(rememberScrollState()), fontFamily = FontFamily.Monospace, fontSize = 13.sp, lineHeight = 19.sp)
            }
        }
        Spacer(Modifier.weight(1f))
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            SecondaryButton(
                stringResource(if (copied) R.string.error_copied else R.string.error_copy),
                onClick = {
                    scope.launch { clipboard.setClipEntry(ClipEntry(ClipData.newPlainText("error", error))) }
                    copied = true
                },
                modifier = Modifier.weight(1f),
            )
            PrimaryButton(stringResource(R.string.retry_check), onRetry, Modifier.weight(1f))
        }
    }
}

@Composable
internal fun NeedModelScreen(
    state: UiState,
    onCheck: () -> Unit,
) {
    Column(Modifier.fillMaxSize().padding(vertical = 24.dp), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
        MutqinMark(84.dp)
        Spacer(Modifier.height(20.dp))
        Text(stringResource(R.string.need_model_title), style = MaterialTheme.typography.headlineSmall, textAlign = TextAlign.Center)
        Spacer(Modifier.height(10.dp))
        OrnamentRule(Modifier.width(120.dp))
        Spacer(Modifier.height(10.dp))
        Text(stringResource(R.string.need_model_body), textAlign = TextAlign.Center, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.height(16.dp))
        PaperCard {
            Text(state.modelFolder, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.height(8.dp))
            Text(stringResource(R.string.need_model_files), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Spacer(Modifier.height(24.dp))
        PrimaryButton(stringResource(R.string.retry_check), onCheck, Modifier.fillMaxWidth())
    }
}

/** A number in a thin gold ring, the way verses and chapters are numbered in a mushaf. */
@Composable
internal fun NumberRing(
    number: Int,
    size: Dp = 40.dp,
    filled: Boolean = false,
) {
    val gold = LocalGold.current
    Box(
        Modifier
            .size(size)
            .clip(CircleShape)
            .background(if (filled) gold.copy(alpha = 0.14f) else Color.Transparent)
            .border(1.5.dp, gold, CircleShape),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            String.format(Locale.US, "%d", number),
            fontFamily = FontFamily.Serif,
            fontWeight = FontWeight.SemiBold,
            fontSize = if (number > 99) 12.sp else 15.sp,
            color = MaterialTheme.colorScheme.onBackground,
        )
    }
}

/** − value + in a bordered capsule; holding a button repeats, and the number can be typed. */
@Composable
private fun Stepper(
    label: String,
    value: Int,
    max: Int,
    canMinus: Boolean,
    canPlus: Boolean,
    onMinus: () -> Unit,
    onPlus: () -> Unit,
    onSet: (Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    val focus = LocalFocusManager.current
    var text by remember(value) { mutableStateOf(value.toString()) }
    var focused by remember { mutableStateOf(false) }
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        Text(label, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.height(6.dp))
        Row(
            Modifier
                .clip(
                    RoundedCornerShape(16.dp),
                ).border(1.dp, if (focused) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(16.dp)),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            StepKey("−", canMinus, onMinus)
            BasicTextField(
                value = text,
                onValueChange = { typed ->
                    val digits = typed.filter { it.isDigit() }.take(3)
                    text = digits
                    // what is typed counts at once (past the end of the surah it is the last ayah); an empty field waits
                    digits.toIntOrNull()?.let { n -> onSet(n.coerceIn(1, max)) }
                },
                modifier =
                    Modifier
                        .width(56.dp)
                        .onFocusChanged {
                            focused = it.isFocused
                            if (!it.isFocused) text = value.toString()
                        },
                textStyle = MaterialTheme.typography.titleLarge.copy(color = MaterialTheme.colorScheme.onBackground, textAlign = TextAlign.Center),
                singleLine = true,
                cursorBrush = SolidColor(MaterialTheme.colorScheme.primary),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number, imeAction = ImeAction.Done),
                keyboardActions = KeyboardActions(onDone = { focus.clearFocus() }),
            )
            StepKey("+", canPlus, onPlus)
        }
    }
}

/** A − or + key; holding it repeats, so an ayah far away can be reached quickly. */
@Composable
private fun StepKey(
    label: String,
    enabled: Boolean,
    onStep: () -> Unit,
) {
    val step by rememberUpdatedState(onStep)
    Box(
        Modifier
            .size(46.dp)
            .background(MaterialTheme.colorScheme.secondaryContainer.copy(alpha = if (enabled) 1f else 0.35f))
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
        Text(
            label,
            fontSize = 22.sp,
            color = if (enabled) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.5f),
        )
    }
}

@Composable
internal fun SetupScreen(
    state: UiState,
    vm: MainViewModel,
    onOpenPreferences: () -> Unit,
    onOpenHistory: () -> Unit,
    onStart: (auto: Boolean) -> Unit,
) {
    val surahNames = stringArrayResource(R.array.surah_names)
    var picking by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(top = 10.dp), verticalAlignment = Alignment.CenterVertically) {
            RoundAction(onOpenHistory) {
                Icon(
                    Icons.AutoMirrored.Filled.List,
                    stringResource(R.string.history),
                    tint = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.size(22.dp),
                )
            }
            Spacer(Modifier.weight(1f))
            RoundAction(onOpenPreferences) {
                Icon(Icons.Default.Settings, stringResource(R.string.preferences), tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(22.dp))
            }
        }
        // the title page: whatever height the cards below leave
        BoxWithConstraints(Modifier.weight(1f).fillMaxWidth()) {
            val mark = minOf(124.dp, maxOf(56.dp, maxHeight - 120.dp))
            Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
                MutqinMark(mark)
                Spacer(Modifier.height(12.dp))
                Text(stringResource(R.string.app_name), style = MaterialTheme.typography.displaySmall)
                Spacer(Modifier.height(4.dp))
                Eyebrow(stringResource(R.string.app_tagline))
            }
        }
        OrnamentRule(Modifier.padding(vertical = 6.dp))
        Spacer(Modifier.height(8.dp))
        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            // the surah, set like the banner that opens a chapter
            PaperCard(Modifier.clip(RoundedCornerShape(18.dp)).clickable { picking = true }) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    NumberRing(state.surah, 46.dp, filled = true)
                    Spacer(Modifier.width(14.dp))
                    Column(Modifier.weight(1f)) {
                        Eyebrow(stringResource(R.string.surah))
                        Text(surahNames[state.surah - 1], style = MaterialTheme.typography.titleLarge)
                    }
                    Column(horizontalAlignment = Alignment.End) {
                        Text(
                            stringResource(R.string.ayahs_in_surah, state.ayahCount),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Text("⌄", color = LocalGold.current, fontSize = 20.sp)
                    }
                }
            }

            PaperCard {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Eyebrow(stringResource(R.string.practice_passage))
                        Text(stringResource(R.string.range_summary, state.fromAyah, state.toAyah), style = MaterialTheme.typography.titleMedium)
                    }
                    TextButton(onClick = {
                        vm.setFrom(1)
                        vm.setTo(state.ayahCount)
                    }) { Text(stringResource(R.string.whole_surah), fontWeight = FontWeight.SemiBold) }
                }
                Spacer(Modifier.height(10.dp))
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                    Stepper(
                        stringResource(R.string.ayah_from),
                        state.fromAyah,
                        state.ayahCount,
                        state.fromAyah > 1,
                        state.fromAyah < state.ayahCount,
                        { vm.setFrom(state.fromAyah - 1) },
                        { vm.setFrom(state.fromAyah + 1) },
                        vm::setFrom,
                    )
                    Stepper(
                        stringResource(R.string.ayah_to),
                        state.toAyah,
                        state.ayahCount,
                        state.toAyah > 1,
                        state.toAyah < state.ayahCount,
                        { vm.setTo(state.toAyah - 1) },
                        { vm.setTo(state.toAyah + 1) },
                        vm::setTo,
                    )
                }
                Spacer(Modifier.height(12.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(R.string.memorization_mode), style = MaterialTheme.typography.titleSmall)
                        Text(
                            stringResource(R.string.memorization_hint),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Switch(
                        checked = state.memorization,
                        onCheckedChange = vm::setMemorization,
                        colors =
                            SwitchDefaults.colors(
                                checkedTrackColor = MaterialTheme.colorScheme.primary,
                                checkedThumbColor = MaterialTheme.colorScheme.onPrimary,
                                uncheckedTrackColor = MaterialTheme.colorScheme.background,
                                uncheckedThumbColor = MaterialTheme.colorScheme.outline,
                                uncheckedBorderColor = MaterialTheme.colorScheme.outlineVariant,
                            ),
                    )
                }
            }

            state.message?.let {
                PaperCard {
                    if (it == "range") {
                        Text(stringResource(R.string.range_too_long), color = LocalAttention.current)
                    } else if (it == "not_found") {
                        Text(stringResource(R.string.detect_not_found), color = LocalAttention.current)
                    } else {
                        SelectionContainer {
                            Text(it, color = LocalAttention.current, fontFamily = FontFamily.Monospace, fontSize = 13.sp, lineHeight = 19.sp)
                        }
                    }
                }
            }
            val forward = if (LocalLayoutDirection.current == LayoutDirection.Rtl) "←" else "→"
            PrimaryButton(stringResource(R.string.start), { onStart(false) }, Modifier.fillMaxWidth(), trailing = forward)
            SecondaryButton(stringResource(R.string.detect_auto), { onStart(true) }, Modifier.fillMaxWidth())
            Spacer(Modifier.height(6.dp))
        }
    }

    if (picking) {
        SurahSheet(surahNames, state.surah, onPick = {
            vm.setSurah(it)
            picking = false
        }, onDismiss = { picking = false })
    }
}

/** The list of surahs in a bottom sheet; it opens at the chosen one. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SurahSheet(
    names: Array<String>,
    selected: Int,
    onPick: (Int) -> Unit,
    onDismiss: () -> Unit,
) {
    val gold = LocalGold.current
    val list = rememberLazyListState(initialFirstVisibleItemIndex = maxOf(0, selected - 3))
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        containerColor = MaterialTheme.colorScheme.background,
        shape = RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp),
    ) {
        Column(Modifier.padding(horizontal = 24.dp)) {
            Eyebrow(stringResource(R.string.surah))
            Text(stringResource(R.string.choose_surah), style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.height(8.dp))
            OrnamentRule()
        }
        LazyColumn(Modifier.fillMaxWidth(), state = list, contentPadding = PaddingValues(horizontal = 12.dp, vertical = 8.dp)) {
            itemsIndexed(names.toList()) { i, name ->
                val chosen = i + 1 == selected
                Row(
                    Modifier
                        .fillMaxWidth()
                        .padding(vertical = 2.dp)
                        .clip(RoundedCornerShape(14.dp))
                        .background(if (chosen) gold.copy(alpha = 0.14f) else Color.Transparent)
                        .clickable { onPick(i + 1) }
                        .padding(horizontal = 12.dp, vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    NumberRing(i + 1, 38.dp, filled = chosen)
                    Spacer(Modifier.width(14.dp))
                    Text(
                        name,
                        Modifier.weight(1f),
                        style = MaterialTheme.typography.titleMedium,
                        color = if (chosen) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                    )
                    if (chosen) Text("✓", color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
                }
            }
        }
    }
}

@Composable
private fun BackArrow() {
    Icon(Icons.AutoMirrored.Filled.ArrowBack, stringResource(R.string.back), tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(22.dp))
}

/** The attempts kept on this phone, newest first. */
@Composable
internal fun HistoryScreen(
    state: UiState,
    vm: MainViewModel,
    onClose: () -> Unit,
) {
    BackHandler(onBack = onClose)
    val surahNames = stringArrayResource(R.array.surah_names)
    var confirming by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxSize()) {
        HeaderBar(stringResource(R.string.app_name), stringResource(R.string.history), onClose, backIcon = { BackArrow() })
        OrnamentRule(Modifier.padding(bottom = 12.dp))
        if (state.history.isEmpty()) {
            Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    MutqinMark(72.dp, Modifier.alpha(0.5f))
                    Spacer(Modifier.height(16.dp))
                    Text(
                        stringResource(R.string.history_empty),
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        textAlign = TextAlign.Center,
                        modifier = Modifier.padding(horizontal = 32.dp),
                    )
                }
            }
        } else {
            val format = remember { DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT) }
            LazyColumn(Modifier.weight(1f).fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                itemsIndexed(state.history) { _, entry ->
                    PaperCard {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            NumberRing(entry.surah, 42.dp, filled = true)
                            Spacer(Modifier.width(14.dp))
                            Column(Modifier.weight(1f)) {
                                Text(surahNames.getOrElse(entry.surah - 1) { "" }, style = MaterialTheme.typography.titleMedium)
                                Text(
                                    stringResource(R.string.range_summary, entry.fromAyah, entry.toAyah),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            Text("%d:%02d".format(entry.seconds / 60, entry.seconds % 60), color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                        Spacer(Modifier.height(10.dp))
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            val clean = entry.differences == 0
                            val tone = if (clean) MaterialTheme.colorScheme.primary else LocalAttention.current
                            Text(
                                if (clean) {
                                    stringResource(
                                        R.string.history_clean,
                                    )
                                } else {
                                    pluralStringResource(R.plurals.history_differences, entry.differences, entry.differences)
                                },
                                Modifier.clip(RoundedCornerShape(50)).background(tone.copy(alpha = 0.12f)).padding(horizontal = 12.dp, vertical = 4.dp),
                                color = tone,
                                style = MaterialTheme.typography.labelLarge,
                            )
                            if (entry.memorization) {
                                Text(
                                    "  ·  " + stringResource(R.string.history_memorization),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            Spacer(Modifier.weight(1f))
                            Text(
                                format.format(Date(entry.time)),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }
            }
            TextButton(onClick = { confirming = true }, Modifier.align(Alignment.CenterHorizontally)) {
                Text(stringResource(R.string.history_clear), color = LocalAttention.current)
            }
        }
        Spacer(Modifier.height(8.dp))
    }
    if (confirming) {
        AlertDialog(
            onDismissRequest = { confirming = false },
            containerColor = MaterialTheme.colorScheme.background,
            title = { Text(stringResource(R.string.history_clear_title), style = MaterialTheme.typography.titleLarge) },
            text = { Text(stringResource(R.string.history_clear_body)) },
            confirmButton = {
                TextButton(onClick = {
                    vm.clearHistory()
                    confirming = false
                }) { Text(stringResource(R.string.history_clear_confirm), color = LocalAttention.current) }
            },
            dismissButton = { TextButton(onClick = { confirming = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

/** A small picture of a page in one of the themes, for choosing between them. */
@Composable
private fun ThemeTile(
    mode: ThemeMode,
    label: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val primary = MaterialTheme.colorScheme.primary
    val shape = RoundedCornerShape(16.dp)
    Column(
        modifier
            .clip(shape)
            .border(if (selected) 2.dp else 1.dp, if (selected) primary else MaterialTheme.colorScheme.outlineVariant, shape)
            .clickable(onClick = onClick)
            .padding(10.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Canvas(Modifier.fillMaxWidth().height(54.dp).clip(RoundedCornerShape(10.dp))) {
            val light = Color(0xFFFFFFFF)
            val night = Color(0xFF0A1410)

            fun page(
                left: Float,
                right: Float,
                paper: Color,
                ink: Color,
                green: Color,
            ) {
                clipRect(left, 0f, right, size.height) {
                    drawRect(
                        paper,
                        Offset(left, 0f),
                        androidx.compose.ui.geometry
                            .Size(right - left, size.height),
                    )
                    val w = size.width
                    drawRoundRect(
                        ink.copy(alpha = 0.55f),
                        Offset(w * 0.12f, size.height * 0.2f),
                        androidx.compose.ui.geometry
                            .Size(w * 0.76f, 3.dp.toPx()),
                        androidx.compose.ui.geometry
                            .CornerRadius(2.dp.toPx()),
                    )
                    drawRoundRect(
                        ink.copy(alpha = 0.35f),
                        Offset(w * 0.12f, size.height * 0.4f),
                        androidx.compose.ui.geometry
                            .Size(w * 0.5f, 3.dp.toPx()),
                        androidx.compose.ui.geometry
                            .CornerRadius(2.dp.toPx()),
                    )
                    drawCircle(green, 5.dp.toPx(), Offset(w * 0.2f, size.height * 0.72f))
                    drawRoundRect(
                        Color(0xFF3FA57A),
                        Offset(w * 0.34f, size.height * 0.72f - 1.5.dp.toPx()),
                        androidx.compose.ui.geometry
                            .Size(w * 0.4f, 3.dp.toPx()),
                        androidx.compose.ui.geometry
                            .CornerRadius(2.dp.toPx()),
                    )
                }
            }
            when (mode) {
                ThemeMode.Light -> {
                    page(0f, size.width, light, Color(0xFF10281E), Color(0xFF17694A))
                }

                ThemeMode.Dark -> {
                    page(0f, size.width, night, Color(0xFFE3F2EA), Color(0xFF6FD3A4))
                }

                ThemeMode.System -> {
                    page(0f, size.width / 2, light, Color(0xFF10281E), Color(0xFF17694A))
                    page(size.width / 2, size.width, night, Color(0xFFE3F2EA), Color(0xFF6FD3A4))
                }
            }
        }
        Spacer(Modifier.height(8.dp))
        Text(label, style = MaterialTheme.typography.labelLarge, color = if (selected) primary else MaterialTheme.colorScheme.onBackground)
    }
}

/** Three small bars that fill with the level of feedback (0, 1 or 2). */
@Composable
private fun LevelMeter(
    level: Int,
    selected: Boolean,
) {
    val on = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant
    Row(Modifier.height(26.dp), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.Bottom) {
        for (i in 0..2) {
            Box(
                Modifier
                    .width(9.dp)
                    .height((10 + 7 * i).dp)
                    .clip(RoundedCornerShape(3.dp))
                    .background(if (i <= level) on else MaterialTheme.colorScheme.outlineVariant),
            )
        }
    }
}

/** A choice as a bordered tile with a small picture over its label; the chosen one has a green frame. */
@Composable
private fun ChoiceTile(
    label: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    picture: @Composable () -> Unit,
) {
    val primary = MaterialTheme.colorScheme.primary
    val shape = RoundedCornerShape(16.dp)
    Column(
        modifier
            .clip(shape)
            .background(if (selected) primary.copy(alpha = 0.08f) else Color.Transparent)
            .border(if (selected) 2.dp else 1.dp, if (selected) primary else MaterialTheme.colorScheme.outlineVariant, shape)
            .clickable(onClick = onClick)
            .padding(vertical = 14.dp, horizontal = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        picture()
        Spacer(Modifier.height(10.dp))
        Text(label, style = MaterialTheme.typography.labelLarge, color = if (selected) primary else MaterialTheme.colorScheme.onBackground)
    }
}

/** The chosen language, in its own name, as a field that opens the list of languages. */
@Composable
private fun LanguageField(
    language: AppLanguage,
    onClick: () -> Unit,
) {
    val shape = RoundedCornerShape(16.dp)
    Row(
        Modifier
            .fillMaxWidth()
            .clip(shape)
            .border(1.dp, MaterialTheme.colorScheme.outlineVariant, shape)
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(language.nativeName, style = MaterialTheme.typography.titleMedium)
            if (language.englishName != language.nativeName) {
                Text(language.englishName, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        Text("⌄", color = LocalGold.current, fontSize = 20.sp)
    }
}

/**
 * Every language of the app, each in its own name (and in English), with a search field once the list is long.
 * The list comes from AppLanguage.entries, so a new language appears here by itself.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun LanguageSheet(
    selected: AppLanguage,
    onPick: (AppLanguage) -> Unit,
    onDismiss: () -> Unit,
) {
    var query by remember { mutableStateOf("") }
    val shown =
        remember(query) {
            val q = query.trim().lowercase()
            AppLanguage.entries.filter {
                q.isEmpty() ||
                    it.nativeName.lowercase().contains(
                        q,
                    ) || it.englishName.lowercase().contains(q) || it.tag.startsWith(q)
            }
        }
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        containerColor = MaterialTheme.colorScheme.background,
        shape = RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp),
    ) {
        Column(Modifier.padding(horizontal = 24.dp)) {
            Eyebrow(stringResource(R.string.language))
            Text(stringResource(R.string.choose_language), style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.height(8.dp))
            OrnamentRule()
            if (AppLanguage.entries.size > 8) {
                Spacer(Modifier.height(10.dp))
                BasicTextField(
                    value = query,
                    onValueChange = { query = it },
                    singleLine = true,
                    textStyle = MaterialTheme.typography.bodyLarge.copy(color = MaterialTheme.colorScheme.onBackground),
                    cursorBrush = SolidColor(MaterialTheme.colorScheme.primary),
                    decorationBox = { inner ->
                        Box(
                            Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(14.dp))
                                .border(1.dp, MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(14.dp))
                                .padding(horizontal = 14.dp, vertical = 12.dp),
                        ) {
                            if (query.isEmpty()) Text(stringResource(R.string.search), color = MaterialTheme.colorScheme.onSurfaceVariant)
                            inner()
                        }
                    },
                )
            }
        }
        LazyColumn(Modifier.fillMaxWidth(), contentPadding = PaddingValues(horizontal = 12.dp, vertical = 8.dp)) {
            itemsIndexed(shown) { _, language ->
                val chosen = language == selected
                Row(
                    Modifier
                        .fillMaxWidth()
                        .padding(vertical = 2.dp)
                        .clip(RoundedCornerShape(14.dp))
                        .background(if (chosen) LocalGold.current.copy(alpha = 0.14f) else Color.Transparent)
                        .clickable { onPick(language) }
                        .padding(horizontal = 14.dp, vertical = 12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(
                            language.nativeName,
                            style = MaterialTheme.typography.titleMedium,
                            color = if (chosen) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                        )
                        if (language.englishName != language.nativeName) {
                            Text(language.englishName, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                    if (chosen) Text("✓", color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
                }
            }
        }
        Spacer(Modifier.height(12.dp))
    }
}

/**
 * Appearance, language and feedback level, and what the app is. The theme and the language show at once on this
 * screen; nothing is kept until "Save" (going back puts the theme back as it was).
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun PreferencesScreen(
    state: UiState,
    vm: MainViewModel,
    language: AppLanguage,
    appearance: Appearance,
    onLanguageSelected: (AppLanguage) -> Unit,
    onClose: () -> Unit,
) {
    var draftLanguage by rememberSaveable { mutableStateOf(language) }
    var draftLevel by rememberSaveable { mutableIntStateOf(state.strictness) }
    var draftTheme by rememberSaveable { mutableStateOf(appearance.mode) }
    var choosingLanguage by remember { mutableStateOf(false) }
    val discard = {
        appearance.preview(appearance.saved)
        onClose()
    }
    BackHandler(onBack = discard)
    // the screen shows the chosen language at once, before anything is saved
    val base = LocalContext.current
    val current = LocalConfiguration.current
    val preview =
        remember(draftLanguage, base, current) {
            val configuration = android.content.res.Configuration(current)
            configuration.setLocale(Locale.forLanguageTag(draftLanguage.tag))
            base.createConfigurationContext(configuration)
        }
    CompositionLocalProvider(
        LocalContext provides preview,
        LocalLayoutDirection provides if (draftLanguage.rtl) LayoutDirection.Rtl else LayoutDirection.Ltr,
    ) {
        Column(Modifier.fillMaxSize()) {
            HeaderBar(stringResource(R.string.app_name), stringResource(R.string.preferences), discard, backIcon = { BackArrow() })
            OrnamentRule(Modifier.padding(bottom = 14.dp))
            Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                PaperCard {
                    Eyebrow(stringResource(R.string.appearance))
                    Spacer(Modifier.height(4.dp))
                    Text(stringResource(R.string.theme_hint), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Spacer(Modifier.height(14.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        listOf(
                            ThemeMode.System to R.string.theme_system,
                            ThemeMode.Light to R.string.theme_light,
                            ThemeMode.Dark to R.string.theme_dark,
                        ).forEach { (mode, label) ->
                            ThemeTile(
                                mode,
                                stringResource(label),
                                selected = draftTheme == mode,
                                onClick = {
                                    draftTheme = mode
                                    appearance.preview(mode)
                                },
                                modifier = Modifier.weight(1f),
                            )
                        }
                    }
                }
                PaperCard {
                    Eyebrow(stringResource(R.string.language))
                    Spacer(Modifier.height(4.dp))
                    Text(
                        stringResource(R.string.preferences_language_hint),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(Modifier.height(14.dp))
                    LanguageField(draftLanguage) { choosingLanguage = true }
                }
                PaperCard {
                    Eyebrow(stringResource(R.string.strictness))
                    Spacer(Modifier.height(14.dp))
                    val levels =
                        listOf(
                            Triple(Strictness.RELAXED, R.string.strictness_relaxed, R.string.strictness_relaxed_hint),
                            Triple(Strictness.STANDARD, R.string.strictness_standard, R.string.strictness_standard_hint),
                            Triple(Strictness.STRICT, R.string.strictness_strict, R.string.strictness_strict_hint),
                        )
                    // separate tiles, not a joined segmented control: they follow the layout direction by themselves
                    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        levels.forEach { (level, label, _) ->
                            ChoiceTile(
                                stringResource(label),
                                selected = draftLevel == level,
                                onClick = { draftLevel = level },
                                modifier = Modifier.weight(1f),
                            ) { LevelMeter(level, selected = draftLevel == level) }
                        }
                    }
                    Spacer(Modifier.height(12.dp))
                    Text(
                        stringResource(levels.first { it.first == draftLevel }.third),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                PaperCard {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        MutqinMark(44.dp)
                        Spacer(Modifier.width(14.dp))
                        Column {
                            Text(stringResource(R.string.app_name), style = MaterialTheme.typography.titleLarge)
                            Text(
                                stringResource(R.string.about_version, app.mutqin.BuildConfig.VERSION_NAME),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                    Spacer(Modifier.height(12.dp))
                    Text(stringResource(R.string.about_privacy), style = MaterialTheme.typography.bodyMedium)
                    Spacer(Modifier.height(6.dp))
                    Text(stringResource(R.string.about_free), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Spacer(Modifier.height(6.dp))
                    Text(stringResource(R.string.about_font), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                Spacer(Modifier.height(2.dp))
            }
            Spacer(Modifier.height(12.dp))
            PrimaryButton(
                stringResource(R.string.save),
                {
                    appearance.save(draftTheme)
                    vm.setStrictness(draftLevel)
                    onClose()
                    onLanguageSelected(draftLanguage) // recreates the screen when the language changed
                },
                Modifier.fillMaxWidth(),
            )
            Spacer(Modifier.height(8.dp))
        }
        if (choosingLanguage) {
            LanguageSheet(draftLanguage, onPick = {
                draftLanguage = it
                choosingLanguage = false
            }, onDismiss = { choosingLanguage = false })
        }
    }
}
