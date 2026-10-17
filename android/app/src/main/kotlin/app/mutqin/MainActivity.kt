package app.mutqin

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.core.content.ContextCompat
import app.mutqin.ui.MainViewModel
import app.mutqin.ui.MutqinApp
import app.mutqin.ui.MutqinTheme
import java.util.Locale

class MainActivity : ComponentActivity() {
    private val vm: MainViewModel by viewModels()
    private var language = AppLanguage.ENGLISH

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
        enableEdgeToEdge()
        setContent {
            MutqinTheme {
                val state by vm.state.collectAsState()
                val askMic =
                    rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
                        if (granted) vm.start()
                    }
                MutqinApp(state, vm, language, ::selectLanguage) {
                    if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
                        vm.start()
                    } else {
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
