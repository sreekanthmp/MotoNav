// MotoNav Android project (Phase 3).
//
// Modules:
//   :protocol  pure Kotlin/JVM  the BLE protocol: model, frame codec, STATUS, simulated messages
//   :link      pure Kotlin/JVM  the connection logic, written against an interface so it is
//                               tested without a phone (GattPort)
//   :app       Android          the thin Android adapter (BluetoothGatt) and a minimal screen
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "MotoNav"
include(":protocol", ":link", ":app")
