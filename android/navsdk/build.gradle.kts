// The ONLY module that knows about Google's Navigation SDK (DECISIONS.md D-13). Everything
// Google-shaped is converted to this project's own NavUpdate here; nothing below knows about it.
// The SDK's data classes are labelled beta, with no promise of compatibility between versions.
plugins {
    alias(libs.plugins.android.library)
}

android {
    namespace = "com.motonav.navsdk"
    compileSdk = 37

    defaultConfig {
        minSdk = 26
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
    api(project(":protocol"))
    api(libs.navigation.sdk)
    coreLibraryDesugaring(libs.desugar.jdk.libs)
    testImplementation(libs.junit.jupiter)
    testRuntimeOnly(libs.junit.platform.launcher)
}

tasks.withType<Test>().configureEach {
    useJUnitPlatform()
    // The shared files live in the repository's protocol/ directory, next to android/.
    systemProperty("motonav.repoRoot", rootProject.projectDir.parentFile.absolutePath)
    inputs.files(fileTree(rootProject.projectDir.parentFile.resolve("protocol")) { include("*.txt") })
    testLogging {
        events("failed")
        showStandardStreams = false
        exceptionFormat = org.gradle.api.tasks.testing.logging.TestExceptionFormat.FULL
    }
}
