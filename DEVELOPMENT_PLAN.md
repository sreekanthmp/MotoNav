# Development Plan

Status vocabulary: **Implemented**, **Tested**, **Partially implemented**,
**Planned**, **Blocked**. Last updated after the hardware was provisionally identified (2026-11-08).

Rule: each phase ends at a review. The next phase does not start until the
project owner approves.

## Phase status

| Phase | Status | Notes |
|---|---|---|
| 0 — Discovery and planning | Implemented (documents only) | Approved. |
| 1 — ESP32 display prototype | Implemented and host-tested. **Not device-tested.** | Approved by the project owner, including the hardware diagnostics and the validation procedure. **Physical hardware validation (T1-T6) is the current stage and has not been run.** See [Phase 1 results](#phase-1-results). The hardware is identified only provisionally (from product listings) and no validation has been run, so nothing has run on hardware. |
| 2 — Bluetooth communication | Planned | **Not started.** Begins only after the hardware validation succeeds and the owner explicitly approves. First target: Android phone, then BLE, then ESP32, then SSD1306, using simulated navigation messages (not the Google Navigation SDK). |
| 3 — Android app foundation | Planned | |
| 4 — Google Navigation SDK | Planned | Commercial-terms questions open. |
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
   "runs in the background by default". This is the highest-value technical
   spike and should happen at the start of Phase 4, not Phase 5.
7. **Arrival signalling.** The SDK has no `ARRIVED` nav state. Arrival must be
   derived from the `DESTINATION*` maneuver and/or a separate event that has not
   been verified.
8. **BLE payload size.** The default ATT MTU leaves 20 usable bytes. Road names
   and a full snapshot may not fit in one write unless the MTU is negotiated or
   the protocol sends changed fields only. Decide in Phase 2.
9. **BLE on Android.** Behaviour differs by phone vendor (background limits,
   Android 12+ Bluetooth permissions). Needs on-device testing across at least
   two phones.
10. **Left/right U-turn and roundabout direction semantics** are inconsistent in
    Google's own text. Must be tested on the road or with a simulated route.

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
    Adafruit BusIO 1.17.4. Phase 2 must check that the chosen BLE library works
    with this framework version before relying on it.
16. **Android toolchain.** SDK 8.x needs Android Gradle Plugin 9.0.1, Gradle
    9.1.0 and `targetSdk` 37 (Verified in the release notes). The cloud
    environment has Gradle 8.14.3 and JDK 21 and no Android SDK, so builds and
    tests of the Android app there depend on network access that is not yet
    granted. Phase 3 concern.

17. **Road names in non-Latin scripts cannot be shown** (found in Phase 1). The
    built-in 5x7 font is ASCII only, so such names become `?` or are hidden. Google
    returns localised road names, so for a rider in India this will be common. This is a
    font and display decision for Phase 6, but it should be known before choosing the
    production display.
18. **A display unplugged while running is not detected.** The Adafruit driver does not
    report I2C write errors. Not a Phase 1 requirement; relevant to Phase 5.

### D. Tooling

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

Phase 1 stops there. Phase 2 starts only after review.

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
2. **Remaining distance and ETA are not in the model yet.** They are added with the
   protocol in Phase 2.
3. **Screens draw through a small `Canvas` interface** rather than calling Adafruit
   GFX directly (refines D-08). This is what makes the host tests and the later
   AMOLED swap possible.
4. **The host test harness is a plain Makefile**, not PlatformIO's native test
   environment (D-18).

## Later phases (summary, not yet planned in detail)

- **Phase 2:** versioned BLE GATT protocol and `BLUETOOTH_PROTOCOL.md`;
  firmware receives, validates and displays messages; tested with a generic BLE
  tool before any Android code exists.
- **Phase 3:** Kotlin app with scan, connect, persist the paired device, send
  test navigation messages.
- **Phase 4:** Navigation SDK integration, starting with the headless/background
  spike (risk 6).
- **Phase 5:** reliability: reconnection, stale-data detection, interruption
  handling, real-journey testing.
- **Phase 6:** production hardware investigation.
