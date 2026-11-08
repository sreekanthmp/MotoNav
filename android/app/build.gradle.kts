import java.util.Properties

// The Android app: a thin adapter over BluetoothGatt (AndroidGattPort) and a minimal test screen.
// All decisions live in :link and :protocol, which are tested without a phone.
//
// There is no Kotlin plugin here: Android Gradle Plugin 9 has Kotlin built in.
plugins {
    alias(libs.plugins.android.application)
}

// The Google Maps Platform API key for the Navigation SDK. It is a secret: it is NEVER committed.
// Put it in android/secrets.properties (listed in .gitignore) as   MAPS_API_KEY=your-key
// or set the environment variable MAPS_API_KEY. Without either, the build still works with the
// placeholder below, and the SDK refuses to start (NOT_AUTHORIZED), which the screen reports.
val mapsApiKey: String = run {
    val file = rootProject.file("secrets.properties")
    val fromFile = if (file.exists()) {
        Properties().also { p -> file.inputStream().use { p.load(it) } }.getProperty("MAPS_API_KEY")
    } else null
    fromFile?.trim()?.takeIf { it.isNotEmpty() } ?: System.getenv("MAPS_API_KEY")?.trim()?.takeIf { it.isNotEmpty() } ?: "DEFAULT_API_KEY"
}

android {
    namespace = "com.motonav.app"
    compileSdk = 37

    defaultConfig {
        // PLACEHOLDER: the application id cannot change once the app is published. Decide it
        // before the first release (DECISIONS.md).
        applicationId = "com.motonav.app"
        minSdk = 26
        targetSdk = 37
        versionCode = 1
        versionName = "0.4.0"
        manifestPlaceholders["MAPS_API_KEY"] = mapsApiKey
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
        // Required by the Navigation SDK (Java library APIs on older Android versions).
        isCoreLibraryDesugaringEnabled = true
    }

    lint {
        warningsAsErrors = true
        abortOnError = true
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
        allWarningsAsErrors.set(true)
    }
}

dependencies {
    implementation(project(":link"))
    implementation(project(":navsdk"))
    coreLibraryDesugaring(libs.desugar.jdk.libs)
}
