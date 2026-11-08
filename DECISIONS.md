# Technical Decisions

Status values: **Given** (stated by the project owner), **Required** (forced by
a verified external rule), **Proposed** (recommended, awaiting approval),
**Open** (not decided).

Status vocabulary for implementation: **Implemented**, **Host-tested** (checked
by automated tests on a normal computer), **Device-tested** (checked on the real
board; nothing is device-tested yet), **Compile-checked**.

| ID | Decision | Status |
|---|---|---|
| D-01 | The phone is the only navigation processor. The ESP32 never routes or reads GPS. | Given |
| D-02 | Navigation comes from Google's official Navigation SDK TurnByTurn feed. Nothing is extracted from the Google Maps app. | Given |
| D-03 | Phone and device communicate over BLE GATT with a versioned protocol. | Given |
| D-04 | Android app in Kotlin; firmware in C++ with PlatformIO. | Given |
| D-05 | Road names are never persisted (no flash, logs, analytics or storage). | Required (Google policy: persisting road names is "scraping") |
| D-06 | No animations on the navigation screen. | Required for policy fit (Google flags animations during active navigation as unsafe) and chosen for readability |
| D-07 | ESP32 Arduino framework, not ESP-IDF. | Approved (Phase 0 review) |
| D-08 | Device display uses Adafruit GFX + SSD1306. **Refined in Phase 1:** screens do not call Adafruit GFX directly. They draw through an eight-method `Canvas` interface, and `Ssd1306Canvas` adapts it. Reason: the host tests need a drawing target that builds without Arduino, and a round AMOLED later needs only a new adapter. | Implemented. Interface host-tested. SSD1306 adapter compile-checked for the classic ESP32. Not device-tested. |
| D-09 | Arrow and icon graphics are vector-drawn, not stored bitmaps. | Implemented, host-tested |
| D-10 | `DeviceState` is the only thing screens read; `ScreenView` is what is visible; screens know nothing of BLE. | Implemented, host-tested |
| D-11 | BLE stack on the ESP32: NimBLE, specifically `h2zero/NimBLE-Arduino` **2.5.2**, pinned in `platformio.ini`. It was compile-checked on the pinned Arduino core (881,160 B flash, 67.2 %, 0 warnings). | Decided. Compile-checked. **Not device-tested.** |
| D-12 | Android BLE uses the platform APIs directly (`BluetoothGatt`, `BluetoothLeScanner`), no wrapper library. The calls were checked against Google's reference pages. | Implemented (Phase 3), compile-checked. **Not device-tested.** Revisit if a phone shows the raw API to be painful. |
| D-13 | Google types are confined to one Android file/module because the APIs are beta. | Proposed |
| D-14 | Message encoding: **compact binary**, little-endian, with a CRC-16 (see D-33). A NAV_UPDATE is at most 53 bytes, so it fits one write at the negotiated MTU. JSON would be several times larger and needs a parser on the ESP32 for no benefit. | Decided, implemented, host-tested |
| D-15 | Where ETA is computed: **on the phone**. The SDK provides remaining seconds, not a clock time, and the ESP32 has no clock. The protocol carries remaining seconds; if an ETA clock time is ever shown, the phone sends it. | Decided. The field is carried and host-tested; not displayed. |
| D-16 | `BLUETOOTH_PROTOCOL.md` is created when Phase 2 starts; `TESTING.md` was created in Phase 1. | Implemented (both) |
| D-17 | The model has `UTurnLeft` and `UTurnRight`, not the single `U_TURN` of the original brief. Google distinguishes clockwise and counterclockwise U-turns and they mirror between left- and right-hand traffic. | **Needs your approval** (deviates from the brief). Implemented. |
| D-18 | Host tests use a plain Makefile and a 60-line test harness, not PlatformIO's `native` test environment. Reason: it needs only a C++ compiler, with no package downloads. | Implemented |
| D-19 | No default pins, I2C address or display size anywhere in the C++ code. The build fails until `platformio.ini` supplies them. `platformio.ini` now supplies the **provisional** values for the identified hardware. | Implemented (your guideline 6) |
| D-20 | Non-ASCII road names are shown as one `?` per run, and a name with no letters or digits left is hidden. The 5x7 font has ASCII only. **Limitation:** road names in Indian scripts are not readable on this display. Revisit with the production display and font. | Implemented, host-tested |
| D-21 | Remaining distance and ETA are not in the Phase 1 model. They were added with the protocol in Phase 2, as `NavExtras` beside the snapshot, and are **not displayed**. Carried now because adding a field later needs a new protocol version; can be dropped before Phase 3 if you prefer. | Implemented, host-tested. **Open to change.** |
| D-22 | Distance text: under 100 m in 10 m steps, up to 974 m in 50 m steps, then km with one decimal, whole km from 9.95 km, capped at 999 km. The number and unit are separate so the number is drawn large. | Implemented, host-tested |
| D-23 | Phase 1 only supports I2C displays. | Implemented. The listing says I2C; SPI is not planned unless validation shows otherwise. |
| D-24 | The firmware targets the **classic ESP32** via PlatformIO board `esp32doit-devkit-v1` (DOIT DevKit V1, the 30-pin layout), not `nodemcu-32s`, which is a different board. A compile-time guard refuses any other chip. The pins match the board's Arduino defaults (SDA 21, SCL 22). | Implemented, compile-checked. **Provisional** until `hw_check` confirms the chip on the real board. |
| D-25 | Two standalone diagnostic environments live in `firmware/src/tools/`: `hw_check` (chip, radios, cores, flash size read from the chip, pin validity, I2C line state, scan at 100 and 400 kHz, repeated probing, swapped-wire check) and `oled_check` (five test patterns: SSD1306 vs SH1106, 128x64 geometry, scrambling, stuck pixels). They are not part of the MotoNav firmware and `hw_check` needs no display library. | Implemented, compile-checked, host-simulated. Not run on hardware. |
| D-26 | If the controller turns out to be an SH1106, support it by writing a new `Canvas` adapter, not by touching the screens. This is deliberately **not** done in advance. | Decided, not implemented |
| D-27 | **Diagnostic verdicts are decided by pure, host-tested code** (`lib/motonav/src/diag/hw_verdict.*`); the device code only gathers readings. Reason: a diagnostic that wrongly says PASS would mislead the person at the bench, and the readings can only be taken on hardware but the *decisions* can be tested on a computer. | Implemented, host-tested |
| D-28 | The real `hw_check.cpp` and `oled_check.cpp` are run on the host against small Arduino/ESP32 stand-ins (`firmware/host/shim/`) and a simulated board, so the glue code, and the expected output shown in `TESTING.md`, come from real execution. The shim is host-only and invents every value. | Implemented, host-tested |
| D-29 | The firmware's log lines (boot, reset reason, once-a-minute stats, render checks) are formatted in one place (`diag/log_format.*`), and a Python analyser (`firmware/host/analyze_log.py`) judges a long run against fixed criteria. Tests build logs with the real formatters, so the two cannot drift apart. | Implemented, host-tested |
| D-30 | Serial commands `d` (next test distance) and `r` (next test road name) were added to hold-mode, and the firmware prints an `expect:` line describing exactly what the display should show, so every formatting rule can be inspected on the real display. | Implemented, host-tested |
| D-31 | Static screens (Disconnected, Ready, Rerouting, Arrived) are hashed in the RAM frame buffer each time they are drawn, and any change from the first hash is logged. It detects memory corruption or changed drawing code. It **cannot** detect I2C glitches that corrupt what the glass shows. | Implemented, host-tested logic. Not run on hardware. |
| D-32 | **Phase 2 is a separate PlatformIO environment, `motonav_ble`; the Phase 1 environment `motonav` is unchanged.** Reason: the physical validation T1-T6 tests `motonav`, and a Phase 2 change to it would invalidate that work. `motonav` excludes the new files in `build_src_filter`. Cost: about 100 lines of start-up code are duplicated between `main.cpp` and `main_ble.cpp` (the shared parts are in `device_common.h`). Merge them after Phase 1 passes. | Implemented. Compile-checked. |
| D-33 | **Frame format** (version, type, sequence, length, payload, CRC-16/CCITT-FALSE): the BLE link layer has its own CRC but cannot catch a write that was cut short by a small MTU, a bug in the phone app, or a wrong-version peer. A 2-byte application CRC, version and length check make every one of those a clean rejection, and make the codec exhaustively testable. Maximum frame 64 bytes. | Implemented, host-tested |
| D-34 | **A frame that is not fully valid never changes any state** (CRC is checked before the contents are interpreted; the checks have a fixed, documented order). Enforced by tests, including fuzzing. | Implemented, host-tested |
| D-35 | **Sequence rules.** One counter per connection. Modulo-256 arithmetic: the same number is a duplicate (ignored); 1 to 127 ahead is accepted (lost frames are fine); 128 to 255 ahead is stale (rejected); the **third** stale frame in a row is accepted as a resync. The resync exists because a phone app that restarts its counter without reconnecting would otherwise be ignored for up to 128 frames. That scenario is an **assumption** about the future Android app, not an observed behaviour. | Implemented, host-tested |
| D-36 | **One connection at a time** (`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`, checked at compile time). Two phones writing would fight over the display. | Implemented, compile-checked |
| D-37 | **No pairing, bonding or encryption in Phase 2.** Anyone in range can connect and write navigation data. This is accepted for a prototype and recorded as a **security debt that must be paid before any commercial release** (BLUETOOTH_PROTOCOL.md section 11). | Decided (deferred) |
| D-38 | The phone must negotiate an **ATT MTU of at least 56** (53 plus 3 bytes overhead). The firmware requests 247 and logs the negotiated value, with a warning if it is too small. A cut-off frame is rejected safely. Writes with response are recommended; writes without response are accepted. | Implemented. Radio behaviour **not device-tested.** |
| D-39 | **The protocol has two independent implementations from day one:** the firmware's C++ and a Python reference (`protocol/frame_tool.py`), checked against shared vectors (`protocol/vectors.txt`) and against each other on random frames. The Android app must pass the same vectors. Reason: a protocol written and tested by only one implementation can agree with itself and still be wrong. | Implemented, host-tested |
| D-40 | **Proving Phase 1 is unchanged.** A file hash cannot, because an ESP32 image embeds its build time. Instead the current `motonav` image is compared byte by byte with an image built from the Phase 1 baseline commit `3eba30f`: 66 of 384,496 bytes differ, all in the ELF hash, the build-time string and the trailing checksum. | Done once, documented in TESTING.md section 5.3. Not automated. |
| D-41 | **Phase 2 was prepared before the Phase 1 hardware validation**, at the project owner's request to move forward ("we can test this in real hardware, but not now"). The B tests are gated on T1 to T4 passing, because a Bluetooth test on an unverified board proves nothing. | Decided |
| D-42 | **The Android project has three Gradle modules.** `:protocol` and `:link` are pure Kotlin (no Android); `:app` is the thin Android adapter. All Bluetooth operations go through a small interface (`GattPort`), so the connection logic is tested on a computer with a fake instead of being left to a phone. | Implemented. `:protocol` and `:link` host-tested; `:app` compile-checked. |
| D-43 | **The app has no third-party dependencies and uses the plain Android framework for its test screen** (no AndroidX, Material or Compose). Reason: it is a small test tool, fewer dependencies means a smaller supply chain and a build that cannot break on a library update. Revisit with the product UI (Phase 5). A test fails if a dependency is added. | Implemented |
| D-44 | **Android toolchain pins:** Android Gradle Plugin 9.5.1, Gradle 9.6.0, Kotlin 2.2.10 (what AGP 9.5.1 itself depends on), JUnit 6.1.3, compileSdk and targetSdk 37, Build Tools 36.0.0, JDK 17 or newer. AGP 9.5 was chosen because it supports API 37, which the Navigation SDK needs (Phase 4); AGP 9.0 supports only up to API 36.1. **minSdk 26 (Android 8.0) is an assumption, not a measurement of the target riders' phones.** The Navigation SDK's documented minimum is 24. | Implemented, compile-checked. minSdk **open to change.** |
| D-45 | **`applicationId` `com.motonav.app` and the launcher icon are placeholders.** An application id cannot be changed once the app is published, so it must be chosen deliberately before the first release. | **Open** |
| D-46 | **Navigation is state, not events: the newest update wins.** If updates arrive while a write is in flight, only the newest is sent next; the older ones are dropped and use up no sequence number. | Implemented, host-tested |
| D-47 | **Failures are loud and nothing retries silently.** A refused or too-small MTU, a missing service, a failed notification set-up, a refused operation, a lost connection or a timeout all end in a `Failed` state with a kind and a reason that the screen shows. Reconnection is Phase 5. | Implemented, host-tested |
| D-48 | **The phone refuses to send at an MTU below 56**, and treats a failure to turn on STATUS notifications as a failure of the link (a half-working link would hide problems at this stage). | Implemented, host-tested |
| D-49 | **The deprecated `connectGatt(Context, Boolean, Callback, Int)` is used**, with an explicit suppression. The API 37 SDK marks it deprecated in favour of a call that takes `BluetoothGattConnectionSettings`, which exists only on the newest Android; the app supports Android 8 and later. Moving to the new call where available is a later task. | Implemented, compile-checked |
| D-50 | **After every (re)connection, once set-up is complete, the phone sends the newest state again.** The firmware may discard a write that arrives while the connection is being set up (BLUETOOTH_PROTOCOL.md section 3). | Implemented, host-tested |
| D-51 | **The Gradle wrapper is pinned with a SHA-256**, taken from Gradle's published checksums page and equal to the checksum of the distribution downloaded for the build. The wrapper JAR also matches the published checksum. | Implemented |
| D-52 | **Road names are kept out of logs and storage on Android** (extends D-05): no `android.util.Log`, files, preferences or database in the sources (checked by a test), `NavUpdate.toString()` omits the road name, and backups of app data are disabled. | Implemented, host-tested |

## Rationale for the proposed items

**D-07 Arduino framework.** The prototype needs a display, a BLE service and a
simple loop. Arduino on PlatformIO is the shortest route to that, with the
widest library support, and it works across ESP32 variants. ESP-IDF gives finer
control that this prototype does not need. It can be reconsidered in Phase 6
when the production microcontroller is chosen, and the layering in D-10 keeps
that migration cheap. The exact PlatformIO platform and Arduino core versions
are checked and pinned in Phase 1.

**D-08 / D-09 Drawing.** The new circular display will need a different driver
but the drawing calls (lines, triangles, text) are of the same shape across the
common embedded graphics libraries. Vector arrows scale to any resolution and
cost no flash for bitmaps. Caveat to check in Phase 6: confirm the chosen
AMOLED's driver library exposes a compatible API. If not, only the thin
drawing adapter changes.

**D-11 NimBLE.** Smaller RAM and flash footprint than the default Bluedroid BLE
stack in Arduino-ESP32, and it supports the BLE-only chips. Phase 2 confirmed that
NimBLE-Arduino 2.5.2 **compiles** on the pinned core for the identified board target. How it
behaves on the real board (advertising, connecting, MTU, reconnecting, memory use) is **not
validated**: that is what tests B1 to B7 are for.

**D-13 Isolating Google types.** `Maneuver`, `NavInfo`, `StepInfo` and
`NavState` are labelled beta / preview in Google's reference with no
backward-compatibility guarantee.
