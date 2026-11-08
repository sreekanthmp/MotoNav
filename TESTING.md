# Testing

Every result below is labelled with where it was obtained:

- **Host-tested**: automated tests or rendered images on a normal computer
  (here: Linux, gcc 13.3 and clang 18.1). Says nothing about the physical display.
- **Compile-checked**: the device-side code was built by the ESP32 toolchain. Says
  nothing about whether it runs.
- **Device-tested**: run on the real board and display. **Nothing is
  device-tested yet.** The board and display have not been identified (see
  [HARDWARE.md](HARDWARE.md)), and the AI assistant has no access to hardware.

Last updated at the end of Phase 1 (2026-11-08).

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

**Compile-checked. Not run.** The device-side code (`src/main.cpp`,
`src/ssd1306_canvas.h`, `src/display_config.h`) plus the shared library was built
with PlatformIO for three generic ESP32 targets. The boards and pin numbers in
these environments are **placeholders** so the code compiles; they say nothing about
the real hardware.

| Environment | Board (generic) | Result | Static RAM | Flash (app partition) | Warnings |
|---|---|---|---|---|---|
| `compile_check_esp32dev` | classic ESP32 | **SUCCESS** | 23,812 B (7.3 %) | 372,520 B of 1,310,720 B (28.4 %) | 0 |
| `compile_check_esp32s3` | ESP32-S3 | **SUCCESS** | 23,800 B (7.3 %) | 403,842 B of 3,342,336 B (12.1 %) | 0 |
| `compile_check_esp32c3` | ESP32-C3 | **SUCCESS** | 15,244 B (4.7 %) | 385,754 B of 1,310,720 B (29.4 %) | 0 |

Built with `-Wall -Wextra`. Versions (pinned in `firmware/platformio.ini`, 2026-11-08):
PlatformIO Core 6.2.0, platform `espressif32@8.0.0`, framework package
`framework-arduinoespressif32@4.30312.261024`, toolchain packages `toolchain-xtensa-esp-elf` and `toolchain-riscv32-esp` 14.2.0+20260121, Adafruit GFX
Library 1.13.0, Adafruit SSD1306 2.5.17, Adafruit BusIO 1.17.4.

Most of the flash is the Arduino core itself; the MotoNav code is a small part of it.
RAM is static usage only; heap behaviour at run time is measured by the soak test (D7).

The "no default pins" guard was also checked: a build with any of the five
`MOTONAV_*` display values missing stops with the explanatory `#error`, and with all
five present it passes.

What this does **not** show: that the firmware runs, that the I2C wiring or address is
right, that the display initialises, or how anything looks. Those are device tests.

## 3. Device tests (to be run by the board owner)

**None of these has been run.** Do them in this order; record results in the tables.

### Setup

1. Identify the board and display ([HARDWARE.md](HARDWARE.md)).
2. Create a PlatformIO environment for your board from the template in
   `firmware/platformio.ini` with your five display values.
3. `pio run -e myboard -t upload`, then `pio device monitor` (115200 baud).
4. Expected at boot: the configuration line, an I2C scan listing your display's
   address, `display: initialised`, then the screens cycling.

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
