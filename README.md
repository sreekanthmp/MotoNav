# MotoNav

A compact motorcycle navigation display. A small screen on the handlebar shows
simplified turn-by-turn instructions. The phone does all the navigation work
(GPS, routing, instructions) and sends the result to the device over Bluetooth
Low Energy (BLE).

The long-term goal is a commercial product. The current goal is a working
prototype built in small, reviewable stages.

## Status

Status vocabulary: **Implemented**, **Tested**, **Partially implemented**,
**Planned**, **Blocked**.

| Area | Status | Notes |
|---|---|---|
| Phase 0 — discovery and planning | Implemented (documents only) | Approved. |
| Phase 1 — display prototype | Implemented and host-tested. **Not device-tested.** | Approved. Physical hardware validation (T1-T6) is prepared and **not yet run**. Results in [TESTING.md](TESTING.md). |
| ESP32 firmware: model, screens, simulation, diagnostics logic | Host-tested | 77 C++ and 93 Python tests pass on a normal computer (these include the Phase 2 protocol tests). |
| ESP32 firmware: device code (`main.cpp`, SSD1306 adapter) | Compile-checked for the classic-ESP32 DevKit target (0 warnings). **Never run on a board.** | See [TESTING.md](TESTING.md). |
| Phase 2 — Bluetooth LE link, simulated messages | Codec and receiver **implemented and host-tested**. Radio code (`motonav_ble`) **compile-checked only**. **Never run over a real Bluetooth link.** | Prepared at the project owner's request while the Phase 1 hardware validation is pending. Protocol: [BLUETOOTH_PROTOCOL.md](BLUETOOTH_PROTOCOL.md). Device tests B1-B7 are **not run**: [TESTING.md](TESTING.md) section 5. Awaiting review. |
| Phase 3 — Android test sender | Protocol code and connection logic **implemented and host-tested** (138 Kotlin tests, no phone). The Android app (`android/app`) is **compile-checked only**. **Never run on a phone, never run against a MotoNav.** | Sends the simulated messages of Phase 2 from a phone. Not the navigation app. Prepared while the hardware validation is pending. See [android/README.md](android/README.md); phone tests P1-P8 are **not run**: [TESTING.md](TESTING.md) section 6. Awaiting review. |
| Google Navigation SDK integration | Planned | Phase 4. Commercial-terms questions are open, see [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md#technical-risks-and-unknowns). |
| Hardware | **Provisional** | From product listings: classic ESP32 30-pin DevKit (CP2102), 0.96" 128x64 I2C OLED, expected SSD1306. **Not verified on the parts.** Verification steps and diagnostics are in [HARDWARE.md](HARDWARE.md) section 4. |

**Nothing has been run on a real board or display yet.** All firmware results so
far are *host* results (automated tests and rendered images on a normal
computer) plus compile checks. The hardware is identified only from product
listings, so the pin numbers, the display size and the OLED address in
`firmware/platformio.ini` are provisional until verified. The Phase 2 Bluetooth
firmware is a separate environment, `motonav_ble`; the Phase 1 environment
`motonav`, which the hardware tests T1 to T6 check, is unchanged. The Android
test sender exists but has not run on a phone, and there is no Google Navigation
SDK integration yet. The Phase 2 B tests should wait until T1 to T4 have passed,
and the Phase 3 P tests until B1 and B2 have passed.

## Try it

Host tests and screen images (need `g++`, `make` and Python 3; Pillow for the images):

```bash
cd firmware/host
make test        # layering check + 77 C++ tests, with sanitizers
make test-tools  # 93 Python tests: diagnostics simulations, log analyser, Bluetooth reference tool, docs and source checks
make render      # writes out/*.pbm and out/montage_*.png to look at
```

On the board, in this order. The full procedures, expected output and pass/fail
criteria are in [TESTING.md](TESTING.md) section 3 (tests T1 to T6):

```bash
cd firmware
pio run -e hw_check   -t upload && pio device monitor   # T1 chip identity, T2 I2C detection
pio run -e oled_check -t upload && pio device monitor   # T3 SSD1306 or SH1106?
pio run -e motonav    -t upload && pio device monitor   # T4 the firmware; T5/T6 long runs
python host/analyze_log.py t5.log --min-minutes 30      # judge a long-run log
```

Phase 2 (Bluetooth), **only after T1 to T4 have passed**. The steps and the ready-to-send
test messages are in [TESTING.md](TESTING.md) section 5 (tests B1 to B7):

```bash
cd firmware
pio run -e motonav_ble -t upload && pio device monitor
python ../protocol/frame_tool.py scenarios --compact   # messages to paste into a BLE app
```

Phase 3 (Android), tests on a computer need JDK 17 or newer; building the app also needs the Android SDK
(platform 37.0, build-tools 36.0.0). The phone tests P1 to P8 are in [TESTING.md](TESTING.md) section 6:

```bash
cd android
./gradlew :protocol:test :link:test         # 138 Kotlin tests, no phone or SDK needed
./gradlew :app:lintDebug :app:assembleDebug     # lint (warnings are errors) and the debug APK
```

## Documents

| Document | Purpose |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design, code layers, verified Google SDK facts, repository layout |
| [HARDWARE.md](HARDWARE.md) | Hardware assumptions, identification steps, firmware configuration |
| [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) | Phases, risks, Phase 1 results and acceptance criteria |
| [DECISIONS.md](DECISIONS.md) | Technical decisions and their status |
| [BLUETOOTH_PROTOCOL.md](BLUETOOTH_PROTOCOL.md) | The phone-to-device Bluetooth LE protocol, version 1: GATT layout, frame format, validation, sequence rules |
| [android/README.md](android/README.md) | The Android project: modules, build, pinned versions, the rules the code keeps |
| [TESTING.md](TESTING.md) | What was tested, how, with actual results; device test procedures |
