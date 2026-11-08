# Testing

Every result in this document carries one of three labels, and they are never mixed:

- **Host-tested**: automated tests, simulations or rendered images on a normal computer
  (Linux, gcc 13.3 and clang 18.1). Says nothing about the physical hardware.
- **Compile-checked**: the device-side code was built by the real ESP32 toolchain. Says
  nothing about whether it runs.
- **Device-tested**: run on the real board and display, by the board owner.
  **Nothing is device-tested yet.** The AI assistant has no access to the hardware and
  has not marked any hardware test as passed. Every hardware result below says
  `NOT RUN` until the board owner fills it in with real evidence.

Last updated: Phase 4a (Google Navigation SDK conversion) prepared; the Phase 1 hardware validation is still pending (2026-11-08).

## 0. Status at a glance

| Area | Label | Status |
|---|---|---|
| Firmware logic: model, screens, simulation, diagnostics verdicts, Bluetooth frame codec and receiver (77 C++ tests) | Host-tested | pass |
| Log analyser, `hw_check` and `oled_check` simulations, Bluetooth reference tool, documentation and source checks (105 Python tests) | Host-tested | pass |
| Android protocol code and connection logic, against the shared vectors and a fake Bluetooth (106 + 32 Kotlin tests) | Host-tested | pass |
| Google feed message to device message: maneuver table (all 66 SDK maneuvers), states, arrival, numbers, road names; 17 reviewed cases through the firmware's real receiver (27 Kotlin tests) | Host-tested | pass |
| Firmware (`motonav`), both diagnostics and the Phase 2 firmware (`motonav_ble`) build with the real ESP32 toolchain | Compile-checked | pass, 0 warnings |
| Android app builds with the real Android toolchain and the real Navigation SDK 8.1.0 (0 compiler warnings, lint clean) | Compile-checked | pass |
| **T1** ESP32 identification | Device-tested | **NOT RUN** |
| **T2** I2C detection | Device-tested | **NOT RUN** |
| **T3** OLED controller validation | Device-tested | **NOT RUN** |
| **T4** Main firmware validation | Device-tested | **NOT RUN** |
| **T5** 30-minute continuous operation | Device-tested | **NOT RUN** |
| **T6** 12-hour soak (extended reliability test) | Device-tested | **NOT RUN** |
| **B1** to **B7** Bluetooth LE (Phase 2, [section 5](#5-phase-2-bluetooth-le-prepared-nothing-has-run-over-a-real-bluetooth-link)) | Device-tested | **NOT RUN** |
| **P1** to **P8** Android test sender on a phone (Phase 3, section 6) | Device-tested | **NOT RUN** |
| **N1** to **N7** Google Navigation SDK: start, route, feed, end to end, screen off, ride, cost (Phase 4a, section 7) | Device-tested | **NOT RUN** |

## 1. Host tests

Run from `firmware/host`. They need a C++ compiler and `make`; the Python ones also need
Python 3.

```bash
make test                            # layering check + 77 C++ tests (gcc, ASan + UBSan, -Werror)
make test-tools                      # builds the simulators, then 105 Python tests
make -B test CXX=clang++ SANITIZE=   # second compiler, no sanitizers
make render                          # screen images in out/ (needs Pillow for the contact sheets)
make reference                       # regenerates docs/reference/ (images and expected outputs)
make golden-update                   # accept a deliberate rendering change (look at the images first!)
```

### Results (Host-tested)

| Run | Result |
|---|---|
| gcc 13.3, `-std=c++11 -Wall -Wextra -Wpedantic -Werror`, AddressSanitizer + UBSan: `make test` | **77 tests, 339,828 checks, 0 failed** |
| clang 18.1, same warnings, no sanitizers: `make test` | **77 tests, 339,828 checks, 0 failed**; golden hashes identical to gcc |
| gcc with sanitizers: `make test-tools` | **105 Python tests, OK** |
| JUnit 6.1.3, in `android/`: `./gradlew :protocol:test :link:test :navsdk:testDebugUnitTest` | **106 + 32 + 27 = 165 Kotlin tests, OK** (sections 6 and 7) |
| Layering check (`check_layering.sh`) | ok |

clang could not be run with sanitizers: this sandbox's clang has no AddressSanitizer
runtime library. gcc covered the sanitizers. (AddressSanitizer did catch one real bug
while these tests were written: a use-after-scope in a *test*, fixed.)

### What the tests cover

| Area | Tests |
|---|---|
| Distance formatting | 33 boundary cases; over 0-30,000 m the shown value never goes backwards and never differs from the truth by more than half a display step |
| Text and view building | width arithmetic, fitting with `...`; screen selection; road-name cleanup (UTF-8, Malayalam, cut mid-character); equal-looking states give equal views |
| Rendering | every screen at 128x64 and 128x32 stays inside the canvas and is neither blank nor solid; all 11 icons pairwise different; left/right icons mirror each other; every turn icon leans towards its side; roundabout shows its exit number; golden hashes of 34 screens |
| Flicker policy | over two full simulation loops (1,720 ticks) exactly 604 frames are presented, equal to the independently counted number of visible changes |
| Simulation | all 5 screens and 11 maneuvers visited; the `d` and `r` serial commands show exactly the distances and road names the documentation lists |
| **Diagnostic verdicts** | every pass/warn/fail rule of `hw_check`: chip, BLE required, cores, flash size (a 2 MB chip that claims 4 MB fails), pin validity (flash pins, input-only pins, strapping pins), I2C line states, probe reliability (one missed probe fails), OLED address (an unrelated device is never taken for the OLED, 0x3D is reported with the line to paste, swapped wires named) |
| **Frame consistency and log lines** | FNV-1a hash against published test vectors; a screen that changes is detected; the exact text of every log line the analyser reads |
| **`hw_check` simulation** | the real `hw_check.cpp` run against 16 simulated boards: healthy; OLED at 0x3D; both addresses; unrelated device; nothing connected; swapped wires; SDA shorted; no pull-ups; intermittent contact; device that only answers at 100 kHz; wrong chip; no BLE; 2 MB flash; 8 MB flash; unreadable flash; single core; and a mid-run unplug |
| **`oled_check` simulation** | the real `oled_check.cpp` draws exactly what this document says each of the five patterns looks like on a healthy SSD1306 |
| **Log analyser** | a clean 30-minute and 12-hour log pass; each of a leak, slow leak, fragmentation, reboot, brown-out, crash, firmware stall, frozen display, slow display updates, render mismatch, low stack, display init failure, too-short run, missing banner is detected; heap jitter is not mistaken for a leak |
| **Documentation** | this document cannot silently go stale: a test fails if any expected-output line shown here no longer matches what the code prints, or if any hardware test is recorded as passed |
| Architecture | `check_layering.sh` fails if `core/`, `sim/`, `ui/` or `diag/` include Arduino, Wire, a display driver or a radio header |
| **Bluetooth protocol (Phase 2)** | frame codec, receiver and the path from bytes to the display, fuzzed and cross-checked against an independent Python implementation: see [section 5.2](#52-what-the-host-tests-cover-host-tested) |

### Do the tests actually catch bugs? (Host-tested)

Bugs were injected one at a time, each was detected by the expected test, and the code was
restored (and verified identical) afterwards:

| Injected bug | Caught by (observed failing) |
|---|---|
| Distance rounded to 100 m instead of 50 m steps | `distance_boundaries` |
| Presenter always redraws (flicker) | `presenter_draws_first_frame_then_only_on_visible_change` |
| `#include <Arduino.h>` added to `core/` | layering check |
| `TurnLeft` drawn as a right turn | `all_maneuver_icons_are_distinct`, `left_and_right_icons_mirror_each_other` |
| One missed probe counted as a pass | `test_intermittent_contact_fails` |
| `hw_check` never tries the swapped-wires scan | `test_swapped_wires_fail_and_say_so`, `test_nothing_connected_fails` |
| A flash chip smaller than 4 MB accepted | `test_small_flash_fails_even_if_the_image_header_claims_4mb` |
| Analyser ignores a falling heap | `test_memory_leak_fails`, `test_fragmentation_fails`, `test_exit_codes` |

Only the first few failure lines of each run were inspected.

### Visual review of the screens (Host-tested; judgement of the AI assistant)

The rendered contact sheets were inspected. Findings and fixes: sharp-turn arrows were an
unrecognisable blob on the first render (fixed); turn arrows sat off-centre (fixed); on
128x32 the distance was small and "ARRIVED" overlapped the pin (both fixed). Stray single
pixels on some arrowheads remain: they come from the host's triangle rasteriser, and the
real display uses Adafruit GFX's, so edge pixels can differ by about a pixel.
**How the screens look on the real panel is not known yet.**

## 2. Compile check of the device code

**Compile-checked. Not run.** Built with PlatformIO for the board `esp32doit-devkit-v1`
(a classic ESP32, 4 MB flash). The hardware itself is still **provisional** (see
[HARDWARE.md](HARDWARE.md)).

| Environment | What it is | Result | Static RAM | Flash (app partition) | Warnings |
|---|---|---|---|---|---|
| `motonav` | the MotoNav firmware | **SUCCESS** | 23,860 B (7.3 %) | 377,280 B of 1,310,720 B (28.8 %) | 0 |
| `hw_check` | diagnostic: chip, board and I2C checks | **SUCCESS** | 23,924 B (7.3 %) | 341,908 B of 1,310,720 B (26.1 %) | 0 |
| `oled_check` | diagnostic: five OLED test patterns | **SUCCESS** | 23,684 B (7.2 %) | 356,432 B of 1,310,720 B (27.2 %) | 0 |

Built with `-Wall -Wextra`. Versions (pinned in `firmware/platformio.ini`): PlatformIO
Core 6.2.0, platform `espressif32@8.0.0`, framework package
`framework-arduinoespressif32@4.30312.261024`, toolchain packages
`toolchain-xtensa-esp-elf` and `toolchain-riscv32-esp` 14.2.0+20260121, Adafruit GFX
Library 1.13.0, Adafruit SSD1306 2.5.17, Adafruit BusIO 1.17.4, esptool 5.4.0. Most of the
flash is the Arduino core itself. RAM is static usage only.

Compiling proved that the device APIs the diagnostics use exist in this framework version
(`esp_flash_read_id`, `esp_reset_reason`, `uxTaskGetStackHighWaterMark`,
`ESP.getMaxAllocHeap`, `INPUT_PULLDOWN`, `Wire.begin` returning a status). **It did not show
that they behave as expected on the chip.** In particular, reading the flash chip's own
JEDEC id (T1) is untested on real hardware.

Build-system checks, each performed: each environment compiles only its own sources;
building the firmware for an ESP32-S3 is refused by the `#error` in `src/main.cpp`;
building with any of the five `MOTONAV_*` display values missing is refused; the Arduino
variant for this board defines `SDA = 21`, `SCL = 22`; `esptool chip-id` and `flash-id`
exist in the pinned esptool (`--help` only; not run against a board).

## 3. Physical hardware validation (to be run by the board owner)

### 3.1 Ground rules

1. Run the tests **in order**: T1, T2, T3, T4, T5, then T6. **Stop at the first FAIL**,
   record it, and report it. Fixing a wire and re-running the same test is fine; skipping
   ahead is not.
2. A test **passes only if every pass criterion was met by what you actually observed.**
   If you are unsure, it is not a pass. Write what you saw.
3. Keep evidence: save each serial log to a file and take the photos listed in each test.
4. **The expected outputs below come from simulations.** They were produced by running the
   real diagnostic code against a *simulated* board, or the firmware logic on a computer.
   Values such as the MAC address, chip revision, flash chip id and heap sizes are
   **invented** and will differ on your board. Compare the *shape* of the output and the
   `PASS` / `WARN` / `FAIL` words, not the numbers.
5. Nothing in this document tells you the hardware is good. Only your results do.

### 3.2 Before you start

You need: the ESP32 board, the OLED, four jumper wires, a USB cable that carries data (a
charge-only cable gives no serial port), a computer with PlatformIO, a phone for photos and
a short video, and a computer USB port that stays powered (avoid unpowered hubs). For T5 and
T6 you also need Python 3 (`python --version`).

```bash
git clone https://github.com/sreekanthmp/MotoNav.git
cd MotoNav
git checkout claude/magical-hamilton-nv0umh
```

Wire the display by its own pin labels as described in [HARDWARE.md](HARDWARE.md) section 3
(**OLED pin order differs between modules; reversed power can destroy the display**), then
connect USB. Do **not** change the wiring while USB is connected, except where a test says so.

Optional sanity check that this checkout works on your computer (needs `g++`, `make`,
Python 3; skip on Windows): `cd firmware/host && make test test-tools`.

**Where to run commands.** Run every `pio` command **from the `firmware/` folder**, and keep your
log files there. That folder holds `platformio.ini`, which sets the serial speed to 115200 baud:
from anywhere else the monitor falls back to 9600 baud and shows garbage. The command blocks below
start with `cd firmware` for a fresh terminal; if you are already in `firmware/`, that line just
prints an error and you can ignore it. Paths in the commands below (such as `host/analyze_log.py`)
are relative to `firmware/`.

To save a serial log while watching it:

```bash
# Linux / macOS
pio device monitor | tee t1.log
# Windows PowerShell
pio device monitor | Tee-Object -FilePath t1.log
```

(PlatformIO's own `pio device monitor --filter log2file` also works and writes into a `logs/`
folder.) Press the board's **EN** button after starting the monitor so the log begins with
the boot messages. Stop the monitor with Ctrl+C.

---

### T1. ESP32 identification

**Purpose:** establish what the board really is: chip, radios, flash size, board
compatibility, whether the configured I2C pins are usable, and the electrical state of SDA
and SCL.

**Label when done:** Device-tested.

```bash
cd firmware
pio run -e hw_check -t upload
pio device monitor | tee t1.log          # then press EN
```

If upload stalls at `Connecting......`, hold the board's **BOOT** button until it starts.

**Expected output** (a healthy board; produced by running the real `hw_check.cpp` against a
**simulated** board, so the numbers are invented):

```text
MotoNav hw_check (diagnostic, not the MotoNav firmware)
config: 128x64 display, expected address 0x3C, SDA=GPIO21 SCL=GPIO22

========== hw_check run 1 ==========
--- Chip and board ---
Silicon model    : ESP32 (classic)
Model (SDK name) : ESP32-D0WD-V3
Revision         : 301
CPU              : 2 core(s) at 240 MHz
Radios           : Wi-Fi BLE Bluetooth-Classic
MAC (eFuse)      : 240AC4123456
Flash JEDEC id   : 0xEF4016 (manufacturer 0xEF, capacity code 0x16)
Flash from chip  : 4194304 bytes
Flash per SDK    : 4194304 bytes
[PASS] chip: classic ESP32, as expected
[PASS] radios: Wi-Fi and BLE present
[PASS] cpu: dual core at 240 MHz, as expected
[PASS] flash: 4 MB, as expected (read from the flash chip itself)
--- I2C pins and electrical state ---
Configured: SDA=GPIO21 SCL=GPIO22
[PASS] SDA pin: usable for I2C
[PASS] SCL pin: usable for I2C
[PASS] pin pair: SDA and SCL are different pins
SDA line (GPIO21): with internal pull-up reads HIGH, with internal pull-down reads HIGH
[PASS] SDA line: a strong external pull-up resistor is present (good)
SCL line (GPIO22): with internal pull-up reads HIGH, with internal pull-down reads HIGH
[PASS] SCL line: a strong external pull-up resistor is present (good)
--- I2C bus ---
100 kHz: device at 0x3C
400 kHz: device at 0x3C
Scan results at 100 kHz: ack=1 nack-on-address=125 nack-on-data=0 other=0 timeout=0
(Informational. Empty addresses normally all report the same code. A mixture of codes can mean noise.)
[PASS] OLED address: OLED found at the configured address
OLED detected at 0x3C
Probes at 100 kHz: 200 of 200 acknowledged
Probes at 400 kHz: 200 of 200 acknowledged
[PASS] probes at 100 kHz: every probe was acknowledged
[PASS] probes at 400 kHz: every probe was acknowledged
--- Summary ---
HW_CHECK RESULT: PASS  (12 pass, 0 warn, 0 fail)
run 2: PASS, OLED found, same as the previous run (1 in a row)
run 3: PASS, OLED found, same as the previous run (2 in a row)
```

The first full report appears within a few seconds of boot. Afterwards it prints one short
line every 5 seconds, and prints a full report again only if something changes.

**Pass criteria:**

| # | Criterion | Look for | Pass when |
|---|---|---|---|
| 1.1 | Upload and serial work | PlatformIO says `SUCCESS`; text appears at 115200 baud | yes |
| 1.2 | The chip is a classic ESP32 | `[PASS] chip`, `Silicon model: ESP32 (classic)` | PASS |
| 1.3 | BLE is present (Phase 2 needs it) | `[PASS] radios`, `BLE` in the Radios line | PASS |
| 1.4 | Dual core at 240 MHz | `[PASS] cpu` | PASS, or a WARN you report |
| 1.5 | Flash is 4 MB, **read from the flash chip itself** | `[PASS] flash`, `Flash from chip: 4194304 bytes`, a JEDEC id line | PASS |
| 1.6 | Board compatibility with the `esp32doit-devkit-v1` definition (classic ESP32, 4 MB, 240 MHz) | criteria 1.2 to 1.5 | all met |
| 1.7 | The configured SDA and SCL pins are usable GPIOs | `[PASS] SDA pin`, `[PASS] SCL pin`, `[PASS] pin pair` | PASS |
| 1.8 | The I2C lines are electrically sane | `[PASS] SDA line`, `[PASS] SCL line` | PASS. A `WARN ... no strong external pull-up` is accepted only if T2 then passes completely, and must be reported |

**If it fails:**

| What you see | Meaning | What to do |
|---|---|---|
| `[FAIL] chip ... NOT a classic ESP32` | the board is another ESP32 family | **Stop and report.** The firmware is deliberately not built for it |
| `[FAIL] radios ... no BLE` | no BLE on this chip | **Stop and report** |
| `[FAIL] flash ... smaller than the 4 MB` | the flash is smaller than the firmware's layout needs | **Stop and report** |
| `[WARN] flash ... differs` or `could not be read` | size source disagreement | run `pio pkg exec -p tool-esptool -- esptool flash-id` and send its output |
| `[WARN] cpu` | single core or slower clock | report; Phase 1 does not need the second core |
| `[FAIL] SDA pin` or `SCL pin` | the pin in `platformio.ini` cannot be used for I2C | report; do not rewire without asking |
| `[FAIL] SDA line ... shorted to GND` | a line is held low | unplug USB, check the wiring for shorts, retry |
| `[FAIL] ... inconsistent readings` | something is driving the line | unplug USB, check the wiring |
| no serial text at all | wrong port, charge-only cable, driver missing, or wrong baud | HARDWARE.md section 4 (V2) |
| garbled text | wrong baud rate | the monitor must be 115200 |

**Evidence to send:** the complete `t1.log`, and a photo of the ESP32 module's marking.

**Result record:**

| Date | Tester | Result | Notes |
|---|---|---|---|
| | | **NOT RUN** | |

---

### T2. I2C detection

**Purpose:** establish that the OLED answers on the bus, at which address (**never assume
0x3C**), that SDA and SCL are wired the right way round, and that the bus is reliable.

**Label when done:** Device-tested.

This uses the same `hw_check` firmware as T1. If it is still running, continue; otherwise
re-upload it.

1. Read the **I2C bus** part of the output. Find the line `OLED detected at 0x..`. That
   address came from detection on the bus, not from the configuration.
2. If an `ACTION:` line appears, edit `firmware/platformio.ini` exactly as it says, upload
   `hw_check` again, and confirm `[PASS] OLED address`.
3. Leave it running for **at least 2 minutes** without touching anything. You should see
   `same as the previous run (N in a row)` counting up to 20 or more.
4. While it runs, **gently** press and wiggle each of the four connectors, one at a time.
   Each unstable connection should produce `something changed since the previous run` and a
   full report. Record which wire does that.

**Expected output** (from the T1 listing above, I2C part): both scans list the same
address, `OLED detected at 0x3C`, then `Probes at 100 kHz: 200 of 200 acknowledged` and
`Probes at 400 kHz: 200 of 200 acknowledged`, then `[PASS]` for each.

**Pass criteria:**

| # | Criterion | Look for | Pass when |
|---|---|---|---|
| 2.1 | The OLED responds | a `device at 0x..` line at 100 kHz and at 400 kHz | yes, same address at both speeds |
| 2.2 | The address was detected, not assumed | `OLED detected at 0x..`; `[PASS] OLED address` once `platformio.ini` matches | PASS |
| 2.3 | SDA and SCL are the right way round | no `swapped` message | none |
| 2.4 | **No bus errors**: the OLED acknowledges every probe, at the speed the firmware uses | `[PASS] probes at 100 kHz` **and** `[PASS] probes at 400 kHz` (200 of 200 each) | both PASS |
| 2.5 | The bus is stable | at least 20 `same as the previous run` lines in a row, no `something changed` | yes (when you are not touching wires) |

The line `Scan results at 100 kHz: ack=.. nack-on-address=.. other=.. timeout=..` is
informational. Record it. If `other` or `timeout` are not zero for *empty* addresses, report
it: it may only be how this Arduino version reports an empty address, so it is not a pass
criterion by itself. The pass criterion is that the OLED itself never fails to answer.

**If it fails:**

| What you see | Meaning | What to do |
|---|---|---|
| `[WARN] OLED address ... NOT at the address in platformio.ini` | the OLED is at the other address | do what the `ACTION:` line says, re-upload, re-run |
| `[FAIL] ... only with SDA and SCL swapped` | the two signal wires are swapped | unplug USB, swap them |
| `[FAIL] ... no I2C device answered` (and the swapped scan found nothing) | no power, wrong pins, bad contact, or not a 4-pin I2C OLED | check VCC, GND, SDA, SCL against HARDWARE.md section 3; check the `SDA line` / `SCL line` results |
| `[WARN] ... devices at BOTH 0x3C and 0x3D` | another device is on the bus | report |
| `[WARN] ... none at a typical OLED address` | the device is something else, or an unusual OLED | report the addresses |
| `[FAIL] probes ... intermittent` | a connection is loose or the leads are poor | reseat the jumpers, try shorter leads, re-run; report if it persists |
| `[FAIL] probes at 400 kHz` but `[PASS] probes at 100 kHz` | the bus works slowly but not at the speed the firmware needs | report; do not proceed |
| `something changed` while you were not touching anything | an unstable connection | reseat the jumpers; report |

**Evidence to send:** the `hw_check` log covering the 2-minute run, and which (if any) wire
caused changes.

**Result record:**

| Date | Tester | OLED address found | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

---

### T3. OLED controller validation

**Purpose:** confirm the display initialises, renders the full 128x64 area correctly, and
works with the SSD1306 driver the firmware uses. If it looks like an SH1106, **stop**:
the display driver must be investigated before any production code changes.

**Label when done:** Device-tested.

```bash
cd firmware
pio run -e oled_check -t upload
pio device monitor | tee t3.log
```

The display cycles through five test patterns, six seconds each. Type `1` to `5` in the
serial monitor to hold one pattern, and `a` to cycle again. Photograph each pattern close
to the glass. (Phone cameras can show dark bands on an OLED: that is the camera's shutter,
not necessarily a fault. If a photo looks banded, take several, or a short video.)

**Expected serial output** (produced by running the real `oled_check.cpp` against a
simulated panel):

```text
MotoNav oled_check (diagnostic, not the MotoNav firmware)
config: 128x64 at 0x3C, SDA=21 SCL=22
Driving the panel as an SSD1306. Keys: 1 to 5 hold a pattern, a = cycle.
pattern 1 of 5: border (expect all four sides)
pattern 2 of 5: all pixels on (expect solid white to every edge)
pattern 3 of 5: edge columns (expect 4 lines at each side, 2 px apart)
pattern 4 of 5: grid (expect 8 rows x 8 columns of regular cells)
pattern 5 of 5: all pixels off (expect completely dark)
```

**What a healthy SSD1306 should show** (predicted by running the real drawing code on a
bitmap; this is **not** a photograph, and pixel edges can differ slightly on the real
glass). The image below is `docs/reference/oled_check_patterns.png`:

![Expected oled_check patterns](docs/reference/oled_check_patterns.png)

**Pass criteria** (all must be met):

| # | Criterion | Pattern | Pass when |
|---|---|---|---|
| 3.1 | Initialisation works | serial | `Driving the panel as an SSD1306...`; **no** `NO ANSWER` |
| 3.2 | No missing edges | 1 (border) | a complete rectangle on the very edge of the glass: **all four sides**, diagonals reaching all four corners |
| 3.3 | Solid to every edge | 2 (all on) | solid white to **all four edges**, no dark or speckled strip |
| 3.4 | No lost columns | 3 (edge columns) | **four** thin lines at the left edge and **four** at the right edge, evenly spaced |
| 3.5 | Correct 128x64 geometry, nothing scrambled | 4 (grid) | a regular grid of **8 rows by 8 columns**, with no broken, shifted, repeated or uneven cells |
| 3.6 | No stuck pixels or noise | 5 (all off) | **completely dark**; no lit pixel, speckle or stripe anywhere |

A two-colour module (a yellow band across the top 16 rows, blue below) is normal and does
not affect the criteria.

**Deciding SSD1306 versus SH1106.** An SSD1306 has 128 columns of display memory; an SH1106
has 132 and shows columns 2 to 129, and uses a different memory-addressing scheme. Driven by
the SSD1306 driver, an SH1106 is *expected* to lose the first two columns on the left, show
speckles on the right edge, and possibly look scrambled or torn. (This is predicted from the
controllers' designs and has not been seen on your panel.) So:

- **All five patterns match: the controller is compatible with the SSD1306 driver.**
- **Anything else: stop.** Send the photos. Do not guess whether it is the controller, the
  wiring or the power.

**If it fails:**

| What you see | Likely cause | What to do |
|---|---|---|
| `NO ANSWER at 0x..` | wrong address, wiring | back to T2 |
| Nothing lights, no error | display unpowered or contrast off | check VCC and GND; report |
| Left border or left lines missing, speckles on the right | SH1106 (expected symptom) | **stop and report with photos** |
| Only the top half shows, or the picture repeats or is squashed | a 128x32 panel configured as 128x64 | report; one-line change |
| Scrambled, torn or noisy picture | SH1106, or a wiring/power fault | **stop and report with photos** |
| Upside down or mirrored | panel mounted the other way up | report; easy to change |
| Lit pixels in pattern 5 | stuck pixels or controller mismatch | report with a photo |

**Evidence to send:** `t3.log`, and one photo of **each of the five patterns**.

**Result record:**

| Date | Tester | Controller conclusion | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

---

### T4. Main firmware validation

**Purpose:** verify that the MotoNav firmware boots reliably, draws all five screens and all
11 maneuver icons correctly, formats distances and road names correctly, switches screens
cleanly, and does not flicker or reset.

**Label when done:** Device-tested. **Only run after T1 to T3 passed.**

```bash
cd firmware
pio run -e motonav -t upload
pio device monitor | tee t4.log
```

The firmware prints, on every serial command that holds a screen, an `expect:` line saying
exactly what the display should be showing. **Compare the display with that line and with
the reference sheets** (`docs/reference/`, generated by the same code, so they match the
`expect:` lines). The reference images are predictions from the drawing code, not photographs.

![Expected screens](docs/reference/screens_128x64.png)

#### T4.1 Boot reliability and the boot output

Expected at every boot (the chip line shows your board's values, and the `[render]` lines
appear as each static screen is drawn for the first time; their hashes are specific to your
board's build):

```text
MotoNav firmware - Phase 1 (simulated navigation, no BLE)
reset: POWERON
chip: <your chip model> rev <n>, 2 core(s), flash 4194304 bytes
config: 128x64 display at 0x3C, SDA=21 SCL=22
I2C scan:
  device at 0x3C
display: initialised
[render] baseline screen=Disconnected hash=<8 hex digits>
```

- Unplug and replug USB **20 times**. After each, note whether the display shows the
  **MOTONAV / BLUETOOTH DISCONNECTED** screen within about two seconds, and that the serial
  log shows `display: initialised`.
- Press the **EN** button **20 times** and do the same. (`reset:` should say `POWERON` or
  `EXT` for these; either is normal.)

| Criterion | Pass when |
|---|---|
| 4.1a | on all 20 power-ups the display shows the first screen, never blank, never garbled |
| 4.1b | on all 20 resets, the same |
| 4.1c | every boot's serial output contains `display: initialised` and a `reset:` line of `POWERON` or `EXT` |
| 4.1d | the `config:` line reads 128x64 at the address from T2 |

| Power-ups OK of 20 | Resets OK of 20 | Faults seen | Result |
|---|---|---|---|
| | | | **NOT RUN** |

#### T4.2 A failed initialisation recovers

1. With the board running, disconnect the **SDA signal wire only** (never VCC or GND) from
   the board, then press EN.
2. Expect, repeating about every 2 seconds: `display: no answer at 0x.. (I2C error N)`
   (`N` is an error code). It must **not** reboot repeatedly.
3. Reconnect the SDA wire. Within about 2 seconds expect `display: initialised` and the
   first screen.

| Criterion | Pass when |
|---|---|
| 4.2a | the no-answer message repeats and the boot banner does **not** repeat (no reboot loop) |
| 4.2b | after reconnecting, the display recovers by itself, without a reset |

| Result | Notes |
|---|---|
| **NOT RUN** | |

#### T4.3 The five screens

Type the key in the serial monitor; the screen holds until you type another.

| Press | Serial prints | The display should show |
|---|---|---|
| `0` | `expect: screen=Disconnected` | Large **MOTONAV**, and below it **BLUETOOTH** / **DISCONNECTED** |
| `1` | `expect: screen=Ready` | Large **MOTONAV**, and below it **BLUETOOTH CONNECTED** / **READY TO NAVIGATE** |
| `2` | `expect: screen=Navigating icon=Straight distance="450 m" road="Main Street"` | A straight-up arrow on the left; **450** large with **m** below it, **CONTINUE**, and **Main Street** along the bottom |
| `3` | `expect: screen=Rerouting` | **REROUTING** in large letters, and **FINDING NEW ROUTE** below |
| `4` | `expect: screen=Arrived` | A map-pin icon, and **DESTINATION REACHED** along the bottom |

| Criterion | Pass when |
|---|---|
| 4.3 | all five screens match the description and the reference sheet; text complete, nothing cut off, nothing shifted |

| Result | Notes |
|---|---|
| **NOT RUN** | |

#### T4.4 The 11 maneuver icons (arrow clarity)

Press `2`, then `n` eleven times. Each press moves to the next maneuver; the eleventh wraps
back to the first. For each, compare the display with the table and the reference sheet.

| Press | Serial prints | Instruction text | The arrow should look like |
|---|---|---|---|
| `2` | `expect: screen=Navigating icon=Straight distance="450 m" road="Main Street"` | CONTINUE | an arrow pointing straight up |
| `n` | `expect: screen=Navigating icon=TurnLeft distance="450 m" road="Main Street"` | TURN LEFT | an L-shaped arrow: up, then bending left, arrowhead pointing left |
| `n` | `expect: screen=Navigating icon=TurnRight distance="450 m" road="Main Street"` | TURN RIGHT | the mirror image: bending right, arrowhead pointing right |
| `n` | `expect: screen=Navigating icon=SlightLeft distance="450 m" road="Main Street"` | SLIGHT LEFT | an arrow leaning up and to the left at about 45 degrees |
| `n` | `expect: screen=Navigating icon=SlightRight distance="450 m" road="Main Street"` | SLIGHT RIGHT | an arrow leaning up and to the right at about 45 degrees |
| `n` | `expect: screen=Navigating icon=SharpLeft distance="450 m" road="Main Street"` | SHARP LEFT | an arrow going up, then folding back down to the left (arrowhead pointing down-left) |
| `n` | `expect: screen=Navigating icon=SharpRight distance="450 m" road="Main Street"` | SHARP RIGHT | the mirror image (arrowhead pointing down-right) |
| `n` | `expect: screen=Navigating icon=UTurnLeft distance="450 m" road="Main Street"` | U-TURN | a U-shaped arrow: up on the right, over the top, arrowhead pointing down on the left |
| `n` | `expect: screen=Navigating icon=UTurnRight distance="450 m" road="Main Street"` | U-TURN | the mirror image: up on the left, over the top, arrowhead pointing down on the right |
| `n` | `expect: screen=Navigating icon=Roundabout exit=2 distance="450 m" road="Main Street"` | ROUNDABOUT | a ring with the digit **2** inside, a stem entering from below, and an arrowhead leaving at the top |
| `n` | `expect: screen=Navigating icon=Arrived distance="450 m" road="Main Street"` | DESTINATION | a map pin: a circle with a hole above a point |
| `n` | `expect: screen=Navigating icon=Straight distance="450 m" road="Main Street"` | CONTINUE | an arrow pointing straight up (wraps back to the start) |

Judge each icon **at arm's length, at a glance**: could a rider tell the arrow types apart?

| Criterion | Pass when |
|---|---|
| 4.4a | all 11 icons are recognisable and match the table |
| 4.4b | left and right variants point the right way (not mirrored) |
| 4.4c | the roundabout shows a legible **2** inside the ring |

| Maneuver | Recognisable? (yes/no) | Notes |
|---|---|---|
| (record all 11) | | |

Photograph all 11 (or send a short video of the sequence). Result: **NOT RUN**.

#### T4.5 Distance formatting

Press `d` ten times (the maneuver stays as last chosen). The large number and its unit must
match the table; the tenth press wraps back to 450 m. The reference sheet is
`docs/reference/t4_distances.png`.

| Press | Serial prints | Large number and unit on the display |
|---|---|---|
| `d` | `expect: screen=Navigating icon=Straight distance="90 m" road="Main Street"` | **90 m** |
| `d` | `expect: screen=Navigating icon=Straight distance="0 m" road="Main Street"` | **0 m** |
| `d` | `expect: screen=Navigating icon=Straight distance="10 m" road="Main Street"` | **10 m** |
| `d` | `expect: screen=Navigating icon=Straight distance="1.0 km" road="Main Street"` | **1.0 km** |
| `d` | `expect: screen=Navigating icon=Straight distance="1.2 km" road="Main Street"` | **1.2 km** |
| `d` | `expect: screen=Navigating icon=Straight distance="9.9 km" road="Main Street"` | **9.9 km** |
| `d` | `expect: screen=Navigating icon=Straight distance="12 km" road="Main Street"` | **12 km** |
| `d` | `expect: screen=Navigating icon=Straight distance="999 km" road="Main Street"` | **999 km** |
| `d` | `expect: screen=Navigating icon=Straight distance="--" road="Main Street"` | **--** |
| `d` | `expect: screen=Navigating icon=Straight distance="450 m" road="Main Street"` | **450 m** |

![Expected distances](docs/reference/t4_distances.png)

| Criterion | Pass when |
|---|---|
| 4.5a | all ten distances show exactly as listed |
| 4.5b | the number is the largest text on the screen and easy to read |

Result: **NOT RUN**. Notes:

#### T4.6 Road name positioning

Press `r` five times. The bottom line must match the table. The reference sheet is
`docs/reference/t4_road_names.png`.

| Press | Serial prints | Bottom line of the display |
|---|---|---|
| `r` | `expect: screen=Navigating icon=Straight distance="450 m" road="Sir Mokshagundam Visvesvaraya R"` | Sir Mokshagundam V... (shortened to fit 21 characters, ending in three dots) |
| `r` | `expect: screen=Navigating icon=Straight distance="450 m" road=""` | no road line at all |
| `r` | `expect: screen=Navigating icon=Straight distance="450 m" road=""` | no road line at all (the Malayalam name has no letters the font can draw, so it is hidden) |
| `r` | `expect: screen=Navigating icon=Straight distance="450 m" road="Caf? Road"` | Caf? Road (the accented letter becomes a question mark) |
| `r` | `expect: screen=Navigating icon=Straight distance="450 m" road="Main Street"` | Main Street |

Note that the serial line shows the 31-character cleaned name, while the display fits it to
21 characters.

![Expected road names](docs/reference/t4_road_names.png)

| Criterion | Pass when |
|---|---|
| 4.6a | each road line is as listed, centred along the bottom, within the glass |
| 4.6b | where the table says "no road line", nothing is drawn there |

Result: **NOT RUN**. Notes:

#### T4.7 Screen transitions and flicker

Type `a` (automatic mode) and watch for **5 minutes**, ideally recording a 60-second video.
The script repeats every 86 seconds:

| Time in loop | Screen |
|---|---|
| 0 to 4 s | Disconnected |
| 4 to 8 s | Ready |
| 8 to 74 s | the 11 maneuvers, 6 s each, in the order of T4.4, each counting down from 1.5 km to about 45 m, with these roads in turn: Main Street, MG Road, Outer Ring Road, National Highway 66, Sir Mokshagundam V..., (none), Pacific Coast Highway, Main Street, MG Road, Outer Ring Road, National Highway 66 |
| 74 to 78 s | Rerouting |
| 78 to 83 s | Arrived |
| 83 to 86 s | Ready (navigation stopped), then it repeats |

| Criterion | Pass when |
|---|---|
| 4.7a | every transition is clean: no blank flash, half-drawn screen, tearing or garbage |
| 4.7b | no visible flicker or flashing; the screen changes only when the text or icon changes |
| 4.7c | no jitter or shifting of text between updates |
| 4.7d | **no unexpected reset**: the boot banner appears only at the start of `t4.log`, never again |

(A phone camera may show bands; judge with your eyes, and report what the camera shows
separately.)

| Result | Notes |
|---|---|
| **NOT RUN** | |

**Evidence to send for T4:** `t4.log`; photos of the five screens; photos or a video of the
11 icons; photos of the distance and road-name checks that did *not* match; the 5-minute
transition video if anything looked wrong.

**If T4 fails:**

| What you see | Meaning | What to do |
|---|---|---|
| Display shows nothing but serial says `initialised` | wiring or contrast, or SH1106 | back to T3 |
| A screen does not match its `expect:` line | a rendering or wiring fault | send a photo and the log |
| An icon looks wrong or mirrored | rendering differs from the host prediction | send a photo of that icon; do not change code |
| Any distance differs from the table | formatting fault | send the serial line and a photo |
| Flashing or tearing during automatic mode | frame updates not clean | send a video |
| `reset: BROWNOUT`, `PANIC`, `TASK_WDT` or `INT_WDT`, or the banner appears again | the board restarted | send the whole log; try another USB cable or port |

---

### T5. 30-minute continuous operation

**Purpose:** run the navigation simulation continuously and watch free heap, unexpected
resets, display freezing, memory stability and rendering consistency, judged by a program
instead of by eye.

**Label when done:** Device-tested. **Only run after T4 passed.**

1. Upload the firmware if needed: `cd firmware && pio run -e motonav -t upload`.
2. Start the monitor with a log, **then press EN** so the log starts with the boot lines:

   ```bash
   pio device monitor | tee t5.log          # Windows: | Tee-Object -FilePath t5.log
   ```

3. Take a photo of the running display. Leave it in automatic mode for **at least
   32 minutes** (the check needs 30 minutes of statistics). Look at the display now and then.
   Use a stable USB port and keep the computer awake: if the computer sleeps, the log stops
   and the board may lose power.
4. Stop the monitor (Ctrl+C) and photograph the display. **Do not type any key during the
   run**: a held screen stops the display from updating, and the analyser would report that as
   a frozen display.
5. Judge the log:

   ```bash
   python host/analyze_log.py t5.log --min-minutes 30
   ```

   (Run from `firmware/`. Use `python3` on Linux or macOS if `python` is not found, and `py` or
   `python` on Windows.) It exits with status 0 unless a criterion failed.
6. Only now check the display itself after the long run: open the monitor again (`pio device
   monitor`; the board may restart when it opens, which is fine for this check), type `0`,
   `1`, `2`, `3`, `4`, and compare each screen with your T4.3 photos. They should be identical.

The firmware prints a line like `[stats] uptime=60s heap_free=... heap_min=... max_alloc=...
stack_free=... frames=... present_max_ms=... render_mismatch=...` once a minute. The analyser
reads them. What each field means:

| Field | Meaning |
|---|---|
| `heap_free`, `heap_min` | free memory now, and the lowest it has been since boot |
| `max_alloc` | the largest single block that could be allocated (falls if memory fragments) |
| `stack_free` | the least free stack the main task has had, in bytes |
| `frames` | how many frames have been sent to the display |
| `present_max_ms` | the slowest single display update in the last minute |
| `render_mismatch` | how many times a screen that must always look the same came out different |

**Expected analyser output for a clean run** (produced from a **synthetic** log built with the
firmware's real log formatters; the numbers are invented):

```text
[PASS] boot banner          exactly one boot in the log
[PASS] reset reason         first boot: POWERON
[PASS] duration             ran 31 min 0 s (needed 30 min)
[PASS] stats cadence        stats every 60 to 60 s
[PASS] display updating     6510 frames in total, never fewer than 100 per minute
[PASS] free heap            free heap: lowest in first third 230000 B, lowest in last third 230000 B (change +0 B; range 230000 to 230000)
[PASS] heap fragmentation   largest free block: lowest in first third 110000 B, lowest in last third 110000 B (change +0 B; range 110000 to 110000)
[PASS] heap low-water mark  lowest-ever free heap went from 229000 B to 229000 B
[PASS] stack                least free stack seen: 5200 B
[PASS] display update time  slowest single display update: 38 ms (a full-screen update is normally tens of ms)
[PASS] render consistency   static screens were identical every time they were drawn
[PASS] display init         no display errors in the log

OVERALL: PASS
```

**Pass criteria:**

| # | Criterion | Pass when |
|---|---|---|
| 5.1 | the analyser's `OVERALL` line | `PASS`, or `WARN` that you have read and reported. **Any FAIL fails T5** |
| 5.2 | no unexpected resets | `[PASS] boot banner` (exactly one boot) and `[PASS] reset reason` |
| 5.3 | free heap | `[PASS] free heap` and `[PASS] heap fragmentation`: no downward trend |
| 5.4 | display not frozen | `[PASS] display updating`, and you saw it changing when you looked |
| 5.5 | rendering consistent | `[PASS] render consistency`, and the end-of-run screens match the T4.3 photos |
| 5.6 | the run was long enough | `[PASS] duration` |

**What the analyser can and cannot see.** It judges what the firmware logs. The render
consistency check watches the firmware's frame buffer in RAM, so it detects memory
corruption or changed drawing code. It **cannot** see a glitch on the I2C wires that corrupts
what the *glass* shows. That is why you look at the display and compare the end-of-run
photos.

**If it fails:**

| Analyser line | Meaning | What to do |
|---|---|---|
| `boot banner ... N boots` | the board restarted during the run | send the log; look at the `reset:` lines |
| `reset reason ... BROWNOUT` | the supply voltage dipped | try another USB cable or port; use a powered port |
| `reset reason ... PANIC` / `TASK_WDT` / `INT_WDT` / `WDT` | a crash or hang | send the whole log |
| `stats cadence ... gap` | the firmware stalled, or the log lost data | send the log; note whether the display froze |
| `display updating ... too few` | the display stopped updating | note when; send the log |
| `free heap` / `heap fragmentation` / `heap low-water mark` | memory falling | send the log |
| `stack` | the task is running out of stack | send the log |
| `display update time` | a display update was slow, for example a stalled I2C transfer | send the log |
| `render consistency` | a static screen changed | send the log and photos |
| `display init` | the display did not start cleanly | send the log |
| `duration` | the run was too short | repeat for 32 minutes |
| `boot banner ... starts mid-run` (WARN) | you started the monitor after boot | repeat with EN pressed after starting the monitor |

**Evidence to send:** `t5.log`, the analyser's output, the start and end photos, and anything
you saw (freeze, flicker).

**Result record:**

| Date | Run length | Analyser OVERALL | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

---

### T6. 12-hour soak (extended reliability test)

**Purpose:** the extended version of T5. A 30-minute run cannot reveal a slow memory leak or a
fault that appears once an hour.

**Label when done:** Device-tested. **Only run after T5 passed.**

Same procedure as T5, but leave it for **at least 12 hours 5 minutes**, on a computer that
will not sleep, with a good USB cable and a powered port. Then:

```bash
python host/analyze_log.py t6.log --min-minutes 720
```

Pass criteria are those of T5, over 12 hours, plus the display still working and matching the
T4.3 photos at the end. The 12-hour run is far more sensitive to small leaks, so a `WARN` on
`free heap` here must be reported even though it does not fail the run.

| Date | Run length | Analyser OVERALL | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

---

## 4. Known limitations

- **No hardware result exists yet.** Every T1 to T6 result is `NOT RUN`.
- All expected outputs and reference images come from simulations and predictions. Real values
  (chip revision, flash id, MAC, heap sizes, timings, edge pixels) will differ.
- The flash size check reads the flash chip's JEDEC id through `esp_flash_read_id`. It compiles,
  but has not run on a real chip. `esptool flash-id` is the independent cross-check.
- The pull-up test infers a "strong external pull-up" from how the line reads with the ESP32's
  weaker internal pull-up and pull-down enabled. It is a hint with a reasoned basis, not a
  measurement. A multimeter or oscilloscope would be the real check.
- The analyser and the render check cannot see glitches on the I2C wires that corrupt what the
  glass shows. Only your eyes and the photos can.
- A display unplugged **while running** is not detected by the driver; T4.2 only covers a failed
  initialisation.
- Readability in sunlight, brightness and burn-in are not covered by any test here.
- Only the 128x64 layout is the target. The 128x32 layout is host-tested only.
- Non-ASCII road names (for example Indian scripts) cannot be shown by the 5x7 font. They are
  replaced by `?` or hidden (T4.6 shows both).
- The Android app does not exist yet. The Bluetooth firmware (Phase 2) exists but has never run over a real link; its limitations are in section 5.5.

## 5. Phase 2: Bluetooth LE (prepared; **nothing has run over a real Bluetooth link**)

Phase 2 adds a **separate** firmware environment, `motonav_ble`, that shows navigation messages
received over Bluetooth LE. The protocol is specified in [BLUETOOTH_PROTOCOL.md](BLUETOOTH_PROTOCOL.md).
The Phase 1 environment `motonav`, which T1 to T6 test, is unchanged (section 5.3).

This section was written while the Phase 1 hardware validation was still pending, at the project
owner's request to move forward. **The B tests below must not be run until T1 to T4 have passed**:
a Bluetooth test on a board whose display or chip is unverified would prove nothing.

### 5.1 Status at a glance

| Area | Label | Status |
|---|---|---|
| Frame codec: format, CRC, validation order, every error class | Host-tested | pass |
| Receiver: sequence rules, duplicates, stale frames, resync, reset, counters, STATUS bytes | Host-tested | pass |
| From received bytes to what the display shows | Host-tested | pass |
| Python reference and the firmware's C++ codec agree (45 shared vectors, 3,000 random frames, the hand-test messages below) | Host-tested | pass |
| `motonav_ble` builds on the real ESP32 toolchain | Compile-checked | pass: 881,160 B of 1,310,720 B flash (67.2 %), 38,228 B static RAM, 0 warnings |
| The Phase 1 firmware (`motonav`) is unchanged by Phase 2 | Compile-checked (byte comparison, 5.3) | pass |
| NimBLE radio code: advertising, connecting, MTU, write callbacks, notifications, reconnecting | Device-tested | **NOT RUN** |
| **B1** discovery, **B2** connect and MTU, **B3** valid messages, **B4** invalid messages, **B5** disconnect and reconnect, **B6** sequence rules, **B7** stability | Device-tested | **NOT RUN** |

### 5.2 What the host tests cover (Host-tested)

| Area | Tests |
|---|---|
| CRC | the published check value of CRC-16/CCITT-FALSE (`0x29B1`), chaining |
| Shared vectors | the firmware decodes all 45 vectors made by the independent Python implementation exactly as stated, and its encoder reproduces the 23 valid ones **byte for byte** |
| Codec properties | round trip for every mode and maneuver; every single-bit flip of a valid frame is detected; every truncation and extension is rejected; validation happens in the documented order |
| Fuzzing | 300,000 random buffers (nothing crashes or reads out of bounds under AddressSanitizer); 200,000 structured frames with a correct CRC checked against an independent oracle, and every accepted frame re-encodes to the same bytes |
| Receiver | accepts valid frames and updates state; **a rejected, stale or duplicate frame never changes the state** (also fuzzed); sequence wrap at 255 to 0; the 127 / 128 boundary; resync after three stale frames; reset on connect and disconnect; dropped-frame counting; the STATUS bytes |
| Bytes to display | a frame makes the display show exactly the `expect:` text listed below; invalid frames do not change it; Disconnected beats leftover data; Ready after a reset; an accented road name becomes `?` |
| Python and C++ agree | the same verdict and the same fields on 45 vectors, on the hand-test messages, and on 3,000 random frames |

Bugs were injected one at a time and each was detected (the code was restored and verified
identical afterwards): wrong CRC polynomial, sequence boundary off by one, a NUL byte accepted in a
road name, `reset()` not clearing the counters, the length checked before the version, stale frames
that still overwrite the state, and the Python reference writing a number big-endian.

### 5.3 The Phase 1 firmware is unchanged (Compile-checked)

The Phase 1 hardware tests (T1 to T6) are only meaningful if `motonav` is exactly what was
prepared. A file hash cannot show that, because an ESP32 image contains its build time. Instead the
current tree's `motonav` image was compared byte by byte with an image built from the Phase 1
baseline, commit `3eba30f`, in a clean directory:

- 384,496 bytes each; **66 bytes differ, all in three places**: the 32-byte ELF hash in the app
  descriptor, the 8-character build-time string of the framework's "Software Info" banner (`20:03:16`
  against `20:02:20`), and the image's trailing checksum and SHA-256.
- **0 bytes differ anywhere else** (code, data, strings).
- The new sources are excluded from `motonav` by its build filter, and no Phase 2 object is linked.

To repeat it: build `motonav` from commit `3eba30f` and from the current tree and compare the two
`firmware.bin` files, ignoring those three regions. The Phase 1 baseline commit is **`3eba30f`**.

### 5.4 Before you run the B tests

You need: the Phase 1 tests T1 to T4 passed; an Android phone with Bluetooth LE; and a generic BLE
app that can scan, connect, request an MTU, read and subscribe to a characteristic, and **write a
hex byte array**. Nordic's free *nRF Connect for Mobile* does all of these. Menu names differ
between apps and versions, so the steps below describe what to do, not exact buttons.

```bash
cd firmware
pio run -e motonav_ble -t upload
pio device monitor | tee b.log          # then press EN; Windows: | Tee-Object -FilePath b.log
```

Expected boot output (the chip line shows your board's values):

```text
MotoNav firmware - Phase 2 (BLE, simulated navigation messages)
reset: POWERON
chip: <your chip model> rev <n>, 2 core(s), flash 4194304 bytes
config: 128x64 display at 0x3C, SDA=21 SCL=22
I2C scan:
  device at 0x3C
display: initialised
ble: advertising as MotoNav
```

The display shows the Disconnected screen. If it says `ble: FAILED to start`, stop and send the log.

The test messages come from `protocol/frame_tool.py`. Print them with:

```bash
python ../protocol/frame_tool.py scenarios --compact     # from firmware/; Linux/macOS: python3
```

The lists below are that output, with the exact serial lines and STATUS bytes the firmware's
real receiver produces for them (generated by replaying them through it on a computer, so these
are **predictions from tested code, not recordings from your board**). Paste each hex string as a
**byte array** value into the NAV_DATA characteristic and send it with Write.

**Order matters.** Send the messages in the order listed: each sequence number must be higher than
the last. To start over, disconnect and reconnect (sequence numbers are per connection).

#### B1. Discovery

| Step | Expected |
|---|---|
| Scan with the phone app | a device named **MotoNav** appears within about 10 seconds, advertising service `0ce668c8-034a-476c-9c79-4ed464204496` |

Pass: it appears and shows that service UUID. If it never appears: check the serial log for
`ble: advertising as MotoNav`, that the phone's Bluetooth is on (and its location or Bluetooth-scan
permission is granted), and that it is within a metre or two.

| Date | Tester | Phone and app | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### B2. Connect, MTU and STATUS

| Step | Expected serial output | Expected display |
|---|---|---|
| Connect from the app | `ble: connected` | **BLUETOOTH CONNECTED / READY TO NAVIGATE** |
| Request MTU 247 and wait for it | `ble: MTU 247` (or whatever the phone grants; it must be **at least 56**, otherwise the firmware prints a warning) | unchanged |
| Read the STATUS characteristic `664be4dd-4733-45a1-9820-496f5923a0ba` | | |

Expected STATUS value right after connecting: `01 00 00 00 00 00 00 00` (version 1, nothing
accepted or rejected yet). Subscribe to its notifications for the next tests.

Pass: connects; the display changes to Ready; MTU is at least 56; the STATUS value is as above.

| Date | Tester | MTU granted | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### B3. Valid messages update the display

Send these in order. After each, compare the display with the `expect:` text, and check the STATUS
notification. (Icons and layouts match the T4 reference sheets in `docs/reference/`.)

```text
 1  ready: connected, no navigation (READY screen)
    01010110000000FFFFFFFFFFFFFFFFFFFFFFFF000B3B
    serial  ble: accepted seq 1 -> expect: screen=Ready
    STATUS  01 01 00 01 01 00 00 00

 2  straight_1500: CONTINUE, 1.5 km, Main Street
    0101021B010000DC050000E02E0000080700000B4D61696E20537472656574F324
    serial  ble: accepted seq 2 -> expect: screen=Navigating icon=Straight distance="1.5 km" road="Main Street"
    STATUS  01 02 00 01 02 00 00 00

 3  turn_left_450: TURN LEFT, 450 m, MG Road
    01010317010100C2010000F82A0000A4060000074D4720526F6164AEFE
    serial  ble: accepted seq 3 -> expect: screen=Navigating icon=TurnLeft distance="450 m" road="MG Road"
    STATUS  01 03 00 01 03 00 00 00

 4  turn_right_90: TURN RIGHT, 90 m, MG Road
    010104170102005A0000009029000072060000074D4720526F6164F691
    serial  ble: accepted seq 4 -> expect: screen=Navigating icon=TurnRight distance="90 m" road="MG Road"
    STATUS  01 04 00 01 04 00 00 00

 5  slight_left_300: SLIGHT LEFT, 300 m
    0101051F0103002C010000FFFFFFFFFFFFFFFF0F4F757465722052696E6720526F61641F40
    serial  ble: accepted seq 5 -> expect: screen=Navigating icon=SlightLeft distance="300 m" road="Outer Ring Road"
    STATUS  01 05 00 01 05 00 00 00

 6  sharp_right_120: SHARP RIGHT, 120 m (the display rounds to 100 m)
    0101062301060078000000FFFFFFFFFFFFFFFF134E6174696F6E616C20486967687761792036362546
    serial  ble: accepted seq 6 -> expect: screen=Navigating icon=SharpRight distance="100 m" road="National Highway 66"
    STATUS  01 06 00 01 06 00 00 00

 7  uturn_left_50: U-TURN (left), 50 m
    0101072501070032000000FFFFFFFFFFFFFFFF155061636966696320436F6173742048696768776179F5BB
    serial  ble: accepted seq 7 -> expect: screen=Navigating icon=UTurnLeft distance="50 m" road="Pacific Coast Highway"
    STATUS  01 07 00 01 07 00 00 00

 8  roundabout_exit2_200: ROUNDABOUT exit 2, 200 m
    0101081C010902C8000000FFFFFFFFFFFFFFFF0C53746174696F6E20526F6164AAEF
    serial  ble: accepted seq 8 -> expect: screen=Navigating icon=Roundabout exit=2 distance="200 m" road="Station Road"
    STATUS  01 08 00 01 08 00 00 00

 9  long_road_name: road name cut to fit: 'Sir Mokshagundam V...'
    0101092F01000020030000FFFFFFFFFFFFFFFF1F536972204D6F6B73686167756E64616D205669737665737661726179612052C6FF
    serial  ble: accepted seq 9 -> expect: screen=Navigating icon=Straight distance="800 m" road="Sir Mokshagundam Visvesvaraya R"
    STATUS  01 09 00 01 09 00 00 00

10  rerouting: REROUTING screen
    01010A10020000FFFFFFFFFFFFFFFFFFFFFFFF007F8A
    serial  ble: accepted seq 10 -> expect: screen=Rerouting
    STATUS  01 0A 00 01 0A 00 00 00

11  arrived: DESTINATION REACHED screen
    01010B10030A0000000000FFFFFFFFFFFFFFFF00F544
    serial  ble: accepted seq 11 -> expect: screen=Arrived
    STATUS  01 0B 00 01 0B 00 00 00

12  stopped: navigation stopped: back to READY
    01010C10000000FFFFFFFFFFFFFFFFFFFFFFFF00F2CA
    serial  ble: accepted seq 12 -> expect: screen=Ready
    STATUS  01 0C 00 01 0C 00 00 00
```

Pass: all 12 are accepted; after each the display shows what `expect:` says; STATUS counts
accepted frames up and shows the last sequence; the transitions are clean; no reset.

| Date | Tester | Messages accepted of 12 | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### B4. Invalid messages are rejected safely

Right after B3, send these five. The display must **keep showing the previous screen (Ready)**.
The firmware must not reset or crash.

```text
 1  BAD_crc: must be REJECTED (BadCrc); the display must not change
    01010D1B010100C2010000FFFFFFFFFFFFFFFF0B4D61696E205374726565740DA0
    serial  ble: rejected BadCrc (33 bytes)
    STATUS  01 0C 04 01 0C 00 01 00

 2  BAD_version: must be REJECTED (BadVersion)
    02010E10010100FFFFFFFFFFFFFFFFFFFFFFFF00BC62
    serial  ble: rejected BadVersion (22 bytes)
    STATUS  01 0C 02 01 0C 00 02 00

 3  BAD_truncated: must be REJECTED (BadLength)
    01010D1B010100C2010000FFFFFFFFFFFFFFFF0B4D61696E205374726565
    serial  ble: rejected BadLength (30 bytes)
    STATUS  01 0C 03 01 0C 00 03 00

 4  BAD_maneuver: must be REJECTED (BadPayload)
    01010E10010B00FFFFFFFFFFFFFFFFFFFFFFFF00F556
    serial  ble: rejected BadPayload (22 bytes)
    STATUS  01 0C 06 01 0C 00 04 00

 5  BAD_garbage: must be REJECTED (BadVersion)
    DEADBEEF0102030405
    serial  ble: rejected BadVersion (9 bytes)
    STATUS  01 0C 02 01 0C 00 05 00
```

Then send one valid message to confirm it still works (`python ../protocol/frame_tool.py encode
--seq 13 --mode navigating --maneuver turn_left --distance 450 --road "Main Street"`): it must be
accepted and shown. (The firmware logs at most one rejection line per 100 ms; sending by hand is
slower than that, so every rejection is logged.)

Pass: all five rejected with the stated reason; display unchanged; STATUS rejected count rises by one
each time and the last-error byte matches; no reset; the next valid message is accepted.

| Date | Tester | Rejected of 5 | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### B5. Disconnect and reconnect

1. Disconnect from the phone app.
   Expected: `ble: disconnected (reason N)` (`N` is a BLE reason code), and the display shows
   **BLUETOOTH DISCONNECTED**.
2. Scan again: **MotoNav** must be visible again without resetting the board (it advertises
   again by itself).
3. Reconnect. Expected: `ble: connected` and the Ready screen.
4. Send the first message of B3 again (sequence 1). It must be accepted even though the previous
   connection had reached a higher sequence number: sequence numbers restart per connection.
5. Walk away, or switch the phone's Bluetooth off, to force a connection loss by timeout. Expected: after a
   few seconds (the exact time depends on the phone) `ble: disconnected (reason N)` and the Disconnected screen.

Pass: every step happens as stated and the board never resets.

| Date | Tester | Result | Notes |
|---|---|---|---|
| | | **NOT RUN** | |

#### B6. Sequence rules: duplicate, stale, resync

Continue from a connection where the last accepted sequence is 12 (the end of B3; the invalid
messages in B4 do not change it).

```text
 1  duplicate_of_12: the same message as sequence 12 again: a DUPLICATE, ignored
    01010C10000000FFFFFFFFFFFFFFFFFFFFFFFF00F2CA
    serial  ble: ignored duplicate (seq 12)
    STATUS  01 0C 02 01 0C 00 05 00

 2  stale_3: an OLDER sequence (3): STALE, rejected
    01010318010200BC020000FFFFFFFFFFFFFFFF084F6C6420526F6164601C
    serial  ble: ignored stale (seq 3)
    STATUS  01 0C 07 01 0C 00 06 00

 3  stale_4: another older one (4): STALE, rejected
    010104180102008A020000FFFFFFFFFFFFFFFF084F6C6420526F61642436
    serial  ble: ignored stale (seq 4)
    STATUS  01 0C 07 01 0C 00 07 00

 4  resync_5: the THIRD stale one in a row (5): accepted as a resync, display changes
    0101051801040058020000FFFFFFFFFFFFFFFF084E657720526F6164BF74
    serial  ble: accepted seq 5 -> expect: screen=Navigating icon=SlightRight distance="600 m" road="New Road"
    STATUS  01 05 07 01 0D 00 07 00

 5  continues_6: sequence 6 follows normally
    0101061801000026020000FFFFFFFFFFFFFFFF084E657720526F6164309F
    serial  ble: accepted seq 6 -> expect: screen=Navigating icon=Straight distance="550 m" road="New Road"
    STATUS  01 06 07 01 0E 00 07 00
```

Pass: exactly the serial lines above; the duplicate and the first two stale messages change nothing
on the display; the third stale message is accepted (the phone restarted its counter) and changes
the display.

| Date | Tester | Result | Notes |
|---|---|---|---|
| | | **NOT RUN** | |

#### B7. Stability

Stay connected for **10 minutes**, sending a message every minute or so. Watch the serial log.

Pass: the boot banner appears only once; no `reset:` line after the first; the once-a-minute
`[stats]` lines keep `heap_free` steady (it will be lower than in Phase 1, because the Bluetooth
stack uses memory: **record the number**, it is not yet known); the `[ble]` lines show the
counts you expect.

Do **not** run `analyze_log.py` on a Bluetooth log: it expects the display to change constantly (as
in the Phase 1 simulation) and would report an idle display as frozen.

| Date | Tester | Run length | heap_free (start / end) | Result | Notes |
|---|---|---|---|---|---|
| | | | | **NOT RUN** | |

**Evidence to send for the B tests:** `b.log`; the app's screen showing the scan result, the MTU
and the STATUS values; photos of the display for B3 (at least the first, a middle and the last
message) and B4.

### 5.5 Known limitations of Phase 2

- **No Bluetooth test has been run.** The radio code (`src/ble_link.cpp`) is thin: it only advertises,
  accepts one phone, queues writes and publishes STATUS. Everything it decides is in tested code, but
  whether it advertises, connects, negotiates, receives and reconnects on a real board is unknown.
- **No security.** No pairing or encryption: anyone in range can connect and write (see
  BLUETOOTH_PROTOCOL.md section 11).
- **No stale-data detection** or heartbeat: if the phone stops sending, the display keeps the last
  state. Phase 5.
- **The phone must negotiate the MTU** (at least 56). One phone at a time.
- **The first frame written in the instant of connecting may be discarded** (queued writes are
  cleared on connect). The phone should send the current state after the MTU exchange.
- **Memory:** the Bluetooth stack's run-time heap cost on this board is unknown until B7 is run.
  Flash use is 67 % of the application partition.
- **The firmware logs one `expect:` line per accepted message.** At one update a second that is a lot
  of lines; acceptable for development.
- **There is no Android app yet.** The messages are sent by hand from a generic BLE app.
- **The Phase 1 and Phase 2 firmware duplicate about 100 lines of start-up code.** `main.cpp` was
  deliberately left untouched so the Phase 1 validation stays valid; once it passes, the two can be
  merged onto `src/device_common.h`.

## 6. Phase 3: Android test sender (prepared; **it has never run on a phone**)

Phase 3 adds an Android project in `android/` with a small test app. It finds a MotoNav over
Bluetooth LE, connects, and sends the simulated navigation messages of section 5. It is **not**
the navigation app: the Google Navigation SDK was added afterwards (Phase 4a, section 7). How the project is built and laid out
is in [android/README.md](android/README.md).

Nothing in this section has run on a phone, and nothing has run against a MotoNav. The protocol
and the connection logic are tested on a computer with a fake in place of Bluetooth. The part that
talks to the phone's Bluetooth stack was only built.

### 6.1 Status at a glance

| Area | Label | Status |
|---|---|---|
| Protocol in Kotlin: codec, CRC, validation order, STATUS parser, road-name fitting, the simulated messages (106 tests) | Host-tested | pass |
| The Kotlin codec against the shared files: all 45 vectors give the stated result, its encoder reproduces the 23 valid ones **byte for byte**, and its 17 simulated messages are identical to the Python tool's, byte for byte | Host-tested | pass |
| Connection logic: set-up order, MTU rule, one write at a time, newest state wins, sequence per connection, resend after reconnecting, timeouts, every failure, plus 40 randomised runs of 2,000 events (32 tests) | Host-tested | pass |
| The road name is never written to the system log, files, preferences or a database (a check on the sources) | Host-tested | pass |
| The app builds: 0 compiler warnings, Android lint clean with warnings treated as errors, debug APK about 64.6 MB (the exact size varies with the build machine) | Compile-checked | pass |
| The built APK declares the Bluetooth permissions Google's documentation prescribes (read back with `aapt2`) | Compile-checked | pass |
| `BluetoothGatt` adapter, scanner, permission flow, screen: scanning, connecting, MTU, writes, notifications, reconnecting | Device-tested | **NOT RUN** |
| **P1** install, **P2** permissions, **P3** find and connect, **P4** simulated states, **P5** play all, **P6** wrong messages, **P7** disconnect and failures, **P8** a second phone | Device-tested | **NOT RUN** |

### 6.2 What the Kotlin tests cover (Host-tested)

Run from `android/`: `./gradlew :protocol:test :link:test`. JUnit 6.1.3, no phone and no Android SDK needed.

| Module | Tests |
|---|---|
| `:protocol` | the CRC check value `0x29B1`; every vector in `protocol/vectors.txt` decodes to the stated result and fields; the encoder reproduces every valid vector byte for byte; the worked example in BLUETOOTH_PROTOCOL.md; every mode, maneuver and exit survives a round trip; every single-bit flip is detected; every truncation and extension is rejected; the documented order of checks; 200,000 random buffers never crash the decoder; 100,000 frames with a correct CRC and one injected defect each get exactly the injected verdict, and every accepted frame re-encodes identically; the error codes, UUIDs, maneuver numbers and size limits equal the ones in BLUETOOTH_PROTOCOL.md and in the firmware's `frame.h` and `ble_link.cpp`; road names are cut between characters, never inside one; STATUS parsing |
| `:link` | set-up in the documented order; an MTU of 56 is accepted and 55 or 23 ends the link with a clear reason and **nothing is ever written**; missing service; refused operations; sequence numbers from 0, wrapping after 255, restarting on a new connection; while a write is in flight only the newest update is kept; the newest state is sent again after (re)connecting; a refused or failed write; stopping cleanly; timeouts for every step; late or unexpected callbacks are ignored; and the random-event runs, which check that nothing is written unless the link is Ready, never two writes at once, and the sequence rises by one |

Bugs were injected one at a time, each was detected, and the code was restored (and verified identical):

| Injected bug | Failing tests |
|---|---|
| `:protocol` wrong CRC polynomial | 61 |
| `:protocol` 32-bit numbers written big-endian | 24 |
| `:protocol` length checked before version | 2 |
| `:protocol` road name cut in the middle of a character | 1 |
| `:protocol` one simulated message changed (450 m to 451 m) | 1 |
| `:protocol` U-turn right numbered 7 instead of 8 | 5 |
| `:link` any MTU accepted | 2 |
| `:link` a write started while another is in flight | 3 |
| `:link` sequence numbers not restarted on a new connection | 1 |
| `:link` newest state not sent again after set-up | 1 |
| `:link` an MTU change cancels the timeout of a write in flight | 1 |
| `:link` writes allowed before the link is Ready | 3 |

(One of these, the MTU change cancelling a write's timeout, was a real bug found by reading the
code before the test existed; the test was written for it.) The source check for the road-name
rule was also shown to fail when a `Log.d` call is added.

### 6.3 Build and compile check (Compile-checked)

| Item | Value |
|---|---|
| Android Gradle Plugin | 9.5.1 (supports API 37, which the Navigation SDK needs; Phase 4) |
| Gradle | 9.6.0, wrapper pinned with its published SHA-256; the wrapper JAR matches Gradle's published checksum |
| Kotlin | 2.2.10 (the version AGP 9.5.1 depends on) |
| compileSdk, targetSdk, minSdk | 37, 37, 26 (Android 8.0 and later) |
| Build Tools | 36.0.0 |
| JDK | 21 here; 17 or newer needed |
| Warnings | 0 compiler warnings (treated as errors), 0 lint findings (treated as errors) |

The Android SDK licence had to be accepted to install the SDK in the cloud sandbox where this was built.

What the compile check does **not** show: that the Bluetooth calls behave on a real phone. It
shows that they exist in the API and are used with the right types.

### 6.4 Before you run the phone tests

You need: a MotoNav board running the `motonav_ble` firmware with the Phase 1 tests **T1 to T4**
and the Phase 2 tests **B1 and B2** passed (so you already know the board advertises and accepts
a connection from a known-good generic app); an Android phone with Bluetooth LE running Android 8
or later; and a way to put the APK on the phone.

Build and install (see android/README.md for the SDK setup):

```bash
cd android
./gradlew :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk       # phone with USB debugging; or copy the APK to the phone
```

Keep the board's serial monitor open (`pio device monitor | tee p.log` in `firmware/`): the board
prints a line for every message it receives, and the pass criteria compare the app with those lines.
Expected board output is the same as in section 5; sequence numbers differ because this app
numbers its frames from 0 on every connection.

### 6.5 The tests

#### P1. Install and start

Open the app. Expected: a screen titled "MotoNav test sender" with the state **Not connected**,
a **Find and connect** button, and a log that is empty. Nothing happens by itself.

Pass: it starts without crashing and rotates without losing the screen.

| Date | Tester | Phone, Android version | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### P2. Permissions

Tap **Find and connect**. Expected: Android asks for permission to find nearby devices (Android
12 and later: "Nearby devices"; Android 11 and earlier: Location). After you allow it, scanning
starts. If you refuse, the log says the permission was refused and nothing else happens.

Pass: both behaviours. (Refuse once, then allow it from the system settings and try again.)

| Date | Tester | Phone, Android version | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### P3. Find and connect

With the board powered and advertising ("ble: advertising as MotoNav" on serial), tap **Find and
connect**. Expected, in the app's log and state line, in this order: scanning; found a MotoNav
(with a signal strength); then the state passes through **Connecting**, **Looking for the MotoNav
service**, **Negotiating the message size**, **Turning on status messages** and ends at
**Connected and ready (MTU n)** with `n` at least 56. Expected on the board: `ble: connected`,
then `ble: MTU n` (the same `n`), and the display shows **BLUETOOTH CONNECTED / READY TO NAVIGATE**.

Pass: all of that. **Write down the MTU.** The **Device status** line still says "(none yet)": the
board sends a status value after each message it handles, and from reading the firmware its
notification at connect time should go out before the app has turned notifications on (not observed). The first status appears in P4, for example
`status(v1, last seq 0, last error None, accepted 1, rejected 0)`.

If no MotoNav is found: check the board (serial), that the phone's Bluetooth and permission are on, and that you are within a metre or two. If it ends at **FAILED**, the text says why (for example MtuTooSmall); send it along with the board log.

| Date | Tester | Phone, Android version | MTU granted | Result | Notes |
|---|---|---|---|---|---|
| | | | | **NOT RUN** | |

#### P4. Simulated states

Choose each entry in the **Send a simulated state** list and tap **Send this state**. After
each, the board prints `ble: accepted seq N -> expect: ...` with the text below (`N` rises by
one for each message sent, starting at 0 on every connection), the display shows what that text
says, and the app logs `device acknowledged frame N`. The Device status line shows the accepted
count rising. (The icons and layouts match the T4 reference sheets in `docs/reference/`.)

| Entry | What it is | The board must print (after `-> expect: `) |
|---|---|---|
| `ready` | connected, no navigation (READY screen) | `screen=Ready` |
| `straight_1500` | CONTINUE, 1.5 km, Main Street | `screen=Navigating icon=Straight distance="1.5 km" road="Main Street"` |
| `turn_left_450` | TURN LEFT, 450 m, MG Road | `screen=Navigating icon=TurnLeft distance="450 m" road="MG Road"` |
| `turn_right_90` | TURN RIGHT, 90 m, MG Road | `screen=Navigating icon=TurnRight distance="90 m" road="MG Road"` |
| `slight_left_300` | SLIGHT LEFT, 300 m | `screen=Navigating icon=SlightLeft distance="300 m" road="Outer Ring Road"` |
| `sharp_right_120` | SHARP RIGHT, 120 m (the display rounds to 100 m) | `screen=Navigating icon=SharpRight distance="100 m" road="National Highway 66"` |
| `uturn_left_50` | U-TURN (left), 50 m | `screen=Navigating icon=UTurnLeft distance="50 m" road="Pacific Coast Highway"` |
| `roundabout_exit2_200` | ROUNDABOUT exit 2, 200 m | `screen=Navigating icon=Roundabout exit=2 distance="200 m" road="Station Road"` |
| `long_road_name` | road name cut to fit: 'Sir Mokshagundam V...' | `screen=Navigating icon=Straight distance="800 m" road="Sir Mokshagundam Visvesvaraya R"` |
| `rerouting` | REROUTING screen | `screen=Rerouting` |
| `arrived` | DESTINATION REACHED screen | `screen=Arrived` |
| `stopped` | navigation stopped: back to READY | `screen=Ready` |

Pass: all 12 accepted, the display matches each line, no `rejected` count, no reset.

If the app is tapped faster than the device acknowledges, the older state is dropped on purpose
(only the newest matters), so a count of frames can be lower than the number of taps.

| Date | Tester | Phone, Android version | States shown correctly (of 12) | Result | Notes |
|---|---|---|---|---|---|
| | | | | **NOT RUN** | |

#### P5. Play all

Tap **Play all, one a second**. Expected: the twelve states are sent one a second, over and over,
and the display changes with them, about the rate Google's turn-by-turn feed will have. Let it
run for **two minutes**, then tap **Stop playing**.

Pass: the display follows without freezing or flicker; the board does not reset; the Device status
`rejected` count stays at 0; the app stays connected.

| Date | Tester | Phone, Android version | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### P6. Wrong messages are rejected

The app can send deliberately wrong messages. After P4, choose each entry in **Wrong messages**
and tap **Send this wrong message**. The display must keep showing what it showed; the board must
not reset.

| Entry | The board must print | Last-error byte in the status |
|---|---|---|
| `BAD_crc` | `ble: rejected BadCrc (33 bytes)` | `04` |
| `BAD_version` | `ble: rejected BadVersion (22 bytes)` | `02` |
| `BAD_truncated` | `ble: rejected BadLength (30 bytes)` | `03` |
| `BAD_maneuver` | `ble: rejected BadPayload (22 bytes)` | `06` |
| `BAD_garbage` | `ble: rejected BadVersion (9 bytes)` | `02` |

Pass: each is rejected with the stated reason, the display does not change, the Device status
`rejected` count rises by one each time, and a normal state sent afterwards is accepted.

| Date | Tester | Phone, Android version | Rejected correctly (of 5) | Result | Notes |
|---|---|---|---|---|---|
| | | | | **NOT RUN** | |

#### P7. Disconnecting and failures

1. Tap **Disconnect**. Expected: the app shows **Disconnected**; the board prints `ble: disconnected (reason n)` and shows **BLUETOOTH DISCONNECTED**.
2. Tap **Find and connect** again. Expected: it connects as in P3, and the first state you send is accepted with `seq 0` (sequence numbers restart), even though the previous connection went higher.
3. While connected, switch the phone's Bluetooth off. Expected: the app shows **FAILED: ConnectionLost** with a reason; the board shows the disconnected screen.
4. Switch Bluetooth on, tap **Find and connect**. Expected: it connects again.
5. While connected, power the board off. Expected: after a few seconds the app shows **FAILED: ConnectionLost**. (The time depends on the phone.)
6. Tap **Find and connect** with the board switched off. Expected: after about 15 seconds the log says no MotoNav was found.

Pass: every step happens as stated, and the app never freezes or crashes.

| Date | Tester | Phone, Android version | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### P8. A second phone

Repeat P3 to P7 on a phone from a different maker, ideally with a different Android version.
Bluetooth behaviour differs between makers (message size, timing, background limits), and one
phone proves little.

| Date | Tester | Phone maker and model, Android version | MTU granted | Result | Notes |
|---|---|---|---|---|---|
| | | | | **NOT RUN** | |

**Evidence to send for the phone tests:** `p.log` (the board's serial log); screenshots of the app
(after P3, P4, P6 and after a failure in P7); photos of the display for P4 (at least the first, a
middle and the last state); the phone's maker, model and Android version.

### 6.6 Known limitations of Phase 3

- **The phone side has never run.** Scanning, the permission flow, connecting, MTU negotiation, writes,
  notifications and the screen are untested. The logic they feed is tested; their behaviour on a
  real Bluetooth stack is unknown.
- **No reconnection.** If the connection drops, the app reports it and stops. Automatic
  reconnection, keeping the link alive with the screen off (a foreground service), and Android's
  background limits are Phase 5.
- **The connection belongs to the screen.** Closing the app ends it.
- **It connects to the first MotoNav it finds.** With two boards in range it may pick either.
- **The deprecated `connectGatt` call is used** (see DECISIONS.md D-49).
- **Placeholder application id and icon** (`com.motonav.app`): to be decided before a release (D-45).
- **English only**, and the screen is a test tool, not a product UI.
- **The messages in P1 to P8 are simulated.** Real navigation data from Google is section 7.
- **No security** on the link: see BLUETOOTH_PROTOCOL.md section 11.

## 7. Phase 4a: Google Navigation SDK (prepared; **none of the Google parts has ever run**)

Phase 4a adds the code that turns Google's turn-by-turn feed into the messages of section 5, in a new
Android module `:navsdk`: a conversion layer, the service the SDK feeds, and a controller that starts
a route (optionally a simulated drive). It is wired into the test screen of section 6.

"4a" means everything that can be done and proven **without** a Google account, an API key, billing
or a phone. "4b" is the rest, which only you can do: section 7.6 (tests N1 to N7).

What is proven, and what is not, in one place:

- The Navigation SDK's own code has **never run**. It was downloaded, compiled against, and its
  class files were read. No request has been made to Google; no key exists in this project.
- The **conversion** from Google's data to our messages is tested with the SDK's own data classes, and
  its output goes through the firmware's real receiver on a computer (section 7.2).
- Whether Google's feed behaves as its documentation says (rate, content, arrival, what happens with
  the screen off) is **unknown** until N1 to N5 are run.

### 7.1 Status at a glance

| Area | Label | Status |
|---|---|---|
| All 66 Google maneuvers map onto the 11 device icons; a test reads the constants from the SDK itself and fails if one is missing (27 tests in `:navsdk`) | Host-tested | pass |
| Google feed message to device message: states, distances, missing and negative values, road names, roundabout exits, arrival (real SDK `NavInfo`/`StepInfo` objects, plus 20,000 random messages that must give nothing or a frame the device accepts) | Host-tested | pass |
| 17 reviewed feed messages: Kotlin frames equal the shared golden file, and the firmware's real receiver shows the reviewed screen text (`protocol/navinfo_cases.txt`) | Host-tested | pass |
| Source checks: Google types only in `:navsdk`; no API key committed; fine location declared on every Android version; no logging or storage of road names | Host-tested | pass |
| The app builds with the SDK (0 compiler warnings, lint clean, warnings are errors); debug APK about 65 MB | Compile-checked | pass |
| SDK feed service, route controller (`setDestinations`, two-wheeler mode, simulator, arrival listener), terms and permission flow, API-key handling | Compile-checked | pass (built against the real SDK classes) |
| SDK start-up, key check, terms dialog, route calculation, the feed, arrival event, simulated drive | Device-tested | **NOT RUN** |
| **N1** SDK start, **N2** simulated route, **N3** phone to board end to end, **N4** U-turn direction, **N5** screen off, **N6** a real ride, **N7** cost check | Device-tested | **NOT RUN** |

### 7.2 What the tests cover (Host-tested)

Run from `android/`: `./gradlew :navsdk:testDebugUnitTest`. No phone, key or network is used. The SDK's classes are on the class path
only as data (`NavInfo`, `StepInfo` and their builders).

| Test | What it checks |
|---|---|
| Maneuver table | there are 66 SDK maneuvers numbered 0 to 65; every one is in the table; the groups are the ones in ARCHITECTURE.md; left is never shown as right; counterclockwise U-turns are the left icon and clockwise the right one; every roundabout constant is a roundabout |
| Converter | a normal turn with every field; a missing or negative number is "unknown", never 0; the largest number the SDK can give is not confused with "unknown"; simple road name preferred, full name as fallback, cut between characters; roundabout exit number (none, negative or 5,000 handled; ignored on other maneuvers); REROUTING, STOPPED (clears the device so no old turn lingers) and UNKNOWN; an ENROUTE message without a step is ignored, not guessed; arrival from the SDK's arrival event, kept until a new route, ended by rerouting |
| Random | 20,000 random messages (random states, maneuvers including values the SDK does not have, missing, negative and huge numbers, empty, NUL-containing, non-Latin and lone-surrogate names): the result is nothing, or a frame the device codec accepts and decodes to the same update |
| Golden frames | 17 feed messages produce exactly the frames in `protocol/navinfo_cases.txt` |
| Python replay | `test_navinfo_cases.py` sends those frames through the firmware's real C++ receiver and compares the text of what the display would show with the third column of the file, which was written and reviewed by hand |

What the 17 reviewed cases show (the display text is from the firmware's real receiver):

| Case | What the display shows |
|---|---|
| `straight_1200` | `screen=Navigating icon=Straight distance="1.2 km" road="Ring Road"` |
| `turn_left_450` | `screen=Navigating icon=TurnLeft distance="450 m" road="MG Road"` |
| `ramp_right_300` | `screen=Navigating icon=TurnRight distance="300 m" road="NH 48"` |
| `keep_left_200` | `screen=Navigating icon=SlightLeft distance="200 m" road="Outer Ring Road"` |
| `fork_right_900` | `screen=Navigating icon=SlightRight distance="900 m" road="Mysore Road"` |
| `sharp_right_120` | `screen=Navigating icon=SharpRight distance="100 m" road="Church Street"` |
| `uturn_counterclockwise_80` | `screen=Navigating icon=UTurnLeft distance="80 m" road="Brigade Road"` |
| `uturn_clockwise_80` | `screen=Navigating icon=UTurnRight distance="80 m" road="Brigade Road"` |
| `roundabout_exit3_200` | `screen=Navigating icon=Roundabout exit=3 distance="200 m" road="Station Road"` |
| `destination_approach_60` | `screen=Navigating icon=Arrived distance="60 m" road="Park Street"` |
| `distance_unknown` | `screen=Navigating icon=TurnRight distance="--" road="MG Road"` |
| `road_name_full_fallback` | `screen=Navigating icon=TurnLeft distance="150 m" road="Mahatma Gandhi Road"` |
| `road_name_non_latin` | `screen=Navigating icon=TurnLeft distance="250 m" road=""` |
| `road_name_long` | `screen=Navigating icon=Straight distance="5.0 km" road="Sir Mokshagundam Visvesvaraya R"` |
| `rerouting` | `screen=Rerouting` |
| `stopped` | `screen=Ready` |
| `arrived` | `screen=Arrived` |

Two things in that table deserve attention. `sharp_right_120` shows 100 m because the display rounds distances
(D-22). `road_name_non_latin` shows **no road name**: the display font is ASCII only (D-20), and a rider in India
will often get road names in Indian scripts. That is a known limitation, not a conversion bug.

Bugs were injected one at a time and each was detected (the code was restored and verified identical):

| Injected bug | Failing tests |
|---|---|
| clockwise U-turn shown as a left U-turn | 1 |
| one SDK maneuver left out of the table | 1 |
| slight left shown as slight right | 2 |
| a negative distance sent as 0 | 1 |
| STOPPED keeps showing the last turn | 3 |
| full road name preferred over the simple one | 3 |
| rerouting does not end the arrived state | 1 |
| an unknown navigation state treated as Ready | 1 |

The source checks were each shown to fail when their rule is broken: fine location capped at Android 11, an
API-key-shaped string in a source file, a Google import in the app module.

### 7.3 What was read from the SDK itself (Verified, 2026-11-08)

The Navigation SDK 8.1.0 package was downloaded and its class files read with `javap`; Google's reference pages and guides
were read as well. Facts the code relies on:

| Fact | Source |
|---|---|
| `TurnByTurnManager.MSG_NAV_INFO` is 7200; `createInstance()` and `readNavInfoFromBundle(Bundle)` decode a feed message | class files, guide |
| The feed arrives "about once a second"; the SDK binds to our service and sends through a `Messenger`; the service starts when registered | guide |
| `NavInfo` and `StepInfo` have public builders; `NavState` is `UNKNOWN`, `ENROUTE`, `REROUTING`, `STOPPED` | class files |
| During REROUTING "the upcoming maneuver step is not available"; STOPPED means "navigation has ended" | guide |
| The SDK has 66 maneuvers (0 to 65). U-turns are "clockwise" or "counterclockwise turn onto the opposite side of the street", and the SDK picks the label from the country's driving side | class files, reference |
| `Navigator.ArrivalListener.onArrival(ArrivalEvent)` with `isFinalDestination()` exists, so arrival does not have to be guessed | class files, guide |
| `RoutingOptions.TravelMode.TWO_WHEELER` exists (value 3); `Simulator` can place a pretend vehicle and drive the route (`speedMultiplier`) | class files, guide |
| The feed options default to no preview steps and no step images; the guide's example asks for bitmaps, which we do not want | class files |
| `setDestinations(list, routingOptions)` is deprecated; the replacement takes a `RouteDescriptionLoadCallback` | reference |
| Core library desugaring (`desugar_jdk_libs_nio` 2.1.5) is required in the app and in every module that uses the SDK | build failure, guide |
| The API key goes in a manifest `<meta-data android:name="com.google.android.geo.API_KEY">`; Google suggests a git-ignored `secrets.properties` | guide |
| The SDK's own manifest adds, among others: `ACCESS_FINE_LOCATION`, `ACCESS_COARSE_LOCATION`, **`ACCESS_BACKGROUND_LOCATION`**, `POST_NOTIFICATIONS`, `FOREGROUND_SERVICE_LOCATION`, `INTERNET`, `READ/WRITE_EXTERNAL_STORAGE`, `RECEIVE_BOOT_COMPLETED`, `WAKE_LOCK` | merged manifest of the built APK |

Not matching what Google lists: Google's table for SDK 8.x names Gradle 9.1.0 and AGP 9.0.1; this project uses Gradle 9.6.0 and
AGP 9.5.1 (needed for API 37). It builds, but whether Google has tested that combination is unknown.

A bug was found by reading the merged manifest: the Phase 3 declaration `ACCESS_FINE_LOCATION maxSdkVersion="30"` won the manifest merge
and removed the permission for Android 12 and later, so navigation could not have started there. It is declared on every version now,
and a test guards it.

### 7.4 Build and configuration

```bash
cd android
./gradlew :navsdk:testDebugUnitTest                      # the conversion tests
./gradlew :app:lintDebug :app:assembleDebug              # build and lint everything
```

The API key is a secret and is **never committed**. Put it in `android/secrets.properties` (git-ignored) as `MAPS_API_KEY=...`, or set the
environment variable `MAPS_API_KEY`. Without either, the build still works with a placeholder (`DEFAULT_API_KEY`) and the SDK will refuse
to start with "refused the API key", which the screen reports. A test fails if a Google API key appears anywhere in the repository.

The debug APK is about 65 MB (it was under 1 MB before the SDK), because the SDK carries native libraries and many dependencies.

### 7.5 Before you run the tests N1 to N7

You need all of these, and only you can arrange them:

- A **Google Cloud project** with the **Navigation SDK** enabled and an API key. Restrict the key to this app (Google's page on API
  keys describes application restrictions; this was not tested here).
- **Billing turned on.** The Navigation SDK is billed per destination (ARCHITECTURE.md section 2). Read Google's pricing page first. Every
  route you start in N2 to N6 can count as a billable destination, simulated or not (not verified: check the usage page after N2).
- Acceptance of **Google's Navigation terms of use** on the phone (the SDK shows the dialog).
- An Android phone with location turned on. (Whether the SDK needs Google Play services on the phone was not checked; the SDK bundles its own copy of the map classes it uses and declares no Play services dependency.)
- For N3: a MotoNav board and the Phase 2/3 tests **B1, B2 and P3** passed, so the Bluetooth chain is already known to work.

### 7.6 The tests

#### N1. Start the Navigation SDK

Put the key in place (7.4), build, install. Tap **1. Start the Navigation SDK**. Expected: Android asks for location permission; then
Google's terms dialog appears; after you accept, the log says `starting the Navigation SDK` then `Navigation SDK ready`.
Failure messages tell you why: `refused the API key`, `the Navigation terms of use were not accepted`, `network error`, `location permission is missing`.

Pass: it reaches "Navigation SDK ready". Also try once without the key to see the "refused the API key" message.

| Date | Tester | Phone, Android version | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### N2. A simulated route, no board

Enter a **destination** (latitude and longitude, for example somewhere you know) and, optionally, a **start** so no GPS fix is needed.
Leave **Simulated drive** ticked. Tap **2. Start the route**. Expected log: `asking Google for a two-wheeler route`, then
`turn-by-turn feed registered`, then `guidance started (simulated drive, 5x speed)`. The line **Navigation feed: received N, sent to the
device N, ignored N, unreadable N** should count up about once a second. `unreadable` should stay 0.

Pass: the route is accepted, the feed counter rises roughly once a second, `ignored` stays low. **Write down** how many messages arrive in one
minute, and whether `ignored` is ever above 0. If you see `no route: ...`, the text says why (for example `LOCATION_UNKNOWN` without a start).

| Date | Tester | Phone, Android version | Messages in 1 minute | Result | Notes |
|---|---|---|---|---|---|
| | | | | **NOT RUN** | |

#### N3. Phone to board, end to end

Connect to the board (P3), then do N2. Watch the display and the board's serial log (`pio device monitor | tee n.log`). Expected: the board prints
`ble: accepted seq N -> expect: ...` about once a second; the display shows the maneuver icon, the distance (counting down), and the road name;
the icon changes at each turn of the route; at the end the **Arrived** screen appears; **Stop the route** brings back **Ready**.

Pass: the display agrees with the route you know (icons plausible, distances falling), no board reset, no rejected messages, Arrived at the end.
Roads in non-Latin script will show no road name (known limitation).

| Date | Tester | Route (length, region) | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### N4. U-turn direction

On a route with a U-turn, check the icon points the way the real turn goes. In right-hand traffic a U-turn is to the left; in left-hand traffic to
the right. The mapping follows Google's wording (clockwise is the right-hand icon) and is **unverified on a real road**. Record region,
Google's constant if you can see it (the board shows only the icon), and whether the icon was right.

| Date | Tester | Region and traffic side | Icon correct? | Result | Notes |
|---|---|---|---|---|---|
| | | | | **NOT RUN** | |

#### N5. Screen off

Run N3. Switch the phone's screen off (or put the app in the background) for **five minutes**, then look at the display and the app. This is a
question, not a pass or fail: does the feed keep arriving, and does the Bluetooth link stay up? This is the highest-value unknown of the project
(DEVELOPMENT_PLAN.md risk 6). Record exactly what happens, and the phone maker and Android version (battery saving rules differ).

| Date | Tester | Phone maker, model, Android | Feed continued? | Link stayed up? | Notes |
|---|---|---|---|---|---|
| | | | | | **NOT RUN** |

#### N6. A real ride (only after N3 and N5)

A short, known route with real GPS (untick Simulated drive). **Safety:** fit the display and the phone before moving; do not touch the phone while riding; have a
passenger or stop to read results. Record anything odd: late turns, wrong icons, freezes, the display after a tunnel or a GPS loss.

| Date | Tester | Route | Result | Notes |
|---|---|---|---|---|
| | | | **NOT RUN** | |

#### N7. Cost check

After N2 to N6, open the Google Cloud usage and billing pages and write down how many destinations were billed. This tells you what testing costs.

| Date | Tester | Routes started | Destinations billed | Cost | Notes |
|---|---|---|---|---|---|
| | | | | | **NOT RUN** |

**Evidence to send:** `n.log`; screenshots of the app after N1, N2 and N5; photos of the display for N3; the Google Cloud usage page for N7; phone maker, model and Android version.

### 7.7 Known limitations of Phase 4a

- **The Google parts have never run** (7.1). Everything about real feed behaviour is documentation, not observation.
- **Needs a Google key, billing and terms.** Per-destination billing; commercial-terms questions are still open (DEVELOPMENT_PLAN.md section A), including whether a
  separate hardware display needs Google Maps attribution.
- **Arrival depends on the SDK's arrival event** being delivered. If it is not, the device goes from the last turn to Ready (when STOPPED arrives) without the Arrived screen.
  Only one destination is supported; waypoints are not.
- **The feed and the link live and die with the test screen**: no background operation, no automatic reconnection (Phase 5). N5 measures what actually happens.
- **The SDK adds broad permissions** to the app, including background location, notifications and storage. They will matter for Google Play review. The SDK may also store
  data of its own; D-05 (no road names stored) is checked for our code, not for the SDK's internals.
- **Indian-script road names are not shown** (D-20).
- **Remaining distance and time** are sent but not displayed (D-21).
- **Whether the SDK needs Google Play services on the phone** at run time was not checked.
- **Version combination:** see 7.3. The app, SDK and Android Gradle Plugin versions are newer than Google's documented table.
