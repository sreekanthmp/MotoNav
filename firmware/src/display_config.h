// Display and I2C settings.
//
// There are deliberately NO default values here. The board and the display
// have not been identified yet (see ../HARDWARE.md), and a wrong guess at a
// GPIO number or I2C address would fail silently on the bench. Every value
// must be given by the build, in a PlatformIO environment's build_flags:
//
//   -DMOTONAV_OLED_WIDTH=128        panel width in pixels
//   -DMOTONAV_OLED_HEIGHT=64        panel height in pixels
//   -DMOTONAV_OLED_I2C_ADDR=0x3C    I2C address (the scan at boot prints it)
//   -DMOTONAV_I2C_SDA=<gpio>        I2C data pin
//   -DMOTONAV_I2C_SCL=<gpio>        I2C clock pin
//
// Only I2C displays are supported so far. A 7-pin SPI module needs a
// different constructor in main.cpp once it is identified.
#pragma once

#if !defined(MOTONAV_OLED_WIDTH) || !defined(MOTONAV_OLED_HEIGHT) || \
    !defined(MOTONAV_OLED_I2C_ADDR) || !defined(MOTONAV_I2C_SDA) || \
    !defined(MOTONAV_I2C_SCL)
#error "MotoNav display configuration is missing. Define MOTONAV_OLED_WIDTH, MOTONAV_OLED_HEIGHT, MOTONAV_OLED_I2C_ADDR, MOTONAV_I2C_SDA and MOTONAV_I2C_SCL in platformio.ini build_flags. See HARDWARE.md and the header of src/display_config.h."
#endif
