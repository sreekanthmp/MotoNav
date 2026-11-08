// Small helpers shared by the device-side programs of Phase 2 (main_ble.cpp).
//
// main.cpp (the Phase 1 firmware) has its own copies. It is deliberately left untouched
// while the Phase 1 hardware validation is pending, so that what is tested on the board
// is exactly what is committed. After validation the two can be merged onto this header.
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>

inline const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}

// Prints every I2C address that answers. Helps identify an unknown display.
inline void scanI2c() {
  Serial.println("I2C scan:");
  int found = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  device at 0x%02X\n", address);
      ++found;
    }
  }
  if (found == 0) Serial.println("  nothing answered (check wiring, power and pin numbers)");
}

inline bool i2cAnswers(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}
