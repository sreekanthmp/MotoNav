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
| D-08 | Device display uses Adafruit GFX + SSD1306. **Refined in Phase 1:** screens do not call Adafruit GFX directly. They draw through an eight-method `Canvas` interface, and `Ssd1306Canvas` adapts it. Reason: the host tests need a drawing target that builds without Arduino, and a round AMOLED later needs only a new adapter. | Implemented. Interface host-tested. SSD1306 adapter compile-checked (ESP32, S3, C3). Not device-tested. |
| D-09 | Arrow and icon graphics are vector-drawn, not stored bitmaps. | Implemented, host-tested |
| D-10 | `DeviceState` is the only thing screens read; `ScreenView` is what is visible; screens know nothing of BLE. | Implemented, host-tested |
| D-11 | BLE stack on the ESP32: NimBLE. | Proposed (to validate in Phase 2) |
| D-12 | Android BLE uses the platform APIs directly, no wrapper library. | Proposed (revisit in Phase 3 if it proves painful) |
| D-13 | Google types are confined to one Android file/module because the APIs are beta. | Proposed |
| D-14 | Message encoding: compact binary versus JSON. | Open (Phase 2) |
| D-15 | Where ETA is computed. The SDK provides remaining seconds, not a clock time, and the ESP32 has no clock. Leaning towards phone-computed. | Open (Phase 2) |
| D-16 | `BLUETOOTH_PROTOCOL.md` is created when Phase 2 starts; `TESTING.md` was created in Phase 1. | Implemented for `TESTING.md` |
| D-17 | The model has `UTurnLeft` and `UTurnRight`, not the single `U_TURN` of the original brief. Google distinguishes clockwise and counterclockwise U-turns and they mirror between left- and right-hand traffic. | **Needs your approval** (deviates from the brief). Implemented. |
| D-18 | Host tests use a plain Makefile and a 60-line test harness, not PlatformIO's `native` test environment. Reason: it needs only a C++ compiler, with no package downloads. | Implemented |
| D-19 | No default pins, I2C address or display size anywhere in the code. The build fails until `platformio.ini` supplies them. | Implemented (your guideline 6) |
| D-20 | Non-ASCII road names are shown as one `?` per run, and a name with no letters or digits left is hidden. The 5x7 font has ASCII only. **Limitation:** road names in Indian scripts are not readable on this display. Revisit with the production display and font. | Implemented, host-tested |
| D-21 | Remaining distance and ETA are not in the Phase 1 model. They are added with the protocol in Phase 2. | Implemented |
| D-22 | Distance text: under 100 m in 10 m steps, up to 974 m in 50 m steps, then km with one decimal, whole km from 9.95 km, capped at 999 km. The number and unit are separate so the number is drawn large. | Implemented, host-tested |
| D-23 | Phase 1 only supports I2C displays. | Implemented. SPI is added once the display is identified. |

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
stack in Arduino-ESP32, and it supports the BLE-only chips. This is a general
recommendation and is **not yet validated** against the identified board and
pinned core version.

**D-13 Isolating Google types.** `Maneuver`, `NavInfo`, `StepInfo` and
`NavState` are labelled beta / preview in Google's reference with no
backward-compatibility guarantee.
