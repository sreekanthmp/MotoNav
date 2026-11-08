# Hardware

**Status: PROVISIONAL.** The facts below come from product listings supplied by
the project owner. **None of them has been verified on the physical parts yet.**
Section 4 is the procedure for verifying them, and results go in
[TESTING.md](TESTING.md). Until then, treat every row marked *provisional* as an
assumption that could be wrong.

## 1. Prototype hardware

| Item | Provisional facts (from listings) | Verified how | Status |
|---|---|---|---|
| Board | ESP32 "NodeMCU"-style development board, 30-pin, CP2102 USB-to-serial chip | look (section 4), T1 (`hw_check`) | **Provisional** |
| Chip / module | Classic ESP32, expected ESP32-WROOM family module, Wi-Fi + BLE | T1 (`hw_check` prints the silicon model) | **Provisional** |
| Display | 0.96 inch OLED, 128x64 pixels, four-pin I2C | look (section 4), T2, T3 | **Provisional** |
| Display controller | Expected SSD1306 | T3 (`oled_check`) | **Provisional** (SH1106 is a known lookalike) |
| Display address | Not stated. 0x3C is typical, 0x3D also occurs. | T2 (`hw_check`) | **Unknown.** `0x3C` is a guess in `platformio.ini`. |
| Power | USB | USB and upload (section 4) | Provisional |

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
   (T2, T3).
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

The step-by-step procedures, with expected output, pass/fail criteria, failure
interpretation and result tables, are in [TESTING.md](TESTING.md) section 3 (tests **T1 to
T6**). This section keeps the hardware background they rely on. **Nothing here has been
verified on the real parts yet.**

| Test | Verifies | Tool |
|---|---|---|
| T1 ESP32 identification | chip model, BLE, cores, flash size, board compatibility, I2C pin validity, SDA/SCL electrical state | `pio run -e hw_check` |
| T2 I2C detection | the OLED answers, at which address, wiring direction, bus reliability at 100 kHz and 400 kHz | `hw_check` |
| T3 OLED controller | initialisation, full 128x64 rendering, no missing edges, no scrambling, SSD1306 versus SH1106 | `pio run -e oled_check` |
| T4 Main firmware | boot reliability, 5 screens, 11 icons, distance and road-name formatting, transitions, flicker | `pio run -e motonav` |
| T5 30-minute run | free heap, resets, display freezing, memory stability, rendering consistency | `motonav` + `firmware/host/analyze_log.py` |
| T6 12-hour soak | the same, over 12 hours | `motonav` + `analyze_log.py` |

### Look at the parts (power off, USB unplugged)

- **ESP32 module:** the metal can on the board carries a marking such as
  `ESP-WROOM-32` or `ESP32-WROOM-32E`. Photograph it. (Other families, for example
  `ESP32-S3-WROOM-1`, look similar and are **not** supported.)
- **USB chip:** a small square chip near the USB connector marked `CP2102` (or similar).
- **OLED:** four pins, labelled. Note their order and compare with section 3 before wiring.
- Count the board's pins: 30 in total, 15 per side.

### USB and upload

1. Plug in the board. A serial port should appear: `COMx` on Windows,
   `/dev/ttyUSB0` on Linux, `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART` on macOS.
   `pio device list` shows it.
2. If no port appears: try another cable (data, not charge-only). On Windows and macOS
   the Silicon Labs **CP210x driver** is often needed; Linux usually has it built in.
3. If upload stalls at `Connecting......`, hold the board's **BOOT** button until the upload
   starts, then release it. Many 30-pin boards need this.

### What `hw_check` measures, and why

- **Flash size from the chip itself.** The SDK-reported flash size can come from the firmware
  image header rather than the chip, so a 2 MB chip flashed with a 4 MB image would look fine.
  `hw_check` also reads the flash chip's own JEDEC identification, whose last byte encodes the
  capacity. (Reading the id compiles; it has not run on a real chip.)
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
- **Electrical state of SDA and SCL.** Each line is read with the ESP32's internal pull-up
  and then its internal pull-down enabled. Low even with the pull-up means a short to
  ground. High even against the pull-down means a strong external pull-up resistor is
  present (the OLED module's own). Low against the pull-down means none was detected. This is
  a hint with a reasoned basis, not a measurement.
- **I2C at two speeds.** The firmware's display driver runs the bus at 400 kHz, so the OLED
  is probed 200 times at 100 kHz and 200 times at 400 kHz. One missed probe is a fail:
  that is what an intermittent connection looks like.
- **Swapped wires.** If nothing answers, the scan is repeated with SDA and SCL exchanged.

### SSD1306 versus SH1106: background for T3

Most 0.96 inch modules sold as "SSD1306" are SSD1306 or a compatible controller, but 1.3
inch modules are often SH1106, and product listings are sometimes wrong about which one is
fitted. The MotoNav firmware only drives an SSD1306.

An SSD1306 has 128 columns of display memory. An SH1106 has 132 and shows columns 2 to 129.
Driven as an SSD1306 an SH1106 is therefore *expected* to lose the first two columns on the
left and show two columns of leftover memory on the right. The SSD1306 driver also uses a
memory-addressing mode that the SH1106 does not support, so on an SH1106 the picture may
additionally look scrambled, torn or repeated. Both effects are predictions from the
controllers' designs. **Neither has been observed on your panel**, so judge by the rule in
T3, not by one exact symptom: **a panel is accepted as an SSD1306 only if all five test
patterns look exactly as expected.** The five patterns are a full-edge border, all pixels
on, four thin lines at each edge, an 8-by-8 grid aligned to the controller's 8-row pages,
and all pixels off.

**Decision after T3:**

- **All five patterns as expected:** the controller is compatible with the SSD1306 driver.
  Continue with T4.
- **Anything else: stop and report it, with photos.** If it turns out to be an SH1106, the
  firmware needs a different display driver. Because the screens draw through the `Canvas`
  interface, that is a change to one adapter file (`src/ssd1306_canvas.h`) and the library
  choice, not to the navigation or screen code. It is deliberately **not** done yet. If it is
  a wiring or power fault, the fix is on the bench, not in the code.

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
