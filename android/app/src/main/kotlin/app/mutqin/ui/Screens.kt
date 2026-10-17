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
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
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
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
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
    onLanguageSelected: (AppLanguage) -> Unit,
    onStart: (auto: Boolean) -> Unit,
) {
    val layoutDirection = if (language == AppLanguage.ARABIC) LayoutDirection.Rtl else LayoutDirection.Ltr
    CompositionLocalProvider(LocalLayoutDirection provides layoutDirection) {
        Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background, contentColor = MaterialTheme.colorScheme.onBackground) {
            Box(Modifier.fillMaxSize().safeDrawingPadding().padding(horizontal = 20.dp)) {
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
                        when (sub) {
                            SetupPage.Preferences -> {
                                PreferencesScreen(state, vm, language, onLanguageSelected) { sub = SetupPage.Home }
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

/** The app icon, softly pulsing: shown while the model loads or a session starts (no text, no spinner). */
@Composable
private fun LogoLoader() {
    val pulse = rememberInfiniteTransition(label = "logo")
    val alpha by pulse.animateFloat(
        initialValue = 0.55f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(900), RepeatMode.Reverse),
        label = "logo alpha",
    )
    MutqinLogo(120.dp, Modifier.alpha(alpha))
}

@Composable
private fun Centered(content: @Composable () -> Unit) {
    Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) { content() }
}

@Composable
private fun Card(
    modifier: Modifier = Modifier,
    content: @Composable () -> Unit,
) {
    Surface(modifier.fillMaxWidth(), shape = RoundedCornerShape(20.dp), color = MaterialTheme.colorScheme.surfaceVariant) {
        Column(Modifier.padding(18.dp)) { content() }
    }
}

/** The exact error, selectable, with a button to copy it (for a bug report). */
@Composable
private fun ErrorScreen(
    error: String,
    onRetry: () -> Unit,
) {
    val clipboard = LocalClipboardManager.current
    var copied by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxSize().padding(vertical = 24.dp)) {
        Text(
            stringResource(R.string.error_quran_title),
            style = MaterialTheme.typography.headlineSmall,
            fontWeight = FontWeight.SemiBold,
            color = LocalAttention.current,
        )
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
                onClick = {
                    clipboard.setText(AnnotatedString(error))
                    copied = true
                },
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
private fun NeedModelScreen(
    state: UiState,
    onCheck: () -> Unit,
) {
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

private enum class SetupPage { Home, Preferences, History }

/** The app icon drawn with the theme colors: a closed ring (itqan, mastery) around three sound bars. */
@Composable
private fun MutqinLogo(
    size: Dp,
    modifier: Modifier = Modifier,
) {
    val tile = MaterialTheme.colorScheme.primary
    val ink = MaterialTheme.colorScheme.onPrimary
    Canvas(modifier.size(size)) {
        val u = this.size.minDimension / 1024f // the icon is designed on a 1024 grid (docs/branding/icon.svg)
        drawRoundRect(tile, cornerRadius = CornerRadius(224f * u))
        val line = Stroke(width = 44f * u, cap = StrokeCap.Round)
        drawCircle(ink, radius = 300f * u, center = Offset(512f * u, 512f * u), style = line)
        for ((x, y1, y2) in listOf(Triple(420f, 452f, 572f), Triple(512f, 392f, 632f), Triple(604f, 432f, 592f))) {
            drawLine(ink, Offset(x * u, y1 * u), Offset(x * u, y2 * u), strokeWidth = 44f * u, cap = StrokeCap.Round)
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SetupScreen(
    state: UiState,
    vm: MainViewModel,
    onOpenPreferences: () -> Unit,
    onOpenHistory: () -> Unit,
    onStart: (auto: Boolean) -> Unit,
) {
    val surahNames = stringArrayResource(R.array.surah_names)
    var picking by remember { mutableStateOf(false) }
    // everything fits on one screen: the logo takes whatever height the rest leaves
    Column(Modifier.fillMaxSize()) {
        BoxWithConstraints(Modifier.weight(1f).fillMaxWidth()) {
            val logo = minOf(180.dp, maxOf(72.dp, maxHeight - 96.dp))
            IconButton(
                onClick = onOpenPreferences,
                modifier =
                    Modifier
                        .align(Alignment.TopEnd)
                        .padding(top = 8.dp)
                        .size(48.dp)
                        .clip(RoundedCornerShape(14.dp))
                        .background(MaterialTheme.colorScheme.secondaryContainer),
            ) {
                Icon(
                    Icons.Default.Settings,
                    contentDescription = stringResource(R.string.preferences),
                    tint = MaterialTheme.colorScheme.primary,
                )
            }
            IconButton(
                onClick = onOpenHistory,
                modifier =
                    Modifier
                        .align(Alignment.TopStart)
                        .padding(top = 8.dp)
                        .size(48.dp)
                        .clip(RoundedCornerShape(14.dp))
                        .background(MaterialTheme.colorScheme.secondaryContainer),
            ) {
                Icon(
                    Icons.AutoMirrored.Filled.List,
                    contentDescription = stringResource(R.string.history),
                    tint = MaterialTheme.colorScheme.primary,
                )
            }
            Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
                MutqinLogo(logo)
                Spacer(Modifier.height(14.dp))
                Text(
                    stringResource(R.string.app_name),
                    style = MaterialTheme.typography.displaySmall,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onBackground,
                )
                Text(stringResource(R.string.app_tagline), style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.primary)
            }
        }

        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Card {
                Text(stringResource(R.string.surah), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
                Spacer(Modifier.height(8.dp))
                Row(
                    Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(14.dp))
                        .background(MaterialTheme.colorScheme.background)
                        .clickable { picking = true }
                        .padding(horizontal = 14.dp, vertical = 12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(String.format(Locale.US, "%02d", state.surah), color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.SemiBold)
                    Spacer(Modifier.width(14.dp))
                    Text(surahNames[state.surah - 1], Modifier.weight(1f), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                    Text(
                        stringResource(R.string.ayahs_in_surah, state.ayahCount),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Text("▾", Modifier.padding(start = 10.dp), color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }

            Card {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(R.string.practice_passage), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
                        Spacer(Modifier.height(2.dp))
                        Text(
                            stringResource(R.string.range_summary, state.fromAyah, state.toAyah),
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.SemiBold,
                        )
                    }
                    TextButton(onClick = {
                        vm.setFrom(1)
                        vm.setTo(state.ayahCount)
                    }) { Text(stringResource(R.string.whole_surah), fontWeight = FontWeight.SemiBold) }
                }
                Spacer(Modifier.height(8.dp))
                RangeRow(
                    stringResource(R.string.ayah_from),
                    state.fromAyah,
                    state.fromAyah > 1,
                    state.fromAyah < state.ayahCount,
                    { vm.setFrom(state.fromAyah - 1) },
                    { vm.setFrom(state.fromAyah + 1) },
                )
                Spacer(Modifier.height(6.dp))
                RangeRow(
                    stringResource(R.string.ayah_to),
                    state.toAyah,
                    state.toAyah > 1,
                    state.toAyah < state.ayahCount,
                    { vm.setTo(state.toAyah - 1) },
                    { vm.setTo(state.toAyah + 1) },
                )
                Spacer(Modifier.height(4.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(R.string.memorization_mode), style = MaterialTheme.typography.bodyLarge)
                        Text(
                            stringResource(R.string.memorization_hint),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Switch(checked = state.memorization, onCheckedChange = vm::setMemorization)
                }
            }

            state.message?.let {
                Card {
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
            Button(onClick = { onStart(false) }, modifier = Modifier.fillMaxWidth().height(52.dp), shape = RoundedCornerShape(16.dp)) {
                Text(stringResource(R.string.start), fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
                Text("  $forward", fontSize = 16.sp)
            }
            TextButton(onClick = { onStart(true) }, Modifier.fillMaxWidth()) {
                Text(stringResource(R.string.detect_auto), fontWeight = FontWeight.SemiBold)
            }
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
    val accent = MaterialTheme.colorScheme.primary
    val list = rememberLazyListState(initialFirstVisibleItemIndex = maxOf(0, selected - 3))
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        containerColor = MaterialTheme.colorScheme.surfaceVariant,
        shape = RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp),
    ) {
        Text(
            stringResource(R.string.choose_surah),
            Modifier.padding(horizontal = 24.dp, vertical = 4.dp),
            style = MaterialTheme.typography.titleMedium,
            fontWeight = FontWeight.SemiBold,
            color = accent,
        )
        LazyColumn(Modifier.fillMaxWidth(), state = list, contentPadding = PaddingValues(horizontal = 12.dp, vertical = 8.dp)) {
            itemsIndexed(names.toList()) { i, name ->
                val chosen = i + 1 == selected
                Row(
                    Modifier
                        .fillMaxWidth()
                        .padding(vertical = 2.dp)
                        .clip(RoundedCornerShape(14.dp))
                        .background(if (chosen) accent.copy(alpha = 0.16f) else Color.Transparent)
                        .clickable { onPick(i + 1) }
                        .padding(horizontal = 12.dp, vertical = 10.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Box(
                        Modifier.size(38.dp).clip(RoundedCornerShape(11.dp)).background(accent.copy(alpha = if (chosen) 0.28f else 0.10f)),
                        contentAlignment = Alignment.Center,
                    ) { Text((i + 1).toString(), fontWeight = FontWeight.SemiBold, color = accent, fontSize = 14.sp) }
                    Spacer(Modifier.width(14.dp))
                    Text(
                        name,
                        Modifier.weight(1f),
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = if (chosen) FontWeight.SemiBold else FontWeight.Normal,
                        color = if (chosen) accent else MaterialTheme.colorScheme.onSurface,
                    )
                    if (chosen) Text("✓", color = accent, fontWeight = FontWeight.Bold)
                }
            }
        }
    }
}

/** The attempts kept on this phone, newest first. */
@Composable
private fun HistoryScreen(
    state: UiState,
    vm: MainViewModel,
    onClose: () -> Unit,
) {
    BackHandler(onBack = onClose)
    val surahNames = stringArrayResource(R.array.surah_names)
    var confirming by remember { mutableStateOf(false) }
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(top = 12.dp, bottom = 16.dp), verticalAlignment = Alignment.CenterVertically) {
            IconButton(
                onClick = onClose,
                modifier = Modifier.size(48.dp).clip(RoundedCornerShape(14.dp)).background(MaterialTheme.colorScheme.secondaryContainer),
            ) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.back), tint = MaterialTheme.colorScheme.primary)
            }
            Spacer(Modifier.width(14.dp))
            Column {
                Text(stringResource(R.string.app_name), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
                Text(stringResource(R.string.history), style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.SemiBold)
            }
        }
        if (state.history.isEmpty()) {
            Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                Text(
                    stringResource(R.string.history_empty),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.padding(horizontal = 24.dp),
                )
            }
        } else {
            val format = remember { DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT) }
            LazyColumn(Modifier.weight(1f).fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                itemsIndexed(state.history) { _, entry ->
                    Card {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(
                                stringResource(R.string.range_title, surahNames.getOrElse(entry.surah - 1) { "" }, entry.surah),
                                Modifier.weight(1f),
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold,
                            )
                            Text("%d:%02d".format(entry.seconds / 60, entry.seconds % 60), color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                        Text(
                            stringResource(R.string.range_summary, entry.fromAyah, entry.toAyah),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Spacer(Modifier.height(8.dp))
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(
                                if (entry.differences == 0) {
                                    stringResource(R.string.history_clean)
                                } else {
                                    pluralStringResource(R.plurals.history_differences, entry.differences, entry.differences)
                                },
                                color = if (entry.differences == 0) MaterialTheme.colorScheme.primary else LocalAttention.current,
                                fontWeight = FontWeight.SemiBold,
                            )
                            if (entry.memorization) {
                                Text("  ·  " + stringResource(R.string.history_memorization), color = MaterialTheme.colorScheme.onSurfaceVariant)
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
            title = { Text(stringResource(R.string.history_clear_title)) },
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

/** Language and feedback level. Nothing changes until "Save preferences"; the choices are then kept on the phone. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun PreferencesScreen(
    state: UiState,
    vm: MainViewModel,
    language: AppLanguage,
    onLanguageSelected: (AppLanguage) -> Unit,
    onClose: () -> Unit,
) {
    var draftLanguage by rememberSaveable { mutableStateOf(language) }
    var draftLevel by rememberSaveable { mutableIntStateOf(state.strictness) }
    BackHandler(onBack = onClose)
    // the screen shows the chosen language at once, before anything is saved
    val base = LocalContext.current
    val current = LocalConfiguration.current
    val preview =
        remember(draftLanguage, base, current) {
            val configuration = Configuration(current)
            configuration.setLocale(Locale.forLanguageTag(draftLanguage.tag))
            base.createConfigurationContext(configuration)
        }
    CompositionLocalProvider(
        LocalContext provides preview,
        LocalLayoutDirection provides if (draftLanguage == AppLanguage.ARABIC) LayoutDirection.Rtl else LayoutDirection.Ltr,
    ) {
        Column(Modifier.fillMaxSize()) {
            Row(Modifier.fillMaxWidth().padding(top = 12.dp, bottom = 16.dp), verticalAlignment = Alignment.CenterVertically) {
                IconButton(
                    onClick = onClose,
                    modifier = Modifier.size(48.dp).clip(RoundedCornerShape(14.dp)).background(MaterialTheme.colorScheme.secondaryContainer),
                ) {
                    Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.back), tint = MaterialTheme.colorScheme.primary)
                }
                Spacer(Modifier.width(14.dp))
                Column {
                    Text(stringResource(R.string.app_name), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
                    Text(stringResource(R.string.preferences), style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.SemiBold)
                }
            }
            Column(Modifier.weight(1f).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(14.dp)) {
                Card {
                    Text(stringResource(R.string.language), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
                    Spacer(Modifier.height(6.dp))
                    Text(
                        stringResource(R.string.preferences_language_hint),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Spacer(Modifier.height(12.dp))
                    val languages = listOf(AppLanguage.ENGLISH to R.string.language_english, AppLanguage.ARABIC to R.string.language_arabic)
                    SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                        languages.forEachIndexed { index, (option, label) ->
                            SegmentedButton(
                                selected = draftLanguage == option,
                                onClick = { draftLanguage = option },
                                shape = SegmentedButtonDefaults.itemShape(index, languages.size),
                            ) { Text(stringResource(label)) }
                        }
                    }
                }
                Card {
                    Text(stringResource(R.string.strictness), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
                    Spacer(Modifier.height(12.dp))
                    val levels =
                        listOf(
                            Triple(Strictness.RELAXED, R.string.strictness_relaxed, R.string.strictness_relaxed_hint),
                            Triple(Strictness.STANDARD, R.string.strictness_standard, R.string.strictness_standard_hint),
                            Triple(Strictness.STRICT, R.string.strictness_strict, R.string.strictness_strict_hint),
                        )
                    SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                        levels.forEachIndexed { i, (level, label, _) ->
                            SegmentedButton(
                                selected = draftLevel == level,
                                onClick = { draftLevel = level },
                                shape = SegmentedButtonDefaults.itemShape(i, levels.size),
                            ) { Text(stringResource(label)) }
                        }
                    }
                    Spacer(Modifier.height(10.dp))
                    Text(
                        stringResource(levels.first { it.first == draftLevel }.third),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
            Spacer(Modifier.height(12.dp))
            Button(
                onClick = {
                    vm.setStrictness(draftLevel)
                    onClose()
                    onLanguageSelected(draftLanguage) // recreates the screen when the language changed
                },
                modifier = Modifier.fillMaxWidth().height(48.dp),
                shape = RoundedCornerShape(14.dp),
            ) { Text(stringResource(R.string.save), fontSize = 16.sp, fontWeight = FontWeight.SemiBold) }
            Spacer(Modifier.height(8.dp))
        }
    }
}

@Composable
private fun RangeRow(
    label: String,
    value: Int,
    canMinus: Boolean,
    canPlus: Boolean,
    onMinus: () -> Unit,
    onPlus: () -> Unit,
) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyLarge)
        StepButton("−", canMinus, onMinus)
        Text(
            value.toString(),
            Modifier.width(56.dp),
            textAlign = TextAlign.Center,
            style = MaterialTheme.typography.titleLarge,
            fontWeight = FontWeight.SemiBold,
        )
        StepButton("+", canPlus, onPlus)
    }
}

/** A round − or + button; holding it repeats, so a surah number far away can be reached quickly. */
@Composable
private fun StepButton(
    label: String,
    enabled: Boolean,
    onStep: () -> Unit,
) {
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
private fun ReciteScreen(
    state: UiState,
    vm: MainViewModel,
) {
    val groups = remember(state.items) { state.items.groupBy { it.ayah }.toList() }
    val listState = rememberLazyListState()
    // the ayahs run on one after the other, as in a mushaf; they are cut into pages of a few ayahs for the list
    val pages = remember(groups) { groups.chunked(AYAHS_PER_PAGE) }
    val currentGroup = groups.indexOfFirst { (_, words) -> words.any { it.state == ItemState.Current } }
    val currentPage = if (currentGroup >= 0) currentGroup / AYAHS_PER_PAGE else -1
    LaunchedEffect(currentPage) { if (currentPage >= 0) listState.animateScrollToItem(currentPage) }
    val reciting = state.phase == Phase.Reciting

    Column(Modifier.fillMaxSize()) {
        TopBar(state, reciting, onClose = vm::back)
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
            // the Quran text is always right to left, whatever the language of the app
            CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
                LazyColumn(
                    Modifier.weight(1f).fillMaxWidth(),
                    state = listState,
                    verticalArrangement = Arrangement.spacedBy(0.dp),
                ) {
                    itemsIndexed(pages) { _, page -> AyahPage(page, hideUpcoming = state.memorization && reciting, onWordClick = vm::showDetail) }
                }
            }
        }
        if (reciting) {
            ListeningPanel(
                state,
                onPause = vm::pause,
                onResume = vm::resume,
                onStop = vm::stop,
            )
        } else {
            ResultPanel(state, onBack = vm::back, onRetry = vm::retry)
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
    Row(Modifier.fillMaxWidth().padding(vertical = 10.dp), verticalAlignment = Alignment.CenterVertically) {
        Box(
            Modifier
                .size(44.dp)
                .clip(CircleShape)
                .background(MaterialTheme.colorScheme.surfaceVariant)
                .clickable(onClick = onClose),
            contentAlignment = Alignment.Center,
        ) { Text("✕", fontSize = 18.sp, color = MaterialTheme.colorScheme.onSurfaceVariant) }
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            if (state.auto && state.items.isEmpty()) {
                Text(stringResource(R.string.detect_title), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            } else {
                Text(
                    stringResource(R.string.range_title, surahNames[state.surah - 1], state.surah),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                )
                Text(
                    stringResource(R.string.range_summary, state.fromAyah, state.toAyah),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        if (reciting) RecordingBadge(state.startedAt, state.pausedAt)
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
        Modifier.clip(RoundedCornerShape(50)).background(tone.copy(alpha = 0.14f)).padding(horizontal = 12.dp, vertical = 8.dp),
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
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.25f)),
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

@Composable
private fun ResultPanel(
    state: UiState,
    onBack: () -> Unit,
    onRetry: () -> Unit,
) {
    Column(Modifier.fillMaxWidth().padding(top = 8.dp)) {
        Card {
            Text(stringResource(R.string.result_title), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(6.dp))
            if (state.findings.isEmpty()) {
                Text(stringResource(R.string.nothing_found), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                Text(
                    pluralStringResource(R.plurals.differences_found, state.findings.size, state.findings.size),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        Spacer(Modifier.height(12.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            OutlinedButton(
                onClick = onBack,
                modifier = Modifier.weight(1f).height(56.dp),
                shape = RoundedCornerShape(16.dp),
            ) { Text(stringResource(R.string.back)) }
            Button(
                onClick = onRetry,
                modifier = Modifier.weight(1f).height(56.dp),
                shape = RoundedCornerShape(16.dp),
            ) { Text(stringResource(R.string.try_again)) }
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
) {
    FlowRow(
        Modifier.fillMaxWidth().padding(vertical = 1.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalArrangement = Arrangement.spacedBy(0.dp),
        itemVerticalAlignment = Alignment.CenterVertically,
    ) {
        ayahs.forEach { (ayah, words) ->
            if (ayah > 0 && words.isNotEmpty()) {
                words.dropLast(1).forEach { WordText(it, hideUpcoming, onWordClick) }
                // the ring stays with the last word instead of wrapping onto a line of its own
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    WordText(words.last(), hideUpcoming, onWordClick)
                    AyahMarker(ayah)
                }
            } else {
                words.forEach { WordText(it, hideUpcoming, onWordClick) }
            }
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
        containerColor = MaterialTheme.colorScheme.surfaceVariant,
        shape = RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp),
    ) {
        Column(Modifier.fillMaxWidth().padding(horizontal = 24.dp).padding(bottom = 28.dp), verticalArrangement = Arrangement.spacedBy(14.dp)) {
            CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Rtl) {
                Text(detail.text, fontSize = 36.sp, lineHeight = 56.sp, color = attention, modifier = Modifier.fillMaxWidth())
            }
            Text(
                stringResource(if (detail.state == ItemState.Skipped) R.string.kind_skipped else R.string.kind_wrong) +
                    " · " + stringResource(R.string.where_format, detail.ayah, detail.word),
                style = MaterialTheme.typography.labelLarge,
                color = attention,
            )
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
            .then(if (item.state == ItemState.Current) Modifier.clip(RoundedCornerShape(10.dp)).background(accent.copy(alpha = 0.18f)) else Modifier)
            .then(
                if (item.state == ItemState.Wrong || item.state == ItemState.Skipped) {
                    Modifier.clip(RoundedCornerShape(8.dp)).clickable { onClick(item) }
                } else {
                    Modifier
                },
            ).padding(horizontal = 3.dp)
            .drawBehind {
                val y = (baseline + 9.dp.toPx()).coerceAtMost(size.height - 1.dp.toPx())
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
                        drawLine(attention, Offset(0f, y + 5.dp.toPx()), Offset(size.width, y + 5.dp.toPx()), strokeWidth = stroke / 1.5f)
                    }
                }
            }
    if (hideUpcoming && (item.state == ItemState.Upcoming || item.state == ItemState.Current)) {
        // memorization: the word is still hidden, only its length shows
        val width = (item.text.count { it.isLetter() } * 13).coerceIn(34, 150).dp
        Box(Modifier.height(50.dp).padding(horizontal = 3.dp), contentAlignment = Alignment.Center) {
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
            fontSize = 30.sp,
            lineHeight = 50.sp,
            color = wordColor,
            onTextLayout = { baseline = it.getLineBaseline(0) },
        )
    }
    if (item.addedAfter) Text("‸", fontSize = 30.sp, color = attention) // a small caret between the words
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
