# MotoNav Android project (Phase 3)

A test sender for the MotoNav device: it finds the device over Bluetooth LE, connects, and sends
simulated navigation. **It is not the navigation app.** The Google Navigation SDK is Phase 4.

**Status:** the protocol and the connection logic are **host-tested** (they run on a computer
with no phone). The Android app is **compile-checked only** (it builds, with no compiler or
lint warnings). **It has never run on a phone, and nothing has run against a MotoNav.**
See [TESTING.md](../TESTING.md) section 6.

## Modules

| Module | Kind | What it holds |
|---|---|---|
| `:protocol` | pure Kotlin/JVM | The BLE protocol ([BLUETOOTH_PROTOCOL.md](../BLUETOOTH_PROTOCOL.md)): model, frame codec, CRC, STATUS parser, the simulated messages. Checked against the shared files `../protocol/vectors.txt` and `../protocol/scenarios.txt`. |
| `:link` | pure Kotlin/JVM | The connection logic as a state machine: connect, discover, negotiate the MTU, turn on status messages, then send. It talks to Bluetooth only through the `GattPort` interface, so it is tested with a fake. |
| `:app` | Android | `AndroidGattPort` (BluetoothGatt), `DeviceScanner`, and one screen. Thin: it starts operations and passes results on. It decides nothing. |

(Not to be confused with `../protocol/`, which holds the shared test files and the Python tool.)

## Build

You need JDK 17 or newer and an Android SDK with **platform 37.0** and **build-tools 36.0.0**.
Tell Gradle where the SDK is, either way:

```bash
export ANDROID_HOME=/path/to/android-sdk          # or create android/local.properties: sdk.dir=/path/to/android-sdk
```

```bash
cd android
./gradlew :protocol:test :link:test               # the host tests: no phone, no SDK needed for these two
./gradlew :app:lintDebug :app:assembleDebug       # lint (warnings are errors) and the debug APK
```

The APK is `app/build/outputs/apk/debug/app-debug.apk`. Install it with `adb install -r` or open the
`android/` folder in Android Studio. The first build downloads Gradle 9.6.0 (checked against a
published checksum), the Android Gradle Plugin and JUnit.

Pinned versions (`gradle/libs.versions.toml`, `gradle/wrapper`): Gradle 9.6.0, Android Gradle Plugin
9.5.1, Kotlin 2.2.10, JUnit 6.1.3, compileSdk and targetSdk 37, minSdk 26.

## Rules the code keeps

- **Road names are never persisted.** No system log, files, preferences or database (a test in
  `firmware/host/tests_py/test_android_policy.py` checks the sources). `NavUpdate.toString()` does
  not print the road name.
- **No third-party dependencies** in the app (a test checks this too).
- **Failures are loud.** A refused MTU, a missing service or a timeout ends the link with a reason.
  Nothing retries silently. Reconnection and background operation are Phase 5.
