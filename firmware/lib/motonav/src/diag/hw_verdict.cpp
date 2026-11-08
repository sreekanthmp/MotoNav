#include "diag/hw_verdict.h"

namespace motonav {

const char* verdictLabel(Verdict v) {
  switch (v) {
    case Verdict::Pass: return "PASS";
    case Verdict::Warn: return "WARN";
    case Verdict::Fail: return "FAIL";
  }
  return "????";
}

Verdict worseOf(Verdict a, Verdict b) {
  return static_cast<uint8_t>(a) >= static_cast<uint8_t>(b) ? a : b;
}

Finding judgeChip(bool isClassicEsp32) {
  if (isClassicEsp32) return Finding{Verdict::Pass, "classic ESP32, as expected"};
  return Finding{Verdict::Fail,
                 "NOT a classic ESP32. The firmware is built for the classic ESP32 only. "
                 "Stop and report this"};
}

Finding judgeRadios(bool hasWifi, bool hasBle) {
  if (!hasBle) {
    return Finding{Verdict::Fail, "no BLE. The project needs BLE. Stop and report this"};
  }
  if (!hasWifi) {
    return Finding{Verdict::Pass, "BLE present (Wi-Fi absent, which the project does not need)"};
  }
  return Finding{Verdict::Pass, "Wi-Fi and BLE present"};
}

Finding judgeCores(int cores, uint32_t cpuMhz) {
  if (cores == 2 && cpuMhz == 240u) {
    return Finding{Verdict::Pass, "dual core at 240 MHz, as expected"};
  }
  return Finding{Verdict::Warn,
                 "not dual core at 240 MHz. Single-core or slower variants exist. The Phase 1 "
                 "firmware does not need the second core, but report it"};
}

uint32_t flashBytesFromJedecCapacity(uint8_t capacityCode) {
  // Capacity code n means 2^n bytes. 0x10 (64 KB) to 0x19 (32 MB) is the
  // plausible range for SPI NOR flash.
  if (capacityCode < 0x10 || capacityCode > 0x19) return 0;
  return 1u << capacityCode;
}

Finding judgeFlashSize(uint32_t physicalBytes, uint32_t reportedBytes) {
  const uint32_t size = physicalBytes != 0 ? physicalBytes : reportedBytes;
  if (size == 0) return Finding{Verdict::Fail, "flash size could not be determined"};

  if (size < kExpectedFlashBytes) {
    return Finding{Verdict::Fail,
                   "flash is smaller than the 4 MB the firmware's partition table assumes. "
                   "It will not boot. Stop and report this"};
  }
  if (physicalBytes != 0 && reportedBytes != 0 && physicalBytes != reportedBytes) {
    return Finding{Verdict::Warn,
                   "the SDK-reported flash size differs from the chip's own size. "
                   "Confirm with: esptool flash-id"};
  }
  if (size > kExpectedFlashBytes) {
    return Finding{Verdict::Warn,
                   "flash is larger than the expected 4 MB. Harmless: the firmware uses a "
                   "4 MB layout. Note the size"};
  }
  if (physicalBytes == 0) {
    return Finding{Verdict::Warn,
                   "4 MB according to the SDK, but the chip's own size could not be read. "
                   "Confirm with: esptool flash-id"};
  }
  return Finding{Verdict::Pass, "4 MB, as expected (read from the flash chip itself)"};
}

Finding judgeI2cPin(int gpio) {
  const bool exists = gpio >= 0 && gpio <= 39 && gpio != 20 && gpio != 24 &&
                      !(gpio >= 28 && gpio <= 31);
  if (!exists) return Finding{Verdict::Fail, "not a valid ESP32 GPIO number"};
  if (gpio >= 6 && gpio <= 11) {
    return Finding{Verdict::Fail, "GPIO 6 to 11 are wired to the flash chip. Never use them"};
  }
  if (gpio >= 34) {
    return Finding{Verdict::Fail, "input-only GPIO: it cannot drive an I2C line"};
  }
  if (gpio == 0 || gpio == 2 || gpio == 5 || gpio == 12 || gpio == 15) {
    return Finding{Verdict::Warn,
                   "a boot-strapping pin: it can upset booting if the OLED holds it at the "
                   "wrong level at reset"};
  }
  return Finding{Verdict::Pass, "usable for I2C"};
}

Finding judgePinPair(int sdaGpio, int sclGpio) {
  if (sdaGpio == sclGpio) {
    return Finding{Verdict::Fail, "SDA and SCL are configured as the same GPIO"};
  }
  return Finding{Verdict::Pass, "SDA and SCL are different pins"};
}

Finding judgeLine(bool highWithPullUp, bool highWithPullDown) {
  if (!highWithPullUp && !highWithPullDown) {
    return Finding{Verdict::Fail,
                   "reads LOW even with the internal pull-up: shorted to GND, or held low by "
                   "a device. Unplug USB and check the wiring"};
  }
  if (!highWithPullUp && highWithPullDown) {
    return Finding{Verdict::Fail,
                   "inconsistent readings (low with the pull-up, high with the pull-down): a "
                   "device may be driving this line, or the wiring is wrong"};
  }
  if (highWithPullDown) {
    return Finding{Verdict::Pass, "a strong external pull-up resistor is present (good)"};
  }
  return Finding{Verdict::Warn,
                 "no strong external pull-up detected. The OLED module may have none. I2C may "
                 "still work on the ESP32's weaker internal pull-ups but is less reliable"};
}

Finding judgeProbes(int acknowledged, int total) {
  if (total <= 0) return Finding{Verdict::Warn, "no probes were run"};
  if (acknowledged == total) {
    return Finding{Verdict::Pass, "every probe was acknowledged"};
  }
  if (acknowledged <= 0) {
    return Finding{Verdict::Fail, "no probe was acknowledged"};
  }
  return Finding{Verdict::Fail,
                 "intermittent: some probes were not acknowledged. Check wire contact and length"};
}

AddressFinding judgeOledAddress(const uint8_t* found, int count, uint8_t configured,
                                int swappedCount) {
  bool hasA = false;
  bool hasB = false;
  bool hasConfigured = false;
  for (int i = 0; i < count; ++i) {
    if (found[i] == kTypicalOledAddressA) hasA = true;
    if (found[i] == kTypicalOledAddressB) hasB = true;
    if (found[i] == configured) hasConfigured = true;
  }

  if (count <= 0) {
    if (swappedCount > 0) {
      return AddressFinding{Finding{Verdict::Fail,
                                    "a device answers only with SDA and SCL swapped. "
                                    "Swap the two wires"},
                            -1};
    }
    return AddressFinding{Finding{Verdict::Fail,
                                  "no I2C device answered. Check VCC, GND, SDA and SCL, and "
                                  "that the OLED is the 4-pin I2C kind"},
                          -1};
  }

  if (hasA && hasB) {
    const bool configuredIsOne = configured == kTypicalOledAddressA || configured == kTypicalOledAddressB;
    return AddressFinding{Finding{Verdict::Warn,
                                  "devices at BOTH 0x3C and 0x3D. Another I2C device may be on "
                                  "the bus. Report this"},
                          (configuredIsOne && hasConfigured) ? static_cast<int>(configured) : -1};
  }

  if (hasA || hasB) {
    const int address = hasA ? kTypicalOledAddressA : kTypicalOledAddressB;
    if (address == configured) {
      return AddressFinding{Finding{Verdict::Pass, "OLED found at the configured address"}, address};
    }
    return AddressFinding{Finding{Verdict::Warn,
                                  "OLED found, but NOT at the address in platformio.ini. Change "
                                  "MOTONAV_OLED_I2C_ADDR to the address shown"},
                          address};
  }

  return AddressFinding{Finding{Verdict::Warn,
                                "I2C device(s) answered, but none at a typical OLED address "
                                "(0x3C or 0x3D). Report the addresses shown"},
                        -1};
}

}  // namespace motonav
