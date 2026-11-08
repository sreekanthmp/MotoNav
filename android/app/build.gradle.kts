// The Android app: a thin adapter over BluetoothGatt (AndroidGattPort) and a minimal test screen.
// All decisions live in :link and :protocol, which are tested without a phone.
//
// There is no Kotlin plugin here: Android Gradle Plugin 9 has Kotlin built in.
plugins {
    alias(libs.plugins.android.application)
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
        versionName = "0.3.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
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
}
