// Runs the real firmware/src/tools/hw_check.cpp against a SIMULATED board.
//   ./build/hw_check_sim <scenario> [loops]
// Prints exactly what the firmware would print on the serial monitor. HOST ONLY:
// the board is invented by the scenario, so values such as the MAC are fake.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Arduino.h"

using shim::world;

static bool configure(const char* name) {
  if (!strcmp(name, "healthy") || !strcmp(name, "unplug_midrun")) return true;
  if (!strcmp(name, "oled_at_3d")) { world.devices = {0x3D}; return true; }
  if (!strcmp(name, "both_addresses")) { world.devices = {0x3C, 0x3D}; return true; }
  if (!strcmp(name, "other_device")) { world.devices = {0x50}; return true; }
  if (!strcmp(name, "no_oled")) { world.devices.clear(); return true; }
  if (!strcmp(name, "swapped_wires")) { world.wireSdaGpio = 22; world.wireSclGpio = 21; return true; }
  if (!strcmp(name, "sda_short")) { world.sdaShortedToGnd = true; world.devices.clear(); return true; }
  if (!strcmp(name, "no_pullups")) { world.sdaHasPullup = false; world.sclHasPullup = false; return true; }
  if (!strcmp(name, "intermittent")) { world.flakyEvery = 7; return true; }
  if (!strcmp(name, "slow_only")) { world.deviceMaxHz = 100000; return true; }
  if (!strcmp(name, "wrong_chip_s3")) { world.model = 9; world.modelName = "ESP32-S3"; return true; }
  if (!strcmp(name, "no_ble")) { world.features = 0x01; return true; }
  if (!strcmp(name, "flash_2mb")) { world.jedecId = 0xEF4015; return true; }  // chip is 2 MB, header says 4
  if (!strcmp(name, "flash_8mb")) { world.jedecId = 0xEF4017; world.sdkFlashBytes = 8u * 1024u * 1024u; return true; }
  if (!strcmp(name, "flash_unreadable")) { world.jedecReadable = false; return true; }
  if (!strcmp(name, "single_core")) { world.cores = 1; return true; }
  return false;
}

int main(int argc, char** argv) {
  const char* scenario = argc > 1 ? argv[1] : "healthy";
  const int loops = argc > 2 ? atoi(argv[2]) : 3;
  if (!configure(scenario)) {
    fprintf(stderr, "unknown scenario '%s'\n", scenario);
    return 2;
  }

  setup();
  for (int i = 0; i < loops; ++i) {
    world.nowMs += 5000;  // the firmware repeats every 5 s
    if (!strcmp(scenario, "unplug_midrun") && i == 0) world.devices.clear();
    loop();
  }
  fputs(Serial.out.c_str(), stdout);
  return 0;
}
