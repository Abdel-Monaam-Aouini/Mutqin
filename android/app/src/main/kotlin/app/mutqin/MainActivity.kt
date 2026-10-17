package app.mutqin

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Color
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import app.mutqin.ui.Appearance
import app.mutqin.ui.MainViewModel
import app.mutqin.ui.MutqinApp
import app.mutqin.ui.MutqinTheme
import app.mutqin.ui.ThemeMode
import java.util.Locale

class MainActivity : ComponentActivity() {
    private val vm: MainViewModel by viewModels()
    private var language = AppLanguage.ENGLISH
    private var savedTheme = ThemeMode.System
    private var shownTheme by mutableStateOf(ThemeMode.System)

    override fun attachBaseContext(newBase: Context) {
        val selected = AppPreferences.language(newBase)
        val locale = Locale.forLanguageTag(selected.tag)
        Locale.setDefault(locale)
        val configuration = Configuration(newBase.resources.configuration)
        configuration.setLocale(locale)
        super.attachBaseContext(newBase.createConfigurationContext(configuration))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        language = AppPreferences.language(this)
        savedTheme = AppPreferences.themeMode(this)
        shownTheme = savedTheme
        enableEdgeToEdge()
        setContent {
            val systemDark = isSystemInDarkTheme()
            val dark = shownTheme == ThemeMode.Dark || (shownTheme == ThemeMode.System && systemDark)
            // the clock and battery icons of the status bar must be readable on the page that is shown
            DisposableEffect(dark) {
                enableEdgeToEdge(
                    statusBarStyle = if (dark) SystemBarStyle.dark(Color.TRANSPARENT) else SystemBarStyle.light(Color.TRANSPARENT, Color.TRANSPARENT),
                    navigationBarStyle = if (dark) SystemBarStyle.dark(Color.TRANSPARENT) else SystemBarStyle.light(Color.TRANSPARENT, Color.TRANSPARENT),
                )
                onDispose {}
            }
            MutqinTheme(shownTheme) {
                val state by vm.state.collectAsState()
                var pendingAuto by remember { mutableStateOf(false) }
                val askMic =
                    rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
                        if (granted) vm.start(pendingAuto)
                    }
                val appearance =
                    Appearance(
                        mode = shownTheme,
                        saved = savedTheme,
                        preview = { shownTheme = it },
                        save = {
                            savedTheme = it
                            shownTheme = it
                            AppPreferences.saveThemeMode(this, it)
                        },
                    )
                MutqinApp(state, vm, language, appearance, ::selectLanguage) { auto ->
                    if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
                        vm.start(auto)
                    } else {
                        pendingAuto = auto
                        askMic.launch(Manifest.permission.RECORD_AUDIO)
                    }
                }
            }
        }
        // Debug builds only: replay a WAV from the app's external files folder instead of the microphone, e.g.
        //   adb shell am start -n app.mutqin/.MainActivity --es wav take.wav --ei surah 5 --ei from 1 --ei to 8 --ei level 1
        if (BuildConfig.DEBUG) {
            if (intent.getBooleanExtra("corrupt_quran", false)) {
                vm.debugCorruptQuran = true
                vm.prepare()
            }
            intent.getStringExtra("wav")?.let { wav ->
                vm.debugReplay(
                    wav,
                    intent.getIntExtra("surah", 1),
                    intent.getIntExtra("from", 1),
                    intent.getIntExtra("to", 7),
                    intent.getIntExtra("level", 1),
                )
            }
        }
    }

    private fun selectLanguage(selected: AppLanguage) {
        if (selected == language) return
        AppPreferences.saveLanguage(this, selected)
        language = selected
        recreate()
    }
}
