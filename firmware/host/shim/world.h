// A simulated board for running the diagnostics on a normal computer.
// HOST ONLY. Every value here is invented by a test scenario; none is a
// measurement of real hardware.
#pragma once

#include <stdint.h>

#include <vector>

namespace shim {

struct World {
  // Chip.
  int model = 1;  // CHIP_ESP32
  uint32_t features = 0x31;  // bit0 Wi-Fi, bit4 BLE, bit5 Bluetooth-Classic
  int cores = 2;
  uint32_t cpuMhz = 240;
  unsigned revision = 301;
  const char* modelName = "ESP32-D0WD-V3";
  uint64_t mac = 0x240AC4123456ULL;
  uint32_t jedecId = 0xEF4016;  // Winbond, 4 MB
  bool jedecReadable = true;
  uint32_t sdkFlashBytes = 4u * 1024u * 1024u;

  // Wiring and I2C bus.
  int wireSdaGpio = 21;  // which GPIO the SDA wire really goes to
  int wireSclGpio = 22;
  bool sdaShortedToGnd = false;
  bool sclShortedToGnd = false;
  bool sdaHasPullup = true;
  bool sclHasPullup = true;
  std::vector<int> devices = {0x3C};
  uint32_t deviceMaxHz = 400000;  // the device stops answering above this
  int flakyEvery = 0;             // if N > 0, every Nth transmission to a device times out
  int absentCode = 2;             // endTransmission() result for an empty address
  bool wireBeginFails = false;

  // Time.
  uint32_t nowMs = 0;
};

extern World world;

}  // namespace shim
