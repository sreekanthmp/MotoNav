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
| Phase 1 — display prototype | Implemented and host-tested. **Not device-tested.** | Awaiting your review. Results in [TESTING.md](TESTING.md). |
| ESP32 firmware: model, screens, simulation | Host-tested | 34 automated tests pass on a normal computer. |
| ESP32 firmware: device code (`main.cpp`, SSD1306 adapter) | Compile-checked for the classic-ESP32 DevKit target (0 warnings). **Never run on a board.** | See [TESTING.md](TESTING.md). |
| BLE protocol | Planned | Phase 2. Not started. |
| Android app | Planned | Phase 3. |
| Google Navigation SDK integration | Planned | Phase 4. Commercial-terms questions are open, see [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md#technical-risks-and-unknowns). |
| Hardware | **Provisional** | From product listings: classic ESP32 30-pin DevKit (CP2102), 0.96" 128x64 I2C OLED, expected SSD1306. **Not verified on the parts.** Verification steps and diagnostics are in [HARDWARE.md](HARDWARE.md) section 4. |

**Nothing has been run on a real board or display yet.** All firmware results so
far are *host* results (automated tests and rendered images on a normal
computer) plus compile checks. The hardware is identified only from product
listings, so the pin numbers, the display size and the OLED address in
`firmware/platformio.ini` are provisional until verified. There is no BLE and no
Android code.

## Try it

Host tests and screen images (need only `g++` and `make`; Python with Pillow
for the contact sheet):

```bash
cd firmware/host
make test      # layering check + 34 tests, with sanitizers
make render    # writes out/*.pbm and out/montage_*.png to look at
```

On the board, in this order (details in [HARDWARE.md](HARDWARE.md) section 4 and
[TESTING.md](TESTING.md) section 3):

```bash
cd firmware
pio run -e hw_check   -t upload && pio device monitor   # chip identity + I2C scan
pio run -e oled_check -t upload && pio device monitor   # SSD1306 or SH1106?
pio run -e motonav    -t upload && pio device monitor   # the firmware
```

## Documents

| Document | Purpose |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design, code layers, verified Google SDK facts, repository layout |
| [HARDWARE.md](HARDWARE.md) | Hardware assumptions, identification steps, firmware configuration |
| [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md) | Phases, risks, Phase 1 results and acceptance criteria |
| [DECISIONS.md](DECISIONS.md) | Technical decisions and their status |
| [TESTING.md](TESTING.md) | What was tested, how, with actual results; device test procedures |

`BLUETOOTH_PROTOCOL.md` is created when Phase 2 starts, so it never describes
work that does not exist.
