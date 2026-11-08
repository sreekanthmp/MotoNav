# Hardware

**Status: PROVISIONAL.** The facts below come from product listings supplied by
the project owner. **None of them has been verified on the physical parts yet.**
Section 4 is the procedure for verifying them, and results go in
[TESTING.md](TESTING.md). Until then, treat every row marked *provisional* as an
assumption that could be wrong.

## 1. Prototype hardware

| Item | Provisional facts (from listings) | Verified how | Status |
|---|---|---|---|
| Board | ESP32 "NodeMCU"-style development board, 30-pin, CP2102 USB-to-serial chip | V1 (look), V3 (`hw_check`) | **Provisional** |
| Chip / module | Classic ESP32, expected ESP32-WROOM family module, Wi-Fi + BLE | V3 (`hw_check` prints the silicon model) | **Provisional** |
| Display | 0.96 inch OLED, 128x64 pixels, four-pin I2C | V1, V4, V5 | **Provisional** |
| Display controller | Expected SSD1306 | V5 (`oled_check`) | **Provisional** (SH1106 is a known lookalike) |
| Display address | Not stated. 0x3C is typical, 0x3D also occurs. | V4 (`hw_check`) | **Unknown.** `0x3C` is a guess in `platformio.ini`. |
| Power | USB | V2 | Provisional |

The SSD1306 display is temporary. It will later be replaced by a small circular
AMOLED. No production hardware is being designed yet (Phase 6).

## 2. What the firmware is configured for

| Setting | Value | Where | Basis |
|---|---|---|---|
| Target chip | classic ESP32 | `platformio.ini` board `esp32doit-devkit-v1`; compile-time guard in `src/main.cpp` | Listing, **provisional** |
| Display | 128x64, I2C | `-DMOTONAV_OLED_WIDTH=128 -DMOTONAV_OLED_HEIGHT=64` | Listing, **provisional** |
| I2C address | `0x3C` | `-DMOTONAV_OLED_I2C_ADDR=0x3C` | **Guess.** Verify with `hw_check`. |
| SDA | GPIO 21 | `-DMOTONAV_I2C_SDA=21` | Proposed wiring, **provisional** |
| SCL | GPIO 22 | `-DMOTONAV_I2C_SCL=22` | Proposed wiring, **provisional** |

**Confirmations (what is established and what is not):**

1. **Firmware target is the classic ESP32.**
   *Established:* PlatformIO board `esp32doit-devkit-v1` is chip `esp32` (classic),
   4 MB flash, and the firmware refuses to build for any other chip (checked by
   deliberately building it for an ESP32-S3: rejected with an `#error`). It builds for
   this target with no warnings.
   *Not yet established:* that the physical module really is a classic ESP32.
   `hw_check` and the boot banner print the real chip, and `esptool` will also refuse
   to flash a binary built for the wrong chip.
   *Why this board id:* PlatformIO has no board called "NodeMCU 30-pin". Boards with
   that layout follow the DOIT "ESP32 DevKit V1" design. PlatformIO's `nodemcu-32s` is a
   different board.
2. **Display configuration is 128x64, I2C.**
   *Established:* the firmware and both diagnostics are configured that way and build.
   *Not yet established:* the real panel size, interface, address and controller
   (V4, V5).
3. **The proposed pins match the board's defaults.** For the DOIT DevKit V1 variant
   the installed Arduino core defines `SDA = 21` and `SCL = 22`, so GPIO 21 and 22 are
   the standard I2C pins for this board. (Read from the package, 2026-11-08.)

## 3. Wiring (provisional)

| OLED pin | ESP32 pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL | GPIO 22 (labelled `D22` on most boards) |
| SDA | GPIO 21 (labelled `D21` on most boards) |

**Before applying power:**

- **OLED pin order differs between modules.** Some are `GND VCC SCL SDA`, others
  `VCC GND SCL SDA`. Read the labels printed on *your* module and wire by label,
  never by position. Reversed power can destroy the display.
- Use the board's **3V3** pin, not `VIN`. (`VIN` is the 5 V USB rail.)
- Pin labels and positions vary between 30-pin boards. Find `D21`, `D22`, `3V3` and `GND`
  on your board's silkscreen rather than counting pins.
- Use a USB cable that carries data. Charge-only cables give no serial port.

Most modules include I2C pull-up resistors. Do not add external pull-ups unless
`hw_check` reports a line stuck low or the bus is unreliable.

## 4. Verifying the hardware

Do these in order and record the results in [TESTING.md](TESTING.md) section 3. They
take about 20 minutes. **Stop at the first step that does not match, and report it.**

You need PlatformIO installed on the computer the board is plugged into (VS Code
extension or `pip install platformio`), and this repository.

### V1. Look at the parts (power off, USB unplugged)

- **ESP32 module:** the metal can on the board carries a marking such as
  `ESP-WROOM-32` or `ESP32-WROOM-32E`. Photograph it. (Other families, for example
  `ESP32-S3-WROOM-1`, look similar and are **not** supported.)
- **USB chip:** a small square chip near the USB connector marked `CP2102` (or similar).
- **OLED:** four pins, labelled. Note their order and compare with section 3 before wiring.
- Count the board's pins: 30 in total, 15 per side.

### V2. USB and upload

1. Plug in the board. A serial port should appear: `COMx` on Windows,
   `/dev/ttyUSB0` on Linux, `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART` on macOS.
   `pio device list` shows it.
2. If no port appears: try another cable (data, not charge-only). On Windows and macOS
   the Silicon Labs **CP210x driver** is often needed; Linux usually has it built in.
3. If upload stalls at `Connecting......`, hold the board's **BOOT** button until the upload
   starts, then release it. Many 30-pin boards need this.

### V3. Identify the chip: `hw_check`

```bash
cd firmware
pio run -e hw_check -t upload
pio device monitor            # 115200 baud; press the board's EN button to restart output
```

`hw_check` prints the chip identity, the idle level of the two I2C lines, and every I2C
address that answers, repeating every 5 seconds so you can move wires while it runs.

**Pass:** the chip section ends with `RESULT: classic ESP32. Matches the build target.`,
and the radios line lists Wi-Fi and BLE.

**Fail:** `RESULT: NOT a classic ESP32` means the board is a different chip. **Stop and
report it.** The firmware is deliberately not built for other chips.

Illustrative shape of a healthy run (your revision, MAC, flash size and addresses
will differ; this is not captured output):

```
--- Chip ---
Model (silicon)  : ESP32 (classic)
Model (SDK name) : ESP32-D0WD...        <- the exact name varies
CPU cores        : 2 at 240 MHz
Radios           : Wi-Fi BLE Bluetooth-Classic
RESULT: classic ESP32. Matches the build target.
--- I2C line levels with nothing driving them ---
SDA (GPIO21) idle: high (normal)
SCL (GPIO22) idle: high (normal)
--- I2C scan (as configured: SDA=GPIO21 SCL=GPIO22) ---
  device at 0x3C  <- typical OLED address ...
  1 device(s). Firmware is configured for 0x3C: FOUND, nothing to change
```

A second way to identify the chip, without flashing anything, is to ask the ESP32's
boot ROM directly. With the esptool that PlatformIO installs (v5.4.0, after the first
`pio run`):

```bash
pio pkg exec -p tool-esptool -- esptool chip-id
pio pkg exec -p tool-esptool -- esptool flash-id
```

It should report a classic `ESP32` and mention dual core, and the flash size. (The
spelling `chip_id` is deprecated in this esptool version. The commands were checked to
exist; they have not been run against a board.) If the port is not found automatically,
add `--port <your port>` after `esptool`.

### V4. Find the OLED address: the I2C scan

The scan is part of `hw_check` (V3). Read the I2C section of the output:

| Output | Meaning | Action |
|---|---|---|
| `device at 0x3C ... FOUND, nothing to change` | OLED at the address the firmware expects | none |
| `device at 0x3D ... NOT FOUND, change MOTONAV_OLED_I2C_ADDR` | OLED at the other common address | edit `-DMOTONAV_OLED_I2C_ADDR=0x3D` in `firmware/platformio.ini` |
| a different address | unusual module, or another I2C device on the bus | report it |
| `no devices answered`, then a second scan `SDA and SCL SWAPPED` that finds one | SDA and SCL wires are swapped | swap them |
| `no devices answered` in both scans | wiring, power or the wrong kind of OLED | check VCC, GND, SDA, SCL against section 3; check the "idle" lines |
| `SDA ... LOW (short to GND or wiring fault?)` | a line is held low | unplug USB and recheck the wiring for shorts |

The OLED's address cannot reveal the controller: SSD1306 and SH1106 modules both use
0x3C or 0x3D. That is what V5 is for.

### V5. Is the controller an SSD1306 or an SH1106? `oled_check`

Most 0.96 inch modules sold as "SSD1306" are SSD1306 or a compatible controller, but
1.3 inch modules are often SH1106, and product listings are sometimes wrong about
which one is fitted. The MotoNav firmware only drives an SSD1306.

```bash
cd firmware
pio run -e oled_check -t upload
pio device monitor
```

The display shows three test patterns, each for 6 seconds. In the serial monitor, press
`1`, `2` or `3` to hold a pattern and `a` to cycle again.

**Why this discriminates:** an SSD1306 has 128 columns of display memory. An SH1106 has
132 and shows columns 2 to 129. Driven as an SSD1306, an SH1106 therefore loses the
first two columns on the left and shows two columns of leftover memory on the right.
The SSD1306 driver also uses a memory-addressing mode that the SH1106 does not support,
so on an SH1106 the picture may additionally look **scrambled, torn or repeated**, not
just shifted. (Both effects are expected from the controllers' designs. Neither has been
observed on your panel, so judge by the rule below, not by one exact symptom.)

**The rule: a panel is accepted as an SSD1306 only if all three patterns look exactly like
the "healthy" column.** Anything else is either an SH1106 or a wiring or power fault, and
should be reported with photos rather than guessed at.

| Pattern | Healthy SSD1306 | SH1106 driven as an SSD1306 (typical, may also be scrambled) |
|---|---|---|
| 1. Border, two diagonals, text | a complete rectangle on the very edge of the glass: **all four sides**, diagonals reaching all four corners | the **left side of the border is missing**; the right side sits about 2 pixels in from the edge with speckles outside it |
| 2. All pixels on | solid white to **all four edges** | white, but a strip about **2 pixels wide on the right edge** is dark or speckled |
| 3. Edge columns | **four** thin lines at the left edge and **four** at the right edge, evenly spaced 2 pixels apart | **three** lines at the left edge, four at the right edge but pulled 2 pixels in, speckles beyond them |

Quick test: **count the thin lines at the left edge in pattern 3. Four, together with a
clean pattern 1 and 2, means SSD1306. Three means SH1106.** If you cannot tell by eye,
photograph the display with a phone camera held close, and send the photos.

Other symptoms and what they point to:

| Symptom | Likely cause |
|---|---|
| Nothing lights, serial says `NO ANSWER` | wrong address or wiring: go back to V4 |
| Image fills only the top half, or lines repeat | the panel is 128x32, not 128x64: set `-DMOTONAV_OLED_HEIGHT=32` |
| Image is garbled or looks like static | wiring or power fault, or a different controller |
| Image upside down or mirrored | the panel is mounted the other way up: report it, easy to change |

**Decision:**

- **All three patterns exactly as in the healthy column:** SSD1306 confirmed. Go ahead
  with the Phase 1 device tests (D1 to D7 in TESTING.md).
- **Anything else (SH1106 symptoms, scrambling, noise): stop and report it, with photos.**
  If it turns out to be an SH1106, the firmware needs a different display driver. Because
  the screens draw through the `Canvas` interface, that is a change to one adapter file
  (`src/ssd1306_canvas.h`) and the library choice, not to the navigation or screen code.
  It is deliberately **not** done yet. If it is a wiring or power fault, the fix is on the
  bench, not in the code.

### V6. Then run the firmware

```bash
cd firmware
pio run -e motonav -t upload
pio device monitor
```

Expected at boot: the chip line, the configuration line, an I2C scan listing your
display, `display: initialised`, and the screens cycling. Continue with the Phase 1
device tests in TESTING.md.

## 5. If something does not match

| Finding | What changes |
|---|---|
| Chip is not a classic ESP32 | **Stop.** The target, board id and chip guard change. Report it. |
| OLED at 0x3D | One line in `platformio.ini` |
| Panel is 128x32 | One line in `platformio.ini`; compact layout already exists and is host-tested |
| Controller is SH1106 | **Stop.** New display adapter (small, but not yet written) |
| Display is SPI, not I2C | **Stop.** The firmware supports I2C only (D-23) |
| Different SDA/SCL pins | Two lines in `platformio.ini` |

## 6. Later phases (not started)

Circular AMOLED, production microcontroller, custom PCB, motorcycle power input and
protection, weather resistance, vibration resistance, handlebar mounting and
manufacturing are Phase 6 topics. They are deliberately not specified yet.
