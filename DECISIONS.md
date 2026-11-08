# Technical Decisions

Status values: **Given** (stated by the project owner), **Required** (forced by
a verified external rule), **Proposed** (recommended, awaiting approval),
**Open** (not decided).

Nothing here has been built or tested yet.

| ID | Decision | Status |
|---|---|---|
| D-01 | The phone is the only navigation processor. The ESP32 never routes or reads GPS. | Given |
| D-02 | Navigation comes from Google's official Navigation SDK TurnByTurn feed. Nothing is extracted from the Google Maps app. | Given |
| D-03 | Phone and device communicate over BLE GATT with a versioned protocol. | Given |
| D-04 | Android app in Kotlin; firmware in C++ with PlatformIO. | Given |
| D-05 | Road names are never persisted (no flash, logs, analytics or storage). | Required (Google policy: persisting road names is "scraping") |
| D-06 | No animations on the navigation screen. | Required for policy fit (Google flags animations during active navigation as unsafe) and chosen for readability |
| D-07 | ESP32 Arduino framework, not ESP-IDF. | Proposed |
| D-08 | Draw with an Adafruit GFX-style API (Adafruit GFX + SSD1306 driver). | Proposed |
| D-09 | Arrow and icon graphics are vector-drawn, not stored bitmaps. | Proposed |
| D-10 | `NavSnapshot` is the only thing screens read. Screens know nothing of BLE. | Proposed |
| D-11 | BLE stack on the ESP32: NimBLE. | Proposed (to validate in Phase 2) |
| D-12 | Android BLE uses the platform APIs directly, no wrapper library. | Proposed (revisit in Phase 3 if it proves painful) |
| D-13 | Google types are confined to one Android file/module because the APIs are beta. | Proposed |
| D-14 | Message encoding: compact binary versus JSON. | Open (Phase 2) |
| D-15 | Where ETA is computed. The SDK provides remaining seconds, not a clock time, and the ESP32 has no clock. Leaning towards phone-computed. | Open (Phase 2) |
| D-16 | Docs `BLUETOOTH_PROTOCOL.md` and `TESTING.md` are created when their phase starts. | Proposed |

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
