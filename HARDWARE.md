# Hardware

**Status: Blocked on identification.** No hardware was accessible when this was
written (Phase 0 ran in a cloud sandbox with no USB devices and an empty
repository). Everything below marked *Assumption* is unverified.

## Prototype hardware (as stated by the project owner)

| Item | What is known | What is unknown |
|---|---|---|
| ESP32 development board | "An ESP32 board" | Exact chip (ESP32, S2, S3, C3, C6...), exact board, USB-serial chip, flash size |
| SSD1306 OLED | "An SSD1306 OLED" | Resolution (128x64 or 128x32), interface (I2C or SPI), I2C address, whether the controller is really an SSD1306 |
| Phone | "A smartphone" | Android version, device model |
| Power | USB | — |

The SSD1306 is temporary. It will later be replaced by a small circular AMOLED.
No production hardware is being designed yet.

## Why the exact board matters

- **BLE support differs by chip.** Classic ESP32 supports BLE and Bluetooth
  Classic. S3, C3 and C6 are BLE only. BLE only is all this project needs, but
  library and PlatformIO board selection depend on the exact chip.
- **Default I2C pins differ by board.** The wiring below is only valid for a
  classic ESP32 DevKit layout.
- **Do not assume ESP32-S3.**

## How to identify the board (please run on the machine the board is plugged into)

With PlatformIO or esptool installed:

```bash
pio device list
python -m esptool chip_id        # prints the chip family and revision
python -m esptool flash_id       # prints flash size
```

Also useful: a photo of the top and bottom of the board, and of the silver
module can (the text printed on it, for example `ESP32-WROOM-32E` or
`ESP32-S3-WROOM-1`).

## How to identify the display

- Count the pins. **4 pins** (GND, VCC, SCL, SDA) means I2C. **7 pins**
  (GND, VCC, D0, D1, RES, DC, CS) means SPI.
- Note the resolution printed in the listing or on the board (128x64 or 128x32).
- Many 1.3 inch modules sold as "SSD1306" actually use an **SH1106**
  controller. They need a different driver or they show a shifted image with
  noise at the edge. This is a known, common problem and is checked in Phase 1.
- An I2C scan sketch reports the address, normally `0x3C`, sometimes `0x3D`.

## Prototype wiring (Assumption)

**Valid only for a 4-pin I2C display on a classic ESP32 DevKit.** To be
confirmed or corrected once the board and display are identified.

| Display pin | ESP32 pin |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | GPIO 22 |
| SDA | GPIO 21 |

Notes:

- Power the display from 3.3 V. Most modules also accept 5 V, but 3.3 V keeps the
  I2C lines at ESP32 logic level.
- Most modules include I2C pull-up resistors. If the display is not detected,
  check the address and the wiring before adding external pull-ups.
- The pin numbers will be set in one place in the firmware so a different board
  needs a one-line change.

## Later phases (not started)

Circular AMOLED, production microcontroller, custom PCB, motorcycle power
input and protection, weather resistance, vibration resistance, handlebar
mounting and manufacturing are Phase 6 topics. They are deliberately not
specified yet.
