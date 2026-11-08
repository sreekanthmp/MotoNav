import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// The BLE protocol, in pure Kotlin: no Android, no Bluetooth. Tested against the shared
// vectors in ../../protocol/vectors.txt and ../../protocol/scenarios.txt.
plugins {
    alias(libs.plugins.kotlin.jvm)
}

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
        allWarningsAsErrors.set(true)
    }
}

dependencies {
    testImplementation(libs.junit.jupiter)
    testRuntimeOnly(libs.junit.platform.launcher)
}

tasks.test {
    useJUnitPlatform()
    // The shared files live in the repository's protocol/ directory, next to android/.
    systemProperty("motonav.repoRoot", rootProject.projectDir.parentFile.absolutePath)
    // Re-run even if nothing changed in this module: the shared files are inputs too.
    inputs.files(fileTree(rootProject.projectDir.parentFile.resolve("protocol")) { include("*.txt") })
    inputs.file(rootProject.projectDir.parentFile.resolve("BLUETOOTH_PROTOCOL.md"))
    testLogging {
        events("failed")
        showStandardStreams = false
        exceptionFormat = org.gradle.api.tasks.testing.logging.TestExceptionFormat.FULL
    }
}
