# Development Plan

Status vocabulary: **Implemented**, **Tested**, **Partially implemented**,
**Planned**, **Blocked**. Last updated after Phase 4a (Google Navigation SDK conversion) was prepared (2026-11-08).

Rule: each phase ends at a review. The next phase does not start until the
project owner approves.

## Phase status

| Phase | Status | Notes |
|---|---|---|
| 0 — Discovery and planning | Implemented (documents only) | Approved. |
| 1 — ESP32 display prototype | Implemented and host-tested. **Not device-tested.** | Approved by the project owner, including the hardware diagnostics and the validation procedure. **Physical hardware validation (T1-T6) is the current stage and has not been run.** See [Phase 1 results](#phase-1-results). The hardware is identified only provisionally (from product listings) and no validation has been run, so nothing has run on hardware. |
| 2 — Bluetooth communication (firmware side) | Protocol, codec and receiver **implemented and host-tested**. Radio code **compile-checked only**. **Not device-tested: nothing has run over a real Bluetooth link.** | Prepared at the owner's request ("we can test this in real hardware, but not now") while the Phase 1 hardware validation is pending. Separate environment `motonav_ble`; Phase 1 `motonav` unchanged. Messages are simulated (sent by hand from a generic BLE app); no Google SDK. See [Phase 2](#phase-2--bluetooth-le-link-firmware-side). **Awaiting review.** The device tests B1-B7 should wait until T1-T4 have passed. |
| 3 — Android app foundation (test sender) | Protocol code and connection logic **implemented and host-tested**. The Android app is **compile-checked only**. **Not device-tested: it has never run on a phone, and never against a MotoNav.** | Prepared at the owner's request while the hardware validation is pending. Sends the Phase 2 simulated messages; not the navigation app and no Google SDK. See [Phase 3](#phase-3--android-test-sender). **Awaiting review.** The phone tests P1-P8 should wait until B1 and B2 have passed. |
| 4 — Google Navigation SDK | **4a** (everything that needs no account): the feed conversion is **implemented and host-tested**; the SDK glue is **compile-checked only**. **The Google parts have never run: no key, no account, no phone.** **4b** (tests N1-N7) is **not started** and needs you. | Prepared at the owner's "go". See [Phase 4a](#phase-4a--google-navigation-sdk-everything-that-needs-no-account). Commercial-terms questions are still open. **Awaiting review.** |
| 5 — Navigation reliability | Planned | |
| 6 — Production hardware investigation | Planned | Not started on purpose. |

## Phase 0 findings

- The repository was empty: no commits locally or on the GitHub remote, no
  files, no existing code or dependencies to preserve.
- No hardware was reachable, so the ESP32 model and display configuration were
  **not identified** at the time. (Since then the project owner has supplied
  *provisional* details from product listings; see [HARDWARE.md](HARDWARE.md).)
- Google's TurnByTurn feed is documented for exactly this kind of device, with
  two-wheeled vehicles named as an example. Details and sources are in
  [ARCHITECTURE.md](ARCHITECTURE.md).
- The commercial terms are not fully resolved. See the risks below.

## Technical risks and unknowns

Ordered by how badly they could change the plan.

### A. Commercial and licensing (could block the product)

1. **Service Specific Terms were not read.** The legally operative terms live in
   the Google Maps Platform Service Specific Terms and Terms of Service on
   `cloud.google.com`. That site could not be read in the Phase 0 environment
   (only page titles were returned). The Navigation SDK *policies* page was
   read in full and contains no ban on hardware displays or on motorcycles.
   During Phase 0 an automated search summary claimed the policies page bans
   "embedded devices"; that text is **not on the page**, so it is not relied on
   either way. Someone must read the actual terms, and a lawyer should review
   them before any commercial commitment.
2. **Attribution on a hardware display.** The policies page requires "Google
   Maps" attribution (logo, or Roboto 12-16sp text) when displaying Maps
   Platform content. It does not say whether a separate device's screen
   counts, and a 128x64 OLED cannot meet the stated sizes.
3. **Safety requirements in the EEA.** The setup page says the SDK's *default*
   UI is deemed compliant with the EEA Safety Requirements. A custom
   second-screen display is not the default UI. Whether it needs its own
   assessment is an open question (this is an inference, not a statement from
   Google).
4. **Beta APIs.** `Maneuver`, `NavInfo`, `StepInfo` and `NavState` are marked
   beta / preview with no backward-compatibility guarantee. The Android app
   must isolate Google types in one file so a breaking change costs one file.
5. **Price per destination** is on a separate pricing-list page that was not
   read. The billing model itself (per destination requested; first 1,000 per
   month free) is verified.

Recommended action: contact Google Maps Platform sales (the docs have a
"Contact sales" link) with three precise questions: is a Bluetooth-paired
second-screen device a supported use of the TurnByTurn feed, how must
attribution work on it, and what are the terms for commercial distribution.
This does not block Phases 1-3. It should be answered before Phase 4 spending
and before any manufacturing commitment.

### B. Technical unknowns

6. **Headless and background operation.** Not verified whether the Navigator
   can run without a visible navigation view, or keep delivering the feed with
   the screen off and the phone in a pocket (the normal riding case).
   Partial evidence only: the release notes mention an SDK-owned
   foreground `NavigationService`, and the FAQ says the location provider
   "runs in the background by default". The turn-by-turn guide adds that the receiving
   service "can live in a separate background process". **Phase 4a could not run this
   spike** (it needs a phone and a key): it is test N5 in TESTING.md section 7, and the
   highest-value unknown that remains. The test screen holds the connection, so it is
   expected to stop when the app is closed.
7. **Arrival signalling.** The SDK has no `ARRIVED` nav state. **Resolved in design in Phase 4a:**
   the SDK has a real event, `Navigator.ArrivalListener.onArrival(ArrivalEvent)` with
   `isFinalDestination()` (read from the SDK's class files and its guide), and the converter uses
   it instead of guessing from distances (D-55). Whether the event arrives as documented is unverified (N3).
8. **BLE payload size.** The default ATT MTU leaves 20 usable bytes. Road names
   and a full snapshot may not fit in one write unless the MTU is negotiated or
   the protocol sends changed fields only. **Decided in Phase 2:** every message is a full
   state of at most 53 bytes, so the phone must negotiate an MTU of at least 56
   (BLUETOOTH_PROTOCOL.md section 3). Whether a real phone does so is untested.
9. **BLE on Android.** Behaviour differs by phone vendor (background limits,
   Android 12+ Bluetooth permissions). Needs on-device testing across at least
   two phones.
10. **Left/right U-turn and roundabout direction semantics** are inconsistent in
    Google's own text. **Partly resolved in Phase 4a:** Google defines the U-turn maneuvers as
    a "clockwise" or "counterclockwise turn onto the opposite side of the street" and picks the
    label from the driving side, so clockwise is the right-hand icon (D-58). The guide's one
    contradictory sentence is a typo. Not yet confirmed on a real road (N4).

### C. Hardware and display

11. **Hardware identified only from product listings.** The chip, display size, address
    and wiring are unverified. Nothing can be **device-tested** (criteria 2, 4,
    5-device, 6-device, 7) until the validation tests T1 to T3 pass and the board is flashed.
    The firmware refuses to build for a non-classic chip and the diagnostics report the
    real chip, so a wrong assumption is caught early rather than silently.
12. **"SSD1306" may actually be SH1106.** Phase 1 only drives an SSD1306. A mismatch shows
    as a missing left edge and a speckled right edge. `oled_check` tests for it (T3). If it
    is an SH1106, one display adapter has to be written; the screens do not change.
13. **Readability.** A small monochrome OLED is hard to read in direct sunlight,
    and static content can cause burn-in over long periods. This affects the
    prototype demo only. The production display is a separate decision.
14. **Hardware testing cannot be done by the AI assistant.** It has no access to
    the board. Anything that depends on the physical display, flicker, BLE
    radio behaviour or long-run stability must be tested by the owner, and
    results recorded honestly in `TESTING.md` as device-tested vs host-tested.
15. **ESP32 Arduino core and PlatformIO platform versions** change quickly.
    Pinned in Phase 1 to what the compile check resolved on 2026-11-08: platform
    `espressif32@8.0.0`, package `framework-arduinoespressif32@4.30312.261024`
    (with `esp32-libs@3.3.12`), Adafruit GFX 1.13.0, Adafruit SSD1306 2.5.17,
    Adafruit BusIO 1.17.4. Phase 2 checked that NimBLE-Arduino 2.5.2 builds with this
    framework version (it does; pinned in `platformio.ini`). It has not run.
16. **Android toolchain.** Navigation SDK 8.x needs Android Gradle Plugin 9.0.1 or newer,
    Gradle 9.1.0 and `targetSdk` 37 (Verified in the release notes). Resolved in Phase 3:
    the project uses AGP 9.5.1 (supports API 37; AGP 9.0 stops at API 36.1), Gradle 9.6.0,
    Kotlin 2.2.10, compileSdk and targetSdk 37, and builds. Building it in the cloud sandbox
    needed `dl.google.com` to be allowed and the Android SDK licence to be accepted there.
    Phase 4 must still check that Navigation SDK 8.1.0 builds with these versions.

17. **Road names in non-Latin scripts cannot be shown** (found in Phase 1). The
    built-in 5x7 font is ASCII only, so such names become `?` or are hidden. Google
    returns localised road names, so for a rider in India this will be common. This is a
    font and display decision for Phase 6, but it should be known before choosing the
    production display.
18. **A display unplugged while running is not detected.** The Adafruit driver does not
    report I2C write errors. Not a Phase 1 requirement; relevant to Phase 5.

### D. Tooling

### E. Bluetooth (found in Phase 2)

19. **The radio code has never run.** Advertising, connecting, MTU negotiation, write
    callbacks, notifications and reconnecting are all untested. The code is thin and the
    decisions are in tested logic, but only tests B1 to B7 can show it works.
20. **No security.** No pairing or encryption: anyone in range can connect and write
    navigation data. Acceptable for a prototype, **not for a product** (D-37).
21. **Memory cost of the Bluetooth stack is unknown.** Flash use is 67 % of the application
    partition (881,160 B of 1,310,720 B); free heap at run time is measured only by B7.
22. **The sequence-number resync is built for an app behaviour that is only assumed**
    (an app restarting its counter without reconnecting). Revisit when the Android app
    exists (D-35).
23. **Android behaves differently across phone makers** (MTU negotiation, connection
    parameters, background limits). Needs at least two phones in Phase 3 and Phase 5.
24. **Two firmware entry points** (`main.cpp`, `main_ble.cpp`) duplicate about 100 lines.
    Deliberate, to keep the Phase 1 validation valid (D-32). Merge after Phase 1 passes.
25. **No stale-data detection.** If the phone stops sending, the display keeps the last
    state. Belongs to Phase 5 (heartbeat and timeout).

### F. Android (found in Phase 3)

26. **The Android app has never run on a phone.** Scanning, permissions, connecting, MTU
    negotiation, writes, notifications and the screen are untested (tests P1 to P8). The
    logic they feed is tested.
27. **`connectGatt` is deprecated in the API 37 SDK** in favour of a call that exists only on the
    newest Android (D-49). It works across Android 8 and later today; revisit when the newest
    Android is common.
28. **The connection lives and dies with the test screen.** Closing the app ends it. Keeping the
    link alive with the screen off or the app in the background needs a different design, and
    Android's background-Bluetooth rules vary by version and maker. Not yet researched; Phase 5.
29. **`minSdk` 26, `applicationId` and the icon are placeholders** (D-44, D-45).
30. **One phone proves little.** Android Bluetooth behaviour (MTU, timing, permissions) differs by
    maker. P8 asks for a second phone; a real compatibility matrix is Phase 5.
31. **The build environment was set up in a sandbox** that needed an extra network host and an
    accepted SDK licence. A fresh developer machine needs JDK 17 or newer and the Android SDK
    (platform 37.0, build-tools 36.0.0); the first build downloads about 140 MB of Gradle and
    plugins.

### G. Google Navigation SDK (found in Phase 4a)

32. **None of the Google parts has run.** There is no API key, no Google account and no phone. Rate, content,
    arrival and background behaviour of the feed are documentation, not observation (tests N1 to N7).
33. **Cost.** The SDK is billed per destination (ARCHITECTURE.md section 2). What testing costs is unknown until N7.
    Commercial terms (section A) are still open, including attribution for a separate hardware display.
34. **The SDK merges broad permissions into the app**, including background location, notifications,
    storage, boot-completed and wake lock. They will matter for Google Play review and the Location Purpose Declaration.
    The SDK also sets `largeHeap`. Removing permissions may break the SDK, so this needs a deliberate review.
35. **Size.** The debug APK is about 65 MB with the SDK, against under 1 MB without it.
36. **Version combination.** Google's table for SDK 8.x names Gradle 9.1.0 and AGP 9.0.1; the project uses Gradle 9.6.0
    and AGP 9.5.1 (needed for API 37). It builds; whether Google has tested it is unknown.
37. **The SDK's data classes are beta** with no promise of compatibility. Mitigated by the single `:navsdk` module and by a
    test that fails if the SDK adds a maneuver the table does not know.
38. **Indian-script road names show nothing on the display** (shown concretely by a reviewed test case). For a rider in India this
    will be common. A product decision is needed: fall back to an English or transliterated name, or change the display font (Phase 6).
39. **If the SDK, the feed or the app is killed, the device keeps showing the last state.** There is no stale-data detection yet (Phase 5; risk 25).
40. **The SDK may store data of its own.** D-05 (no road names stored) is checked for our code only.

## Phase 1 — ESP32 display prototype

**Goal:** the ESP32 renders every MotoNav screen on the SSD1306 from simulated
data. No Bluetooth, no Android.

### Inputs needed from the project owner

**Received, provisionally.** Taken from product listings on 2026-11-08: classic ESP32
30-pin DevKit with CP2102, 0.96 inch 128x64 four-pin I2C OLED, expected SSD1306, wiring
SDA=GPIO21, SCL=GPIO22. These are now in `firmware/platformio.ini` and
[HARDWARE.md](HARDWARE.md), but **none is verified**, and the OLED's I2C address is still
unknown (0x3C is a guess). Verification is the hardware validation tests T1 to T6 in
[TESTING.md](TESTING.md), using the `hw_check` and `oled_check` diagnostics.

### Approach

*This is the plan as approved at the end of Phase 0. What was actually built, and
where it differs, is in [Phase 1 results](#phase-1-results) and
[Deviations](#deviations-from-the-approved-plan) below.*

1. **Scaffold** `firmware/` as a PlatformIO project for the identified board,
   plus a `native` environment that runs logic and rendering tests on a normal
   computer. Pin the platform and library versions after checking what is
   current.
2. **Model.** `nav_model.h` holds the snapshot and maneuver enums. It has no
   display or BLE includes. The Phase 1 screens consume this model, and Phase 2
   will fill the same model from BLE, so no rendering code changes then.
3. **Display.** Initialise via Adafruit GFX + SSD1306 (proposed, see
   [DECISIONS.md](DECISIONS.md)). Report an initialisation failure on serial
   and retry; never boot-loop. Pin numbers and the I2C address are defined in
   one place.
4. **Icons as vector drawing,** not stored bitmaps: arrows are built from
   lines and triangles, parameterised by centre and size. They scale to any
   resolution, so the later circular display reuses them.
5. **Screens as small functions:** `drawDisconnected`, `drawReady`,
   `drawNavigating`, `drawRerouting`, `drawArrived`. Each takes a snapshot and a
   drawing target. Layout is relative to the display size and a margin, not
   hard-coded to 128x64.
6. **No flicker.** Redraw and flush only when the snapshot differs from the
   last drawn one. No animation.
7. **Distance formatting** is a separate pure function with unit tests.
   Proposed rules: under 100 m in 10 m steps (`90 m`), 100-999 m in 50 m steps
   (`450 m`), 1 km to under 10 km with one decimal (`1.2 km`), above that whole
   km.
8. **Simulated data.** A scripted loop walks through: disconnected, ready,
   navigating (every maneuver, with distance counting down), rerouting,
   arrived, stopped, back to ready. A serial command can jump to a given screen
   for inspection.
9. **Host-side rendering check.** Render each screen to a 1-bit image on the
   build machine so arrows and layout can be reviewed and regression-tested
   without hardware. Feasibility depends on whether the drawing library builds
   on the host, which is checked first. If it does not, a small host shim is
   used.
10. **Soak test support.** Print uptime and free heap over serial periodically so
    a long run can be judged from a log.

### Out of scope for Phase 1

BLE, Android, protocol definition, Google SDK, power management, any hardware
beyond the existing board and display.

### Phase 1 acceptance criteria

Each criterion is marked with who can verify it. "Host" means verified by
automated tests / rendered images on a build machine. "Device" means it can only
be verified on the physical board by the owner.

| # | Criterion | Verified by |
|---|---|---|
| 1 | Firmware builds with no errors for the target board | Host |
| 2 | Display initialises on 20 consecutive power-ups and 20 consecutive resets, never blank or garbled. A failed init is reported on serial and recovers without a boot loop | Device |
| 3 | Disconnected, Ready, Navigating, Rerouting and Arrived screens each render as specified | Host (images) + Device |
| 4 | All 10 maneuver arrows (straight, left, right, slight L/R, sharp L/R, U-turn, roundabout, arrived) are recognisable at a glance, judged from photos of the real display | Device |
| 5 | Distance is the largest text on the navigation screen and follows the formatting rules; unit tests cover boundaries (0, 99, 100, 999, 1000, 9999, 10000 m and unknown) | Host + Device |
| 6 | Flushes to the display occur only on snapshot change: a test replays a sequence and counts flushes. No visible flicker on the device | Host + Device |
| 7 | The simulated loop runs for at least 12 hours with no reset and no downward trend in free heap, from the serial log | Device |
| 8 | `TESTING.md` records what was run, on what, with actual results, marking each result Host or Device | — |

Phase 1 stops there. (At the owner's request, Phase 2 was then prepared before the hardware validation; see [Phase 2](#phase-2--bluetooth-le-link-firmware-side).)

### Phase 1 results

Details and procedures are in [TESTING.md](TESTING.md). "Host" = automated tests and
rendered images on a normal computer. "Device" = the real board, **not done**.

| # | Criterion | Host result | Device result |
|---|---|---|---|
| 1 | Firmware builds for the target board | **Compile-checked** (not run) for `esp32doit-devkit-v1`, the provisional target: the firmware and two diagnostics, 0 warnings. Building for any other chip is refused. The real board is not yet verified. | n/a |
| 2 | Display initialises reliably; failed init recovers | Logic written (probe, retry every 2 s, no reboot loop, hint if the OLED is at the other address). **Not host-testable.** | **Not run.** Needs the board (T4.1, T4.2). |
| 3 | All five screens render as specified | **Pass**: every screen at 128x64 and 128x32, bounds-checked, images reviewed | **Not run** (T4.3) |
| 4 | All 10 arrows recognisable | Images reviewed by the AI assistant and judged recognisable after two fixes. All 11 icons pairwise different, left/right mirrored. This is a judgement on a bitmap, not on the panel. | **Not run** (T4.4) |
| 5 | Distance is the largest text; formatting rules unit-tested at the boundaries | **Pass**: 33 boundary cases, plus a property check over 0-30,000 m; the `d` serial command steps through every rule | **Not run** (T4.5, T4.6) |
| 6 | Flushes only on change | **Pass**: 604 frames for 1,720 ticks over two script loops, equal to the independently counted number of visible changes | **Not run** (T4.7) |
| 7 | 12-hour soak, no reset, stable heap | The firmware logs a stats line a minute (heap, largest block, stack, frames, slowest display update, render mismatches) and the reset reason. A log analyser judges it against fixed criteria. **Host-tested** against synthetic logs built with the firmware's own formatters. The soak itself is not host-testable. | **Not run.** 30 minutes first (T5), then 12 hours (T6). |
| 8 | `TESTING.md` records results, marking Host vs Device | **Done** | — |

**Phase 1 cannot be called complete until the hardware validation tests T1 to T6 are run on
the real board** (T6, the 12-hour soak, is the extended reliability test; T5, 30 minutes, is
the first gate). What is done is everything that can be proven without hardware.

### Deviations from the approved plan

1. **U-turns have a side** (`UTurnLeft`, `UTurnRight`) in the internal model instead of
   the brief's single `U_TURN`. Needs your approval (D-17 in [DECISIONS.md](DECISIONS.md)).
2. **Remaining distance and remaining time** were not in the Phase 1 model. Phase 2 carries
   them in the protocol (as `NavExtras`) but the display does not show them (D-21).
3. **Screens draw through a small `Canvas` interface** rather than calling Adafruit
   GFX directly (refines D-08). This is what makes the host tests and the later
   AMOLED swap possible.
4. **The host test harness is a plain Makefile**, not PlatformIO's native test
   environment (D-18).

## Phase 2 — Bluetooth LE link (firmware side)

**Goal:** the ESP32 receives navigation messages over Bluetooth LE and shows them on the
SSD1306, with the phone replaced by a generic BLE app sending simulated messages. No Android
code and no Google Navigation SDK.

**How it was done.** Prepared at the owner's request while the Phase 1 hardware validation
(T1-T6) is still pending. To keep that validation valid, Phase 1's `motonav` environment is
untouched (shown by a byte comparison against commit `3eba30f`) and Phase 2 is the new
environment `motonav_ble` (D-32).

**Delivered:**

- [BLUETOOTH_PROTOCOL.md](BLUETOOTH_PROTOCOL.md): GATT layout, frame format, validation order,
  sequence rules, STATUS characteristic, versioning, limitations.
- `firmware/lib/motonav_protocol`: CRC, frame codec and receiver as pure C++ (D-33 to D-35).
- `firmware/src/ble_link.*` and `main_ble.cpp`: NimBLE glue and the Phase 2 firmware.
- `protocol/frame_tool.py` and `protocol/vectors.txt`: an independent Python implementation, and
  45 shared test vectors (D-39).
- Host tests: 24 new C++ tests (77 in total), including fuzzing; new Python tests (93 in total after Phase 3).
- [TESTING.md](TESTING.md) section 5: ready-to-send test messages with the predicted serial
  output and STATUS bytes, and the device tests B1 to B7.

### Phase 2 acceptance criteria and results

| # | Criterion | Verified by | Result |
|---|---|---|---|
| 1 | The protocol is specified in `BLUETOOTH_PROTOCOL.md` | Document | **Done** |
| 2 | Every class of invalid frame is rejected (too short, wrong version, wrong length, bad CRC, unknown type, bad payload) and every valid frame accepted | Host | **Pass** (45 shared vectors, round trip, bit flips, truncation, 500,000 fuzz frames) |
| 3 | A rejected, stale or duplicate frame never changes what the display shows | Host | **Pass** (also fuzzed) |
| 4 | Sequence rules: duplicates, stale frames, wrap-around, resync, reset on connect | Host | **Pass** |
| 5 | Received bytes produce the expected screen for every mode and all 11 maneuvers | Host | **Pass** (through the real `describeView`) |
| 6 | The firmware codec and an independent implementation agree | Host | **Pass** (45 vectors, 3,000 random frames, the hand-test messages) |
| 7 | `motonav_ble` builds with no warnings; only one connection is possible | Compile | **Pass** (881,160 B flash, 38,228 B static RAM, 0 warnings) |
| 8 | The Phase 1 firmware is unchanged | Compile (byte comparison) | **Pass** (66 of 384,496 bytes differ, all timestamp, ELF hash and checksum) |
| 9 | The device advertises, accepts a connection and negotiates an MTU of at least 56 | Device | **NOT RUN** (B1, B2) |
| 10 | Messages written from a generic BLE app update the display; invalid ones are rejected without a change | Device | **NOT RUN** (B3, B4) |
| 11 | Disconnecting and reconnecting works without a reset | Device | **NOT RUN** (B5) |
| 12 | The sequence rules behave as specified on the device | Device | **NOT RUN** (B6) |
| 13 | Ten minutes connected with no reset; heap use recorded | Device | **NOT RUN** (B7) |

**Phase 2 cannot be called complete until B1 to B7 are run on a real board with a real phone.**
Everything that can be proven without a radio is done.

Phase 2 stops here. Phase 3 (Android) starts only after review.

## Phase 3 — Android test sender

**Goal:** an Android app that finds a MotoNav over Bluetooth LE, connects, negotiates the message
size and sends the simulated navigation messages from Phase 2, so that the complete chain
phone, Bluetooth, ESP32, display can be tested before any Google SDK work.

**How it was done.** Prepared at the owner's request while the hardware validation is pending.
No phone and no MotoNav were available, so the work was arranged so that as much as possible can
be proven on a computer: everything that decides what to do is pure Kotlin behind a small
interface, tested with a fake. The Android-specific part is thin, and was built and linted but
not run (D-42).

**Delivered:** the `android/` Gradle project ([android/README.md](android/README.md)) with
`:protocol` (the Kotlin implementation of BLUETOOTH_PROTOCOL.md, checked against the shared
vectors and against the Python tool's messages), `:link` (the connection state machine) and
`:app` (BluetoothGatt adapter, scanner and one test screen); `protocol/scenarios.txt`; source
checks for the road-name rule and for dependencies; and [TESTING.md](TESTING.md) section 6 with
the phone tests P1 to P8 and the board output each should produce.

### Phase 3 acceptance criteria and results

| # | Criterion | Verified by | Result |
|---|---|---|---|
| 1 | The Kotlin protocol code decodes all shared vectors as stated and reproduces the valid ones byte for byte | Host | **Pass** (106 tests) |
| 2 | Its simulated messages are identical to the Python tool's | Host | **Pass** |
| 3 | The connection logic sets up in the documented order, refuses an MTU below 56, never writes before Ready or two at once, numbers frames per connection, keeps only the newest update, resends after reconnecting, times out, and reports every failure | Host, with a fake Bluetooth | **Pass** (32 tests incl. 40 randomised runs) |
| 4 | Road names are never logged or stored | Host (source checks) | **Pass** |
| 5 | The app builds with no compiler warnings and no lint findings; the packaged manifest has the documented permissions | Compile | **Pass** |
| 6 | The app starts, asks for permission, finds a MotoNav, connects and reaches Ready with an MTU of at least 56 | Device | **NOT RUN** (P1 to P3) |
| 7 | Each simulated state appears on the display as the board's `expect:` line says; wrong messages are rejected | Device | **NOT RUN** (P4 to P6) |
| 8 | Disconnecting, reconnecting and connection loss behave as specified; a second phone behaves the same | Device | **NOT RUN** (P7, P8) |

**Phase 3 cannot be called complete until P1 to P8 are run on a real phone against a real board.**

Phase 3 stops here. Phase 4 (Google Navigation SDK) starts only after review.

## Phase 4a — Google Navigation SDK (everything that needs no account)

**Goal:** turn Google's turn-by-turn feed into the messages the device already understands, as far as possible
without a Google account, an API key, billing or a phone. "4b" is the rest, which only the owner can do (tests N1 to N7).

**How it was done.** The Navigation SDK 8.1.0 package was downloaded and its class files were read, together with Google's
reference pages and guides, so the code is written against the real API. The conversion is pure and uses the SDK's own
data classes in tests. Everything that talks to Google's services is compile-checked only.

**Delivered:** the `:navsdk` module ([android/README.md](android/README.md)): `ManeuverMap` (all 66 Google maneuvers), `NavInfoConverter`,
`NavFeed`, `NavUpdatesService` and `NavigationController`; the test screen's "Real navigation" section (destination, optional simulated
start, simulated drive, feed counters); `protocol/navinfo_cases.txt` and a Python replay through the firmware's real receiver; source checks (Google
types only in `:navsdk`, no API key committed, fine location on every Android version); and [TESTING.md](TESTING.md) section 7 with the
tests N1 to N7.

### Phase 4a acceptance criteria and results

| # | Criterion | Verified by | Result |
|---|---|---|---|
| 1 | Every Google maneuver maps to a device icon, and an SDK update that adds one is noticed | Host (reads the constants from the SDK) | **Pass** (66 of 66) |
| 2 | Feed messages become device messages correctly: states, missing and negative numbers, road names, roundabout exits, arrival | Host, with the SDK's own data classes | **Pass** (27 tests incl. 20,000 random messages) |
| 3 | 17 reviewed feed messages produce the expected screens on the firmware's real receiver | Host | **Pass** |
| 4 | Google types appear only in `:navsdk`; no API key is committed; fine location is declared on every Android version | Host (source checks) | **Pass** |
| 5 | The app builds against the real SDK with no compiler warnings and no lint findings | Compile | **Pass** |
| 6 | The SDK starts with a valid key, shows Google's terms, and accepts a two-wheeler route | Device | **NOT RUN** (N1, N2) |
| 7 | The feed arrives about once a second and the display follows a route, including arrival | Device | **NOT RUN** (N2, N3) |
| 8 | Icons are right on a U-turn in both traffic sides | Device | **NOT RUN** (N4) |
| 9 | The feed and the link survive the screen being off | Device | **NOT RUN** (N5) |
| 10 | A short real ride; and what testing costs | Device | **NOT RUN** (N6, N7) |

**Phase 4 cannot be called complete until N1 to N7 are run.** Phase 4a stops here; Phase 5 (reliability) starts only after review.

### Deviations from the plan

1. **The headless/background spike did not come first**, as the plan said it should (risk 6). It needs a phone and a key, so it is test N5.
2. **Arrival uses the SDK's event** rather than the `DESTINATION*` maneuver (risk 7).

## Later phases (summary, not yet planned in detail)
- **Phase 3 (prepared, see above):** Kotlin test sender: scan, connect, request the MTU, send
  simulated navigation messages. Passes `protocol/vectors.txt`. Remembering the paired device
  was left out: it belongs with reconnection in Phase 5.
- **Phase 4 (4a prepared, see above):** Navigation SDK integration. 4b is the owner's part: key, billing, terms, and
  the tests N1 to N7, starting with the headless/background question (risk 6, test N5).
- **Phase 5:** reliability: reconnection, stale-data detection, interruption
  handling, real-journey testing.
- **Phase 6:** production hardware investigation.
