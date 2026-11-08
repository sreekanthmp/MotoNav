// Verdict logic for the hardware diagnostics (hw_check).
//
// The diagnostics read the hardware; THIS module decides what the readings
// mean. It is pure C++ with no Arduino includes so every rule is unit-tested on
// a normal computer. A diagnostic that wrongly says PASS would be worse than no
// diagnostic, which is why the decisions live here and not in the device code.
#pragma once

#include <stdint.h>

namespace motonav {

// Ordered so that a larger value is worse.
enum class Verdict : uint8_t { Pass, Warn, Fail };

struct Finding {
  Verdict verdict;
  const char* text;  // static string, written for the person at the bench
};

const char* verdictLabel(Verdict v);  // "PASS", "WARN", "FAIL"
Verdict worseOf(Verdict a, Verdict b);

constexpr uint32_t kExpectedFlashBytes = 4u * 1024u * 1024u;
constexpr uint8_t kTypicalOledAddressA = 0x3C;
constexpr uint8_t kTypicalOledAddressB = 0x3D;

// --- chip and board ---------------------------------------------------------
Finding judgeChip(bool isClassicEsp32);
Finding judgeRadios(bool hasWifi, bool hasBle);  // BLE is required by the project
Finding judgeCores(int cores, uint32_t cpuMhz);

// Flash size in bytes. `physicalBytes` comes from the flash chip's own JEDEC id
// (0 if it could not be read); `reportedBytes` is what the SDK says, which can
// come from the image header instead of the chip.
Finding judgeFlashSize(uint32_t physicalBytes, uint32_t reportedBytes);

// Converts the last byte of a JEDEC flash id (the capacity code, 2^n bytes)
// into a size. Returns 0 for codes that cannot be a plausible flash size.
uint32_t flashBytesFromJedecCapacity(uint8_t capacityCode);

// --- I2C pins and lines -----------------------------------------------------
Finding judgeI2cPin(int gpio);
Finding judgePinPair(int sdaGpio, int sclGpio);  // not the same pin

// `highWithPullUp`:   level read with the internal pull-up enabled.
// `highWithPullDown`: level read with the internal pull-down enabled.
// A strong external pull-up (the OLED module's resistor) wins against the
// internal pull-down and reads high; with none, it reads low.
Finding judgeLine(bool highWithPullUp, bool highWithPullDown);

// --- I2C bus ----------------------------------------------------------------
// How reliably the detected OLED address acknowledged repeated probes.
Finding judgeProbes(int acknowledged, int total);

struct AddressFinding {
  Finding finding;
  int address;  // the OLED address to use, or -1 if none can be recommended
};
// `found`: every address that answered. `swappedCount`: devices found when the
// scan was repeated with SDA and SCL swapped (only meaningful if count == 0).
AddressFinding judgeOledAddress(const uint8_t* found, int count, uint8_t configured,
                                int swappedCount);

}  // namespace motonav
