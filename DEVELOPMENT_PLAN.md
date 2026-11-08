# Development Plan

Status vocabulary: **Implemented**, **Tested**, **Partially implemented**,
**Planned**, **Blocked**. Last updated at the end of Phase 0 (2026-11-08).

Rule: each phase ends at a review. The next phase does not start until the
project owner approves.

## Phase status

| Phase | Status | Notes |
|---|---|---|
| 0 — Discovery and planning | Implemented (documents only) | Awaiting review. |
| 1 — ESP32 display prototype | Planned | Blocked on two inputs, see [Phase 1](#phase-1--esp32-display-prototype). |
| 2 — Bluetooth communication | Planned | |
| 3 — Android app foundation | Planned | |
| 4 — Google Navigation SDK | Planned | Commercial-terms questions open. |
| 5 — Navigation reliability | Planned | |
| 6 — Production hardware investigation | Planned | Not started on purpose. |

## Phase 0 findings

- The repository was empty: no commits locally or on the GitHub remote, no
  files, no existing code or dependencies to preserve.
- No hardware was reachable, so the ESP32 model and display configuration are
  **not identified**. See [HARDWARE.md](HARDWARE.md).
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

11. **Board and display not identified** (blocks Phase 1).
12. **"SSD1306" may actually be SH1106** (common on 1.3 inch modules).
13. **Readability.** A small monochrome OLED is hard to read in direct sunlight,
    and static content can cause burn-in over long periods. This affects the
    prototype demo only. The production display is a separate decision.

### D. Tooling

14. **Hardware testing cannot be done by the AI assistant.** It has no access to
    the board. Anything that depends on the physical display, flicker, BLE
    radio behaviour or long-run stability must be tested by the owner, and
    results recorded honestly in `TESTING.md` as device-tested vs host-tested.
15. **ESP32 Arduino core and PlatformIO platform versions** change quickly. The
    exact versions are verified and pinned at the start of Phase 1, not
    assumed from memory.
16. **Android toolchain.** SDK 8.x needs Android Gradle Plugin 9.0.1, Gradle
    9.1.0 and `targetSdk` 37 (Verified in the release notes). The cloud
    environment has Gradle 8.14.3 and JDK 21 and no Android SDK, so builds and
    tests of the Android app there depend on network access that is not yet
    granted. Phase 3 concern.

## Phase 1 — ESP32 display prototype

**Goal:** the ESP32 renders every MotoNav screen on the SSD1306 from simulated
data. No Bluetooth, no Android.

### Inputs needed from the project owner (blocking)

1. Exact ESP32 board and chip (commands in [HARDWARE.md](HARDWARE.md)).
2. Display: resolution (128x64 or 128x32), interface (I2C 4-pin or SPI 7-pin),
   and if possible the I2C address and how it is wired.

### Approach

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
