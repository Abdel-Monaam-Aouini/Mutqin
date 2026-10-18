package app.mutqin.ui

import android.content.res.Configuration
import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.animateScrollBy
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.PaddingValues
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
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.List
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateMapOf
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
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInRoot
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringArrayResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import app.mutqin.AppLanguage
import app.mutqin.HistoryEntry
import app.mutqin.R
import app.mutqin.core.EventKind
import app.mutqin.core.Strictness
import app.mutqin.core.TajweedRule
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import java.text.DateFormat
import java.util.Date
import java.util.Locale

/** The screen's root: a surface in the theme colors that keeps clear of the status and navigation bars. */
@Composable
fun MutqinApp(
    state: UiState,
    vm: MainViewModel,
    language: AppLanguage,
    appearance: Appearance,
    onLanguageSelected: (AppLanguage) -> Unit,
    onStart: (auto: Boolean) -> Unit,
) {
    val layoutDirection = if (language.rtl) LayoutDirection.Rtl else LayoutDirection.Ltr
    CompositionLocalProvider(LocalLayoutDirection provides layoutDirection) {
        Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background, contentColor = MaterialTheme.colorScheme.onBackground) {
            Box(Modifier.fillMaxSize().safeDrawingPadding().padding(horizontal = 20.dp)) {
                val screen =
                    when (state.phase) {
                        Phase.Loading, Phase.Starting -> 0
                        Phase.NeedModel -> 1
                        Phase.Error -> 2
                        Phase.Setup -> 3
                        Phase.Reciting, Phase.Result -> 4
                    }
                AnimatedContent(
                    targetState = screen,
                    modifier = Modifier.fillMaxSize(),
                    transitionSpec = { fadeIn(tween(260)) togetherWith fadeOut(tween(160)) },
                    label = "screen",
                ) { target ->
                    key(target) {
                        when (state.phase) {
                            Phase.Loading -> {
                                Centered { LogoLoader() }
                            }

                            Phase.NeedModel -> {
                                NeedModelScreen(state, onCheck = vm::prepare)
                            }

                            Phase.Error -> {
                                ErrorScreen(state.error.orEmpty(), onRetry = vm::prepare)
                            }

                            Phase.Setup -> {
                                var sub by rememberSaveable { mutableStateOf(SetupPage.Home) }
                                AnimatedContent(
                                    targetState = sub,
                                    modifier = Modifier.fillMaxSize(),
                                    transitionSpec = {
                                        val forward = targetState != SetupPage.Home
                                        (fadeIn(tween(240)) + slideInHorizontally(tween(260)) { if (forward) it / 10 else -it / 10 }) togetherWith
                                            (fadeOut(tween(140)) + slideOutHorizontally(tween(260)) { if (forward) -it / 10 else it / 10 })
                                    },
                                    label = "page",
                                ) { page ->
                                    when (page) {
                                        SetupPage.Preferences -> {
                                            PreferencesScreen(state, vm, language, appearance, onLanguageSelected) { sub = SetupPage.Home }
                                        }

                                        SetupPage.History -> {
                                            HistoryScreen(state, vm) { sub = SetupPage.Home }
                                        }

                                        SetupPage.Home -> {
                                            SetupScreen(
                                                state,
                                                vm,
                                                onOpenPreferences = { sub = SetupPage.Preferences },
                                                onOpenHistory = {
                                                    vm.loadHistory()
                                                    sub = SetupPage.History
                                                },
                                                onStart = onStart,
                                            )
                                        }
                                    }
                                }
                            }

                            Phase.Starting -> {
                                Centered { LogoLoader() }
                            }

                            Phase.Reciting, Phase.Result -> {
                                ReciteScreen(state, vm)
                            }
                        }
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------- setup

// ----------------------------------------------------------------------------------------- recitation

@Composable
private fun ReciteScreen(
    state: UiState,
    vm: MainViewModel,
) {
    val groups = remember(state.items) { state.items.groupBy { it.ayah }.toList() }
    val listState = rememberLazyListState()
    // the ayahs run on one after the other, as in a mushaf; they are cut into pages of a few ayahs for the list
    val pages =
        remember(groups) {
            val (basmala, ayahs) = groups.partition { it.first == 0 }
            (if (basmala.isEmpty()) emptyList() else listOf(basmala)) + ayahs.chunked(AYAHS_PER_PAGE)
        }
    val currentPage = pages.indexOfFirst { page -> page.any { (_, words) -> words.any { it.state == ItemState.Current } } }
    // item 0 of the list is the surah banner
    LaunchedEffect(currentPage) { if (currentPage >= 0) listState.animateScrollToItem(currentPage + 1) }
    val reciting = state.phase == Phase.Reciting
    val scope = rememberCoroutineScope()
    // where the flagged words are on the screen, to scroll to one exactly (their block of ayahs can be long)
    val wordTops = remember { mutableStateMapOf<Int, Float>() }
    var listTop by remember { mutableFloatStateOf(0f) }
    var showAll by remember { mutableStateOf(false) }
    val flagged = remember(state.items) { state.items.filter { it.state == ItemState.Wrong || it.state == ItemState.Skipped || it.repeated || it.addedAfter } }
    val goTo: (UiItem) -> Unit = { item ->
        val page = pages.indexOfFirst { p -> p.any { it.first == item.ayah } }
        scope.launch {
            if (page >= 0) {
                val visible = listState.layoutInfo.visibleItemsInfo.any { it.index == page + 1 }
                if (!visible) {
                    listState.scrollToItem(page + 1)
                    delay(120)
                }
                wordTops[item.index]?.let { listState.animateScrollBy(it - listTop - 120f) }
            }
        }
        if (item.state == ItemState.Wrong || item.state == ItemState.Skipped) vm.showDetail(item)
    }

    Column(Modifier.fillMaxSize()) {
        TopBar(state, reciting, onClose = vm::back)
        if (state.memorization && reciting) {
            // the grey bars are deliberate: say so, and let the text be shown with one tap
            Row(
                Modifier
                    .fillMaxWidth()
                    .padding(bottom = 10.dp)
                    .clip(RoundedCornerShape(14.dp))
                    .background(LocalGold.current.copy(alpha = 0.12f))
                    .border(1.dp, LocalGold.current.copy(alpha = 0.5f), RoundedCornerShape(14.dp))
                    .padding(start = 14.dp, end = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(stringResource(R.string.memorization_banner), Modifier.weight(1f), style = MaterialTheme.typography.bodySmall)
                TextButton(onClick = { vm.setMemorization(false) }) { Text(stringResource(R.string.memorization_show), fontWeight = FontWeight.SemiBold) }
            }
        }
        if (state.auto && state.items.isEmpty()) {
            Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                Column(horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.spacedBy(16.dp)) {
                    LogoLoader()
                    Text(
                        stringResource(R.string.detect_listening),
                        textAlign = TextAlign.Center,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(horizontal = 24.dp),
                    )
                }
            }
        } else {
            // the text is set on a page: paper, a thin border and a gold line inside it, like a page of a mushaf
            val gold = LocalGold.current
            val frame = RoundedCornerShape(20.dp)
            Box(
                Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .clip(frame)
                    .background(MaterialTheme.colorScheme.surfaceVariant)
                    .border(1.dp, MaterialTheme.colorScheme.outlineVariant, frame)
                    .drawBehind {
                        val inset = 6.dp.toPx()
                        drawRoundRect(
                            gold.copy(alpha = 0.35f),
                            topLeft = Offset(inset, inset),
                            size = Size(size.width - 2 * inset, size.height - 2 * inset),
                            cornerRadius = CornerRadius(15.dp.toPx()),
                            style = Stroke(1.dp.toPx()),
                        )
                    },
            ) {
                // the Quran text is always right to left, whatever the language of the app
                CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
                    LazyColumn(
                        Modifier.fillMaxSize().onGloballyPositioned { listTop = it.positionInRoot().y },
                        state = listState,
                        contentPadding = PaddingValues(horizontal = 14.dp, vertical = 10.dp),
                        verticalArrangement = Arrangement.spacedBy(0.dp),
                    ) {
                        item { SurahBanner(stringArrayResource(R.array.surah_names)[state.surah - 1]) }
                        itemsIndexed(pages) { _, page ->
                            AyahPage(
                                page,
                                hideUpcoming = state.memorization && reciting,
                                onWordClick = vm::showDetail,
                                onWordPlaced = { index, top -> wordTops[index] = top },
                            )
                        }
                    }
                }
            }
            Spacer(Modifier.height(10.dp))
        }
        state.detail?.let { WordDetailSheet(it, onDismiss = vm::closeDetail) }
        if (showAll) {
            ResultListSheet(flagged, onPick = {
                showAll = false
                goTo(it)
            }, onDismiss = { showAll = false })
        }
        if (reciting) {
            ListeningPanel(
                state,
                onPause = vm::pause,
                onResume = vm::resume,
                onStop = vm::stop,
            )
        } else {
            ResultPanel(flagged, onBack = vm::back, onRetry = vm::retry, onPick = goTo, onSeeAll = { showAll = true })
        }
    }
}

@Composable
private fun TopBar(
    state: UiState,
    reciting: Boolean,
    onClose: () -> Unit,
) {
    val surahNames = stringArrayResource(R.array.surah_names)
    Row(Modifier.fillMaxWidth().padding(top = 8.dp, bottom = 12.dp), verticalAlignment = Alignment.CenterVertically) {
        RoundAction(onClose) { Text("✕", fontSize = 16.sp, color = MaterialTheme.colorScheme.primary) }
        Spacer(Modifier.width(14.dp))
        Column(Modifier.weight(1f)) {
            if (state.auto && state.items.isEmpty()) {
                Text(stringResource(R.string.detect_title), style = MaterialTheme.typography.titleMedium)
            } else {
                Eyebrow(stringResource(R.string.range_title, surahNames[state.surah - 1], state.surah))
                Text(stringResource(R.string.range_summary, state.fromAyah, state.toAyah), style = MaterialTheme.typography.titleLarge)
            }
        }
        if (reciting) RecordingBadge(state.startedAt, state.pausedAt)
    }
}

/** The name of the surah on a ribbon with a gold border, as chapters open in a mushaf. */
@Composable
private fun SurahBanner(name: String) {
    val gold = LocalGold.current
    Row(
        Modifier
            .fillMaxWidth()
            .padding(bottom = 10.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(gold.copy(alpha = 0.10f))
            .border(1.dp, gold.copy(alpha = 0.7f), RoundedCornerShape(12.dp))
            .padding(horizontal = 14.dp, vertical = 8.dp),
        horizontalArrangement = Arrangement.Center,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("◆", color = gold, fontSize = 10.sp)
        Spacer(Modifier.width(12.dp))
        Text(name, style = MaterialTheme.typography.titleLarge, color = MaterialTheme.colorScheme.onBackground)
        Spacer(Modifier.width(12.dp))
        Text("◆", color = gold, fontSize = 10.sp)
    }
}

@Composable
private fun RecordingBadge(
    startedAt: Long,
    pausedAt: Long,
) {
    val paused = pausedAt != 0L
    val pulse =
        rememberInfiniteTransition(label = "pulse").animateFloat(
            initialValue = 0.35f,
            targetValue = 1f,
            animationSpec = infiniteRepeatable(tween(800), RepeatMode.Reverse),
            label = "dot",
        )
    val tone by animateColorAsState(if (paused) MaterialTheme.colorScheme.onSurfaceVariant else LocalAttention.current, label = "badge tone")
    var now by remember { mutableLongStateOf(android.os.SystemClock.elapsedRealtime()) }
    LaunchedEffect(Unit) {
        while (true) {
            now = android.os.SystemClock.elapsedRealtime()
            delay(500)
        }
    }
    val seconds = (((if (paused) pausedAt else now) - startedAt) / 1000).coerceAtLeast(0)
    Row(
        Modifier
            .clip(RoundedCornerShape(50))
            .background(tone.copy(alpha = 0.10f))
            .border(1.dp, tone.copy(alpha = 0.45f), RoundedCornerShape(50))
            .padding(horizontal = 12.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(
            Modifier
                .size(10.dp)
                .alpha(if (paused) 0.5f else pulse.value)
                .clip(CircleShape)
                .background(tone),
        )
        Spacer(Modifier.width(8.dp))
        Text("%d:%02d".format(seconds / 60, seconds % 60), style = MaterialTheme.typography.labelLarge, color = tone)
    }
}

/** A microphone drawn with strokes (the basic icon set has none). */
@Composable
private fun MicGlyph(
    color: Color,
    size: Dp,
) {
    Canvas(Modifier.size(size)) {
        val w = this.size.width
        val stroke = w * 0.085f
        val body = Size(w * 0.34f, w * 0.52f)
        drawRoundRect(color, Offset((w - body.width) / 2, w * 0.06f), body, CornerRadius(body.width / 2))
        drawArc(
            color,
            startAngle = 0f,
            sweepAngle = 180f,
            useCenter = false,
            topLeft = Offset(w * 0.2f, w * 0.22f),
            size = Size(w * 0.6f, w * 0.5f),
            style = Stroke(stroke, cap = StrokeCap.Round),
        )
        drawLine(color, Offset(w / 2, w * 0.72f), Offset(w / 2, w * 0.9f), stroke, StrokeCap.Round)
        drawLine(color, Offset(w * 0.34f, w * 0.92f), Offset(w * 0.66f, w * 0.92f), stroke, StrokeCap.Round)
    }
}

/** The microphone tile: while listening a soft ring spreads out and the tile glows with the voice; paused, it rests. */
@Composable
private fun MicTile(
    level: Float,
    listening: Boolean,
) {
    val accent = MaterialTheme.colorScheme.primary
    val ring =
        rememberInfiniteTransition(label = "ring").animateFloat(
            initialValue = 0f,
            targetValue = 1f,
            animationSpec = infiniteRepeatable(tween(1600)),
            label = "ring",
        )
    val glow by animateFloatAsState(if (listening) 0.16f + 0.3f * level else 0.07f, tween(180), label = "glow")
    val tone by animateColorAsState(if (listening) accent else MaterialTheme.colorScheme.onSurfaceVariant, tween(300), label = "mic tone")
    val ringAlpha by animateFloatAsState(if (listening) 1f else 0f, tween(300), label = "ring alpha")
    Box(Modifier.size(48.dp), contentAlignment = Alignment.Center) {
        Box(
            Modifier
                .size(48.dp)
                .graphicsLayer {
                    scaleX = 1f + 0.55f * ring.value
                    scaleY = 1f + 0.55f * ring.value
                    alpha = ringAlpha * (1f - ring.value) * 0.5f
                }.clip(RoundedCornerShape(14.dp))
                .background(accent),
        )
        Box(
            Modifier.size(48.dp).clip(RoundedCornerShape(14.dp)).background(accent.copy(alpha = glow)),
            contentAlignment = Alignment.Center,
        ) { MicGlyph(tone, 24.dp) }
    }
}

@Composable
private fun ListeningPanel(
    state: UiState,
    onPause: () -> Unit,
    onResume: () -> Unit,
    onStop: () -> Unit,
) {
    val paused = state.paused
    // a long stretch of near silence means the phone cannot hear the reciter: say so
    var quietSince by remember { mutableLongStateOf(0L) }
    var quiet by remember { mutableStateOf(false) }
    LaunchedEffect(state.level, paused) {
        val now = android.os.SystemClock.elapsedRealtime()
        if (state.level > 0.2f || paused) {
            quietSince = 0L
            quiet = false
        } else {
            if (quietSince == 0L) quietSince = now
            quiet = now - quietSince > 4000
        }
    }
    val currentRules =
        state.items
            .firstOrNull { it.state == ItemState.Current }
            ?.tajweedRules
            .orEmpty()
    val ruleLabels =
        currentRules.mapNotNull { rule ->
            tajweedLabelResource(rule)?.let { resource -> stringResource(resource) }
        }
    Column(Modifier.fillMaxWidth().padding(top = 8.dp)) {
        AnimatedVisibility(visible = quiet && !paused) {
            Text(
                stringResource(R.string.hint_quiet),
                Modifier.padding(bottom = 8.dp, start = 4.dp),
                style = MaterialTheme.typography.bodyMedium,
                color = LocalAttention.current,
            )
        }
        if (ruleLabels.isNotEmpty() && !paused) {
            Text(
                stringResource(R.string.tajweed_expected, ruleLabels.joinToString(" · ")),
                Modifier.padding(bottom = 8.dp, start = 4.dp),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.primary,
            )
        }
        Surface(
            Modifier.fillMaxWidth().animateContentSize(),
            shape = RoundedCornerShape(22.dp),
            color = MaterialTheme.colorScheme.surfaceVariant,
            contentColor = MaterialTheme.colorScheme.onBackground,
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
        ) {
            Row(Modifier.padding(10.dp), verticalAlignment = Alignment.CenterVertically) {
                MicTile(state.level, listening = !paused)
                Spacer(Modifier.width(12.dp))
                AnimatedContent(
                    targetState = paused,
                    modifier = Modifier.weight(1f),
                    transitionSpec = { (fadeIn(tween(220)) + slideInVertically(tween(220)) { it / 3 }) togetherWith fadeOut(tween(120)) },
                    label = "status",
                ) { isPaused ->
                    Text(
                        stringResource(if (isPaused) R.string.listening_paused else R.string.listening_now),
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 2,
                    )
                }
                Spacer(Modifier.width(8.dp))
                OutlinedButton(
                    onClick = if (paused) onResume else onPause,
                    modifier = Modifier.height(46.dp),
                    shape = RoundedCornerShape(14.dp),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = MaterialTheme.colorScheme.primary),
                    contentPadding = PaddingValues(horizontal = 14.dp),
                ) {
                    AnimatedContent(
                        targetState = paused,
                        transitionSpec = { fadeIn(tween(200)) togetherWith fadeOut(tween(120)) },
                        label = "pause or resume",
                    ) { isPaused ->
                        Text(stringResource(if (isPaused) R.string.resume else R.string.pause), fontWeight = FontWeight.SemiBold, maxLines = 1)
                    }
                }
                Spacer(Modifier.width(8.dp))
                Button(
                    onClick = onStop,
                    modifier = Modifier.height(46.dp),
                    shape = RoundedCornerShape(14.dp),
                    contentPadding = PaddingValues(horizontal = 16.dp),
                ) { Text(stringResource(R.string.finish), fontWeight = FontWeight.SemiBold, maxLines = 1) }
            }
        }
        Spacer(Modifier.height(8.dp))
    }
}

private fun kindOf(item: UiItem): EventKind =
    when {
        item.state == ItemState.Wrong -> EventKind.Wrong
        item.state == ItemState.Skipped -> EventKind.Skipped
        item.repeated -> EventKind.Repeated
        else -> EventKind.Added
    }

private fun kindLabel(kind: EventKind): Int =
    when (kind) {
        EventKind.Wrong -> R.string.kind_wrong
        EventKind.Skipped -> R.string.kind_skipped
        EventKind.Repeated -> R.string.kind_repeated
        EventKind.Added -> R.string.kind_added
    }

/** Every flagged word in a list that scrolls: tap one to go to it. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ResultListSheet(
    flagged: List<UiItem>,
    onPick: (UiItem) -> Unit,
    onDismiss: () -> Unit,
) {
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        containerColor = MaterialTheme.colorScheme.background,
        shape = RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp),
    ) {
        Column(Modifier.padding(horizontal = 24.dp)) {
            Eyebrow(stringResource(R.string.result_title))
            Text(pluralStringResource(R.plurals.history_differences, flagged.size, flagged.size), style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.height(8.dp))
            OrnamentRule()
        }
        LazyColumn(Modifier.fillMaxWidth(), contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp)) {
            itemsIndexed(flagged) { _, item ->
                val kind = kindOf(item)
                Row(
                    Modifier
                        .fillMaxWidth()
                        .padding(vertical = 3.dp)
                        .clip(RoundedCornerShape(14.dp))
                        .background(MaterialTheme.colorScheme.surfaceVariant)
                        .border(1.dp, MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(14.dp))
                        .clickable { onPick(item) }
                        .padding(horizontal = 16.dp, vertical = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            MarkSample(kind)
                            Spacer(Modifier.width(8.dp))
                            Text(stringResource(kindLabel(kind)), style = MaterialTheme.typography.labelLarge)
                        }
                        Text(
                            stringResource(R.string.where_format, item.ayah, item.word),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
                        Text(item.text, fontFamily = QuranFont, fontSize = 24.sp, lineHeight = 38.sp, color = LocalAttention.current)
                    }
                }
            }
        }
        Spacer(Modifier.height(12.dp))
    }
}

/** The words that were flagged, as a strip of cards: tap one to go to it (and see what was heard). */
@Composable
private fun ResultPanel(
    flagged: List<UiItem>,
    onBack: () -> Unit,
    onRetry: () -> Unit,
    onPick: (UiItem) -> Unit,
    onSeeAll: () -> Unit,
) {
    Column(Modifier.fillMaxWidth()) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(
                if (flagged.isEmpty()) {
                    stringResource(R.string.history_clean)
                } else {
                    pluralStringResource(R.plurals.history_differences, flagged.size, flagged.size)
                },
                Modifier.weight(1f),
                style = MaterialTheme.typography.titleLarge,
                color = if (flagged.isEmpty()) MaterialTheme.colorScheme.primary else LocalAttention.current,
            )
            if (flagged.isNotEmpty()) {
                TextButton(onClick = onSeeAll) { Text(stringResource(R.string.result_see_all), fontWeight = FontWeight.SemiBold) }
            }
        }
        if (flagged.isEmpty()) {
            Text(
                stringResource(R.string.nothing_found),
                Modifier.padding(horizontal = 4.dp, vertical = 4.dp),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        } else {
            LazyRow(
                Modifier.fillMaxWidth().padding(top = 8.dp),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                itemsIndexed(flagged) { _, item ->
                    val kind = kindOf(item)
                    Column(
                        Modifier
                            .clip(RoundedCornerShape(16.dp))
                            .background(MaterialTheme.colorScheme.surfaceVariant)
                            .border(1.dp, MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(16.dp))
                            .clickable { onPick(item) }
                            .padding(horizontal = 14.dp, vertical = 10.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                    ) {
                        CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
                            Text(item.text, fontFamily = QuranFont, fontSize = 22.sp, lineHeight = 34.sp, color = LocalAttention.current)
                        }
                        Spacer(Modifier.height(2.dp))
                        MarkSample(kind)
                        Spacer(Modifier.height(4.dp))
                        Text(stringResource(kindLabel(kind)), style = MaterialTheme.typography.labelLarge)
                        Text(
                            stringResource(R.string.where_format, item.ayah, item.word),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }
        }
        Spacer(Modifier.height(12.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            SecondaryButton(stringResource(R.string.back), onBack, Modifier.weight(1f).height(50.dp))
            PrimaryButton(stringResource(R.string.try_again), onRetry, Modifier.weight(1f).height(50.dp))
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
                EventKind.Wrong -> {
                    drawLine(attention, Offset(0f, y), Offset(size.width, y), strokeWidth = stroke)
                }

                EventKind.Skipped -> {
                    drawLine(
                        attention,
                        Offset(0f, y),
                        Offset(size.width, y),
                        strokeWidth = stroke,
                        pathEffect = PathEffect.dashPathEffect(floatArrayOf(6f, 6f)),
                    )
                }

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

private const val AYAHS_PER_PAGE = 10

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun AyahPage(
    ayahs: List<Pair<Int, List<UiItem>>>,
    hideUpcoming: Boolean,
    onWordClick: (UiItem) -> Unit,
    onWordPlaced: (Int, Float) -> Unit,
) {
    val basmala = ayahs.firstOrNull()?.first == 0
    FlowRow(
        Modifier.fillMaxWidth().padding(vertical = 1.dp),
        // the basmala is centered; the other lines are justified, edge to edge, as in a mushaf
        horizontalArrangement = if (basmala) Arrangement.spacedBy(6.dp, Alignment.CenterHorizontally) else Arrangement.SpaceBetween,
        verticalArrangement = Arrangement.spacedBy(0.dp),
        itemVerticalAlignment = Alignment.CenterVertically,
    ) {
        ayahs.forEach { (ayah, words) ->
            if (ayah > 0 && words.isNotEmpty()) {
                words.dropLast(1).forEach { WordText(it, hideUpcoming, onWordClick, onWordPlaced) }
                // the ring stays with the last word instead of wrapping onto a line of its own
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    WordText(words.last(), hideUpcoming, onWordClick, onWordPlaced)
                    AyahMarker(ayah)
                }
            } else {
                words.forEach { WordText(it, hideUpcoming, onWordClick, onWordPlaced) }
            }
        }
        // takes what is left of the last line, so that line is not stretched like the others
        if (!basmala) Spacer(Modifier.weight(1f))
    }
}

/** The ayah number in a gold ring (the ornamental brackets of the Quran fonts are missing from the system font). */
@Composable
private fun AyahMarker(ayah: Int) {
    val gold = LocalGold.current
    Box(
        Modifier
            .padding(horizontal = 2.dp)
            .size(31.dp)
            .clip(CircleShape)
            .background(gold.copy(alpha = 0.12f))
            .border(1.5.dp, gold, CircleShape),
        contentAlignment = Alignment.Center,
    ) {
        Text(arabicDigits(ayah), fontSize = if (ayah > 99) 11.sp else 14.sp, color = MaterialTheme.colorScheme.onBackground)
    }
}

/** The expected units and the heard units of a flagged word, the differing ones marked. */
@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
private fun WordDetailSheet(
    detail: WordDetail,
    onDismiss: () -> Unit,
) {
    val attention = LocalAttention.current
    val diff = remember(detail) { diffUnits(detail.expected, detail.heard) }
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        containerColor = MaterialTheme.colorScheme.background,
        shape = RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp),
    ) {
        Column(Modifier.fillMaxWidth().padding(horizontal = 24.dp).padding(bottom = 28.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
                Text(detail.text, fontFamily = QuranFont, fontSize = 31.sp, lineHeight = 48.sp, color = attention, modifier = Modifier.fillMaxWidth())
            }
            Eyebrow(
                stringResource(if (detail.state == ItemState.Skipped) R.string.kind_skipped else R.string.kind_wrong) +
                    " · " + stringResource(R.string.where_format, detail.ayah, detail.word),
                color = attention,
            )
            OrnamentRule()
            UnitsRow(stringResource(R.string.detail_expected), detail.expected, diff.expectedDiffers)
            if (detail.state == ItemState.Skipped || detail.heard.isEmpty()) {
                Text(stringResource(R.string.detail_nothing), color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                UnitsRow(stringResource(R.string.detail_heard), detail.heard, diff.heardDiffers)
            }
            Text(stringResource(R.string.detail_note), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun UnitsRow(
    label: String,
    units: List<String>,
    differing: Set<Int>,
) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(label, style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
        CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                units.forEachIndexed { i, unit ->
                    val marked = i in differing
                    Text(
                        unit,
                        Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(if (marked) LocalAttention.current.copy(alpha = 0.2f) else MaterialTheme.colorScheme.background)
                            .padding(horizontal = 10.dp, vertical = 4.dp),
                        fontSize = 22.sp,
                        color = if (marked) LocalAttention.current else MaterialTheme.colorScheme.onSurface,
                        fontWeight = if (marked) FontWeight.Bold else FontWeight.Normal,
                    )
                }
            }
        }
    }
}

/** One item with its mark, drawn just under the letters (shapes, not colors, tell the problems apart). */
@Composable
private fun WordText(
    item: UiItem,
    hideUpcoming: Boolean,
    onClick: (UiItem) -> Unit,
    onPlaced: (Int, Float) -> Unit,
) {
    val attention = LocalAttention.current
    val accent = MaterialTheme.colorScheme.primary
    val wordColor =
        when (item.state) {
            ItemState.Ok -> accent
            ItemState.Wrong, ItemState.Skipped -> attention
            else -> MaterialTheme.colorScheme.onBackground
        }
    var baseline by remember { mutableFloatStateOf(0f) }
    val modifier =
        Modifier
            .then(if (item.state == ItemState.Current) Modifier.clip(RoundedCornerShape(10.dp)).background(LocalGold.current.copy(alpha = 0.22f)) else Modifier)
            .then(
                if (item.state == ItemState.Wrong || item.state == ItemState.Skipped) {
                    Modifier.clip(RoundedCornerShape(8.dp)).clickable { onClick(item) }
                } else {
                    Modifier
                },
            ).then(
                if (item.state == ItemState.Wrong || item.state == ItemState.Skipped || item.repeated || item.addedAfter) {
                    Modifier.onGloballyPositioned { onPlaced(item.index, it.positionInRoot().y) }
                } else {
                    Modifier
                },
            ).padding(horizontal = 3.dp)
            .drawBehind {
                val y = (baseline + 7.dp.toPx()).coerceAtMost(size.height - 1.dp.toPx())
                val stroke = 2.dp.toPx()
                when {
                    item.state == ItemState.Wrong -> {
                        drawLine(attention, Offset(0f, y), Offset(size.width, y), strokeWidth = stroke)
                    }

                    item.state == ItemState.Skipped -> {
                        drawLine(
                            attention,
                            Offset(0f, y),
                            Offset(size.width, y),
                            strokeWidth = stroke,
                            pathEffect = PathEffect.dashPathEffect(floatArrayOf(8f, 7f)),
                        )
                    }

                    item.repeated -> {
                        drawLine(attention, Offset(0f, y), Offset(size.width, y), strokeWidth = stroke / 1.5f)
                        drawLine(attention, Offset(0f, y + 4.dp.toPx()), Offset(size.width, y + 4.dp.toPx()), strokeWidth = stroke / 1.5f)
                    }
                }
            }
    if (hideUpcoming && (item.state == ItemState.Upcoming || item.state == ItemState.Current)) {
        // memorization: the word is still hidden, only its length shows
        val width = (item.text.count { it.isLetter() } * 11).coerceIn(30, 130).dp
        Box(Modifier.height(40.dp).padding(horizontal = 3.dp), contentAlignment = Alignment.Center) {
            Box(
                Modifier
                    .width(width)
                    .height(12.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(
                        if (item.state ==
                            ItemState.Current
                        ) {
                            accent.copy(alpha = 0.45f)
                        } else {
                            MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.22f)
                        },
                    ),
            )
        }
    } else {
        Text(
            item.text,
            modifier,
            fontFamily = QuranFont,
            fontSize = 27.sp,
            lineHeight = 40.sp,
            color = wordColor,
            onTextLayout = { baseline = it.getLineBaseline(0) },
        )
    }
    if (item.addedAfter) Text("‸", fontSize = 27.sp, color = attention) // a small caret between the words
}

private fun tajweedLabelResource(rule: TajweedRule): Int? =
    when (rule.tag) {
        "qalqalah_sughra" -> R.string.rule_qalqalah_sughra
        "qalqalah_kubra" -> R.string.rule_qalqalah_kubra
        "idgham_ghunnah" -> R.string.rule_idgham_ghunnah
        "idgham_no_ghunnah" -> R.string.rule_idgham_no_ghunnah
        "idgham_mithlayn_saghir" -> R.string.rule_idgham_mithlayn
        "ikhfa" -> R.string.rule_ikhfa
        "ikhfa_shafawi" -> R.string.rule_ikhfa_shafawi
        "iqlab" -> R.string.rule_iqlab
        "izhar_halqi" -> R.string.rule_izhar_halqi
        "izhar_mutlaq" -> R.string.rule_izhar_mutlaq
        "izhar_shafawi" -> R.string.rule_izhar_shafawi
        "ghunnah" -> R.string.rule_ghunnah
        "madd_tabi_i" -> R.string.rule_madd_tabi_i
        "madd_muttasil" -> R.string.rule_madd_muttasil
        "madd_munfasil" -> R.string.rule_madd_munfasil
        "madd_lazim" -> R.string.rule_madd_lazim
        "madd_arid_lissukun" -> R.string.rule_madd_arid
        "madd_leen" -> R.string.rule_madd_leen
        "madd_silah_sughra" -> R.string.rule_madd_silah_sughra
        "madd_silah_kubra" -> R.string.rule_madd_silah_kubra
        "madd_iwad" -> R.string.rule_madd_iwad
        "tafkhim" -> R.string.rule_tafkhim
        "tarqiq" -> R.string.rule_tarqiq
        "hamzat_wasl" -> R.string.rule_hamzat_wasl
        else -> null
    }

private fun arabicDigits(n: Int): String = n.toString().map { '٠' + (it - '0') }.joinToString("")
