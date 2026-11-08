# Testing

Every result below is labelled with where it was obtained:

- **Host-tested**: automated tests or rendered images on a normal computer
  (here: Linux, gcc 13.3 and clang 18.1). Says nothing about the physical display.
- **Compile-checked**: the device-side code was built by the ESP32 toolchain. Says
  nothing about whether it runs.
- **Device-tested**: run on the real board and display. **Nothing is
  device-tested yet.** The board and display have not been identified (see
  [HARDWARE.md](HARDWARE.md)), and the AI assistant has no access to hardware.

Last updated after the hardware was provisionally identified (2026-11-08). Phase 1 device tests and the hardware validation have not been run.

## 1. Host tests

Run from `firmware/host` (needs only `g++` and `make`):

```bash
make test                          # layering check + all tests, gcc, ASan + UBSan, -Werror
make -B test CXX=clang++ SANITIZE= # second compiler, no sanitizers
make render                        # images in out/ (needs Python + Pillow for the contact sheet)
make golden-update                 # accept a deliberate rendering change (look at the images first!)
```

### Results (Host-tested)

| Run | Result |
|---|---|
| gcc 13.3, `-std=c++11 -Wall -Wextra -Wpedantic -Werror`, AddressSanitizer + UBSan | **34 tests, 38,930 checks, 0 failed** |
| clang 18.1, same warnings, no sanitizers | **34 tests, 38,930 checks, 0 failed**; golden hashes identical to gcc |
| Layering check (`check_layering.sh`) | ok |

clang could not be run with sanitizers: this sandbox's clang has no AddressSanitizer
runtime library. gcc covered the sanitizers.

### What the tests cover

| Area | Tests |
|---|---|
| Distance formatting | 33 boundary cases (0, 4, 5, 99, 100, 974, 975, 1000, 9949, 9950, 999,500, near `uint32` max, unknown); over 0-30,000 m the shown value never goes backwards and never differs from the truth by more than half a display step |
| Text | width arithmetic; fitting with `...`, hard cut, tiny buffers |
| View building | screen selection; disconnected beats stale navigation data; road-name cleanup (UTF-8, Malayalam, cut mid-character, control characters, no letters); roundabout exit only for roundabouts; two states that look identical give equal views |
| Rendering | every screen at 128x64 and 128x32 draws inside the canvas, is neither blank nor solid; rendering is deterministic and leaves no residue from a previous screen; all 11 icons pairwise different (smallest difference 440 pixels); left/right icons mirror each other; every turn icon leans towards its side; roundabout shows its exit number; the distance is the dominant text; no road strip without a road; compact layout; "ARRIVED" does not overlap the icon on 128x32 |
| Flicker policy | first frame draws, identical or equal-looking states do not; stale data while disconnected does not; `invalidate()` forces a redraw; over **two full simulation loops (1,720 ticks) exactly 604 frames are presented, equal to the number of visible changes counted independently**, and every presented frame equals a fresh render |
| Simulation | loop length; wraps; visits all 5 screens and all 11 maneuvers; distance counts down within each maneuver; serial command controller |
| Regression | golden hashes of 34 screens (`golden/screens.txt`) |
| Architecture | `check_layering.sh` fails if `core/`, `sim/` or `ui/` include Arduino, Wire, a display driver or a radio header |

### Do the tests actually catch bugs? (Host-tested)

Four bugs were injected one at a time and each was detected, then the code was
restored and the suite passed again:

| Injected bug | Caught by |
|---|---|
| Distance rounded to 100 m instead of 50 m steps | `distance_boundaries` (observed failing) |
| Presenter always redraws (flicker) | `presenter_draws_first_frame_then_only_on_visible_change` (observed failing) |
| `#include <Arduino.h>` added to `core/` | layering check (observed failing the build) |
| `TurnLeft` drawn as a right turn | `all_maneuver_icons_are_distinct` and `left_and_right_icons_mirror_each_other` (observed failing) |

Only the first few failure lines of each run were inspected, so other tests may also
have failed; those four are the ones confirmed.

### Visual review (Host-tested; judgement of the AI assistant)

The rendered contact sheets (`make render`) were inspected. Findings and fixes:

- Sharp-turn arrows were an unrecognisable blob on the first render. Fixed by
  moving the stem away from the turn side.
- Turn arrows sat off-centre in their box. Fixed.
- On 128x32 the distance was small and "ARRIVED" overlapped the pin. Both fixed.
- Remaining: single stray corner pixels on some arrowheads. They come from the
  host's triangle rasteriser, and the real display uses Adafruit GFX's, so they
  are expected to differ.

The host renderer rasterises triangles and circles itself, so edge pixels can
differ from the real display by about a pixel. **How the screens look on the real
panel, including readability at arm's length and in sunlight, is not known yet.**

## 2. Compile check of the device code

**Compile-checked. Not run.** The device-side code was built with PlatformIO for the
board `esp32doit-devkit-v1` (a classic ESP32, 4 MB flash). The hardware itself is
still **provisional** (see [HARDWARE.md](HARDWARE.md)).

| Environment | What it is | Result | Static RAM | Flash (app partition) | Warnings |
|---|---|---|---|---|---|
| `motonav` | the MotoNav firmware | **SUCCESS** | 23,812 B (7.3 %) | 373,388 B of 1,310,720 B (28.5 %) | 0 |
| `hw_check` | diagnostic: chip identity + I2C scan | **SUCCESS** | 23,556 B (7.2 %) | 333,268 B of 1,310,720 B (25.4 %) | 0 |
| `oled_check` | diagnostic: SSD1306 vs SH1106 test patterns | **SUCCESS** | 23,684 B (7.2 %) | 355,924 B of 1,310,720 B (27.2 %) | 0 |

Built with `-Wall -Wextra`. Versions (pinned in `firmware/platformio.ini`): PlatformIO
Core 6.2.0, platform `espressif32@8.0.0`, framework package
`framework-arduinoespressif32@4.30312.261024`, toolchain packages
`toolchain-xtensa-esp-elf` and `toolchain-riscv32-esp` 14.2.0+20260121, Adafruit GFX
Library 1.13.0, Adafruit SSD1306 2.5.17, Adafruit BusIO 1.17.4, esptool 5.4.0.
Most of the flash is the Arduino core itself. RAM is static usage only; heap behaviour at
run time is measured by the soak test (D7).

Build-system checks, each performed:

| Check | Result |
|---|---|
| Each environment compiles only its own sources (`hw_check` and `oled_check` define their own `setup()`; `motonav` excludes `tools/`) | Confirmed from the build output |
| Building the firmware for an ESP32-S3 (scratch copy) | **Refused** by the `#error` in `src/main.cpp`. The classic-ESP32-only rule is enforced. |
| Building with any of the five `MOTONAV_*` display values missing | Refused by `display_config.h` (checked in the first Phase 1 pass) |
| The Arduino variant for this board defines `SDA = 21`, `SCL = 22` | Read from the installed package |
| `esptool chip-id` and `flash-id` exist in the pinned esptool 5.4.0 | `--help` checked; **not run against a board** |

History: in the first Phase 1 pass, before the board was identified, the firmware was
also compile-checked for generic ESP32-S3 and ESP32-C3 targets (it built for both). Those
environments were removed once the board was identified as a classic ESP32.

What this does **not** show: that any of it runs, that the I2C wiring or address is right,
that the display initialises, or how anything looks. Those are the tests below.

## 3. Hardware validation and device tests (to be run by the board owner)

**None of these has been run.** The AI assistant has no access to the hardware, so every
result in this section has to come from you. Do H1 to H5 first (the full procedure with
explanations is in [HARDWARE.md](HARDWARE.md) section 4), then D1 to D7. **Stop at the
first step that fails and report it.**

### What to send back

- The text of the `hw_check` serial output (copy and paste it).
- Photos of the three `oled_check` patterns, taken close to the glass.
- A photo of the ESP32 module marking and one of the finished wiring.
- Any error message, exactly as printed.

### Hardware validation (H1 to H5)

| Step | What to do | Pass when | Result | Evidence / notes |
|---|---|---|---|---|
| **H1** Look and wire | HARDWARE.md V1 and section 3, with USB **unplugged** | Module marking is an ESP32-WROOM family part; USB chip is a CP2102; OLED pin order read from its own labels and wired by label | | |
| **H2** USB and upload | HARDWARE.md V2: a serial port appears; `pio run -e hw_check -t upload` | Port found; upload completes | | |
| **H3** Chip identity | HARDWARE.md V3: read `hw_check` output | Says `RESULT: classic ESP32. Matches the build target.` and lists Wi-Fi and BLE | | |
| **H4** OLED address | HARDWARE.md V4: read the I2C section of `hw_check` | One device at 0x3C or 0x3D, and the line `Firmware is configured for 0x..: FOUND`. If it says NOT FOUND, edit `MOTONAV_OLED_I2C_ADDR` in `platformio.ini`, reflash `hw_check` and confirm FOUND | | |
| **H5** Controller | HARDWARE.md V5: flash `oled_check`, count lines in pattern 3 | **All three** patterns exactly as described in HARDWARE.md V5 (four lines at the left edge in pattern 3, a complete border in pattern 1, solid white to every edge in pattern 2). Anything else: report with photos | | |

Decisions that follow:

- H3 fails: **stop**. The target chip changes.
- H5 shows an SH1106: **stop**. A different display driver is needed.
- Anything else mismatched: see HARDWARE.md section 5.

Record the facts that were confirmed, so the documentation can be updated from provisional
to verified:

| Fact | Value found |
|---|---|
| Chip model string (from `hw_check`) | |
| Chip revision, cores, flash size | |
| OLED I2C address | |
| OLED controller | |
| OLED size actually shown | |

### Phase 1 device tests (D1 to D7), after H1 to H5 pass

Setup:

1. `cd firmware`
2. `pio run -e motonav -t upload`, then `pio device monitor` (115200 baud).
3. Expected at boot: the chip line, the configuration line, an I2C scan listing your
   display's address, `display: initialised`, then the screens cycling.

### D1. Reliable initialisation (acceptance criterion 2)

Pass: on 20 consecutive power-ups and 20 consecutive resets the display shows the
MotoNav screen, never blank and never garbled.

| # | Power-up (unplug and replug USB) | Reset button |
|---|---|---|
| 1-20 | record `ok` or describe the fault | record `ok` or describe the fault |

### D2. Failed initialisation recovers (criterion 2)

Disconnect the display's SDA wire and reset the board. Pass: serial shows
`display: no answer at 0x..`, repeated every 2 seconds, the board does **not**
reboot repeatedly, and after reconnecting the wire the display initialises on its
own and shows a screen.

### D3. Every screen renders (criterion 3)

Use the serial commands: `0` disconnected, `1` ready, `2` navigating, `3` rerouting,
`4` arrived, `n` next maneuver (repeat `n` through all 11), `a` back to automatic.
Pass: each screen shows what [ARCHITECTURE.md](ARCHITECTURE.md) describes; text is not
cut off; the SSD1306 image is not shifted. (A shifted image with noise on one edge
suggests the controller is an SH1106, not an SSD1306.)

### D4. Arrows are recognisable (criterion 4)

Photograph the display for each of the 10 arrow types (straight, left, right,
slight L/R, sharp L/R, U-turn left/right, roundabout, destination) and judge at
arm's length: can you tell them apart at a glance?

| Maneuver | Recognisable? | Notes |
|---|---|---|
| (fill in) | | |

### D5. Distance readability and formatting (criterion 5)

Run automatic mode and watch the distance count down from 1.5 km. Pass: the number
is the largest text on screen and the unit and format read correctly (for example
`1.2 km`, `450 m`, `90 m`).

### D6. No flicker (criterion 6)

Watch automatic mode for 5 minutes, ideally with a short video. Pass: no visible
flashing or tearing; the screen changes only when the shown text or icon changes.

### D7. 12-hour soak (criterion 7)

```bash
pio device monitor | tee soak.log
```

Leave automatic mode running for 12 hours. Every minute the firmware prints a line like

```
[stats] uptime=3600s heap_free=...  heap_min=...  frames=...
```

Pass: `uptime` only ever increases (a drop or any boot banner means a reset), the
display still works at the end, and `heap_free` shows no downward trend after the
first hour.

| Start | End | Resets seen | heap_free first hour | heap_free last hour | Pass? |
|---|---|---|---|---|---|
| | | | | | |

## 4. Not tested at all

- Anything on the physical board or display.
- Readability in sunlight, burn-in, brightness.
- Non-ASCII road names beyond the replacement-with-`?` behaviour.
- A display unplugged while running (not detected by the driver).
- Any display size other than 128x64 and 128x32.
- BLE and the Android app (not started).
