plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "app.mutqin"
    compileSdk = 37
    ndkVersion = "27.1.12297006"

    defaultConfig {
        applicationId = "app.mutqin"
        minSdk = 21 // old phones are supported (docs/APP_SPEC.md, section 9a)
        targetSdk = 37
        versionCode = 1
        versionName = "0.1.0"
        ndk { abiFilters += listOf("arm64-v8a", "armeabi-v7a") }
        externalNativeBuild { cmake { cppFlags += "-std=c++17" } }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    // ONNX Runtime and the sherpa-onnx C library (android/scripts/fetch_sherpa.sh)
    sourceSets
        .getByName("main")
        .jniLibs.directories
        .add("sherpa/jniLibs")

    buildFeatures {
        compose = true
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildTypes {
        release {
            // R8: shrink and optimize the code, and drop unused resources
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            // Signed with the debug key so the release build can be tried on a phone. A store release needs
            // the project's own signing key, kept outside the repository.
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    // The gated recognition model is not committed to the repository. A developer who has a local
    // copy can opt into putting it in a shareable APK with -PbundleModel=true; normal CI/debug
    // builds stay small and continue to use the adb model-install path.
    sourceSets
        .getByName("main")
        .assets.directories
        .add(
            layout.buildDirectory
                .dir("generated/model-assets")
                .get()
                .asFile
                .absolutePath,
        )
}

dependencies {
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.foundation)
    implementation(libs.androidx.compose.material3)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    debugImplementation(libs.androidx.compose.ui.tooling)
}

// The verified Quran text is not in the repository (Tanzil terms); it is copied from data/quran/ into the
// app's assets at build time (run tools/quran_text/download.py first).
// -PnoQuranAssets skips the copy (CI jobs that only lint or compile, and have no Tanzil text).
val quranSource = rootProject.file("../data/quran")
val modelSource = rootProject.file("../models/zipformer_p-quran")
val modelAssetsDir = layout.buildDirectory.dir("generated/model-assets")
val bundleModel = providers.gradleProperty("bundleModel").map { it == "1" || it.equals("true", ignoreCase = true) }.orElse(false)
tasks.register<Copy>("copyQuran") {
    onlyIf { !providers.gradleProperty("noQuranAssets").isPresent }
    from(quranSource) { include("quran-uthmani.txt", "quran-simple-clean.txt", "checksums.txt") }
    into(layout.projectDirectory.dir("src/main/assets/quran"))
    doFirst {
        require(quranSource.resolve("quran-uthmani.txt").exists()) {
            "data/quran is missing: run python3 tools/quran_text/download.py first"
        }
    }
}

// Keep this as a generated directory so a non-bundled build cannot accidentally reuse a model
// copied by an earlier local build. The model files are ignored by Git and must be obtained under
// the model's NPL-1.1 terms before using this option.
tasks.register<Sync>("bundleLocalModel") {
    if (bundleModel.get()) {
        from(modelSource) {
            include("quran_phoneme_zipformer.int8.onnx", "tokens.txt", "expected_units.tsv", "LICENSE")
        }
    }
    into(modelAssetsDir.map { it.dir("model") })
    doFirst {
        if (bundleModel.get()) {
            val required = listOf("quran_phoneme_zipformer.int8.onnx", "tokens.txt", "expected_units.tsv", "LICENSE")
            val missing = required.filter { !modelSource.resolve(it).isFile }
            require(missing.isEmpty()) {
                "bundleModel=true but the local recognition model is incomplete; missing: ${missing.joinToString()}"
            }
        }
    }
}

tasks.named("preBuild") {
    dependsOn("copyQuran", "bundleLocalModel")
}
