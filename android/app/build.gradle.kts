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
tasks.named("preBuild") { dependsOn("copyQuran") }
