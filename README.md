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
| ESP32 firmware: device code (`main.cpp`, SSD1306 adapter) | Compile-checked for generic ESP32, S3 and C3 (0 warnings). **Never run on a board.** | See [TESTING.md](TESTING.md). |
| BLE protocol | Planned | Phase 2. Not started. |
| Android app | Planned | Phase 3. |
| Google Navigation SDK integration | Planned | Phase 4. Commercial-terms questions are open, see [DEVELOPMENT_PLAN.md](DEVELOPMENT_PLAN.md#technical-risks-and-unknowns). |
| Hardware identification | Blocked | Needs input from the person with the board. See [HARDWARE.md](HARDWARE.md). |

**Nothing has been run on a real board or display yet.** All firmware results so
far are *host* results (automated tests and rendered images on a normal
computer). The board and display are still unidentified, so the firmware has no
default pins and will not build until you give it yours. There is no BLE and no
Android code.

## Try it

Host tests and screen images (need only `g++` and `make`; Python with Pillow
for the contact sheet):

```bash
cd firmware/host
make test      # layering check + 34 tests, with sanitizers
make render    # writes out/*.pbm and out/montage_*.png to look at
```

Flashing a board: see [HARDWARE.md](HARDWARE.md#configuring-the-firmware-implemented-not-yet-used-on-real-hardware)
and the device test procedures in [TESTING.md](TESTING.md).

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
