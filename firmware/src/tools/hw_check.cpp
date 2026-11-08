// Hardware check: identifies the chip and scans the I2C bus.
//
// This is a standalone diagnostic, not the MotoNav firmware. It needs no
// display library, so a wiring problem cannot hide behind a driver problem.
// Build and flash it with:   pio run -e hw_check -t upload
// then:                      pio device monitor
//
// It prints, and repeats every 5 seconds so wires can be moved while it runs:
//   1. which chip this really is (the project expects a classic ESP32)
//   2. whether each I2C line idles high (a hint about pull-ups and shorts)
//   3. every I2C address that answers, with a note on likely OLED addresses
//   4. if nothing answers, a second scan with SDA and SCL swapped, because
//      swapped wires are the most common mistake
//
// See HARDWARE.md ("Verifying the hardware") for how to read the output.
#include <Arduino.h>
#include <Wire.h>
#include <esp_chip_info.h>

#include "display_config.h"  // MOTONAV_I2C_SDA, MOTONAV_I2C_SCL, MOTONAV_OLED_I2C_ADDR

namespace {

constexpr uint32_t kRepeatMs = 5000;
uint32_t lastRunMs = 0;
uint32_t runCount = 0;

const char* modelName(esp_chip_model_t model) {
  switch (model) {
    case CHIP_ESP32:   return "ESP32 (classic)";
    case CHIP_ESP32S2: return "ESP32-S2";
    case CHIP_ESP32S3: return "ESP32-S3";
    case CHIP_ESP32C3: return "ESP32-C3";
    default:           return "other / unknown";
  }
}

void printChipInfo() {
  esp_chip_info_t info;
  esp_chip_info(&info);

  Serial.println("--- Chip ---");
  Serial.printf("Model (silicon)  : %s\n", modelName(info.model));
  Serial.printf("Model (SDK name) : %s\n", ESP.getChipModel());
  Serial.printf("Revision         : %u\n", static_cast<unsigned>(ESP.getChipRevision()));
  Serial.printf("CPU cores        : %u at %lu MHz\n", static_cast<unsigned>(info.cores),
                static_cast<unsigned long>(ESP.getCpuFreqMHz()));
  Serial.printf("Radios           : %s%s%s\n", (info.features & CHIP_FEATURE_WIFI_BGN) ? "Wi-Fi " : "",
                (info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
                (info.features & CHIP_FEATURE_BT) ? "Bluetooth-Classic" : "");
  Serial.printf("Flash            : %lu bytes\n", static_cast<unsigned long>(ESP.getFlashChipSize()));
  Serial.printf("MAC (eFuse)      : %012llX\n", static_cast<unsigned long long>(ESP.getEfuseMac()));

  if (info.model == CHIP_ESP32) {
    Serial.println("RESULT: classic ESP32. Matches the build target.");
  } else {
    Serial.println("RESULT: NOT a classic ESP32. The MotoNav firmware is built for the classic ESP32;");
    Serial.println("        tell the project owner before going any further.");
  }
}

// Reads a pin with no driver and no internal pull-up. An I2C line with working
// pull-up resistors idles high. Low means a short or a wiring fault. A line
// with no pull-up at all floats and can read either way, so this is a hint,
// not proof.
const char* idleLevel(int pin) {
  pinMode(pin, INPUT);
  delay(5);
  return digitalRead(pin) ? "high (normal)" : "LOW (short to GND or wiring fault?)";
}

const char* describe(uint8_t address) {
  if (address == 0x3C || address == 0x3D) return "  <- typical OLED address (SSD1306 and SH1106 both use these)";
  return "";
}

// Returns the number of devices found.
int scan(int sda, int scl, const char* label) {
  Wire.begin(sda, scl);
  Wire.setTimeOut(50);
  Serial.printf("--- I2C scan (%s: SDA=GPIO%d SCL=GPIO%d) ---\n", label, sda, scl);

  int found = 0;
  bool configuredFound = false;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("  device at 0x%02X%s\n", address, describe(address));
      ++found;
      if (address == MOTONAV_OLED_I2C_ADDR) configuredFound = true;
    } else if (error == 4) {
      Serial.printf("  unknown I2C error at 0x%02X\n", address);
    }
  }
  Wire.end();

  if (found == 0) {
    Serial.println("  no devices answered");
  } else {
    Serial.printf("  %d device(s). Firmware is configured for 0x%02X: %s\n", found,
                  static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR),
                  configuredFound ? "FOUND, nothing to change" : "NOT FOUND, change MOTONAV_OLED_I2C_ADDR in platformio.ini");
  }
  return found;
}

void runOnce() {
  ++runCount;
  Serial.printf("\n========== hw_check run %lu ==========\n", static_cast<unsigned long>(runCount));
  printChipInfo();

  Serial.println("--- I2C line levels with nothing driving them ---");
  Serial.printf("SDA (GPIO%d) idle: %s\n", MOTONAV_I2C_SDA, idleLevel(MOTONAV_I2C_SDA));
  Serial.printf("SCL (GPIO%d) idle: %s\n", MOTONAV_I2C_SCL, idleLevel(MOTONAV_I2C_SCL));

  const int found = scan(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL, "as configured");
  if (found == 0) {
    const int swapped = scan(MOTONAV_I2C_SCL, MOTONAV_I2C_SDA, "SDA and SCL SWAPPED");
    if (swapped > 0) {
      Serial.println("RESULT: devices answer only with the two wires swapped. Swap SDA and SCL.");
    } else {
      Serial.println("RESULT: nothing answers. Check, with the board unplugged: VCC to 3V3, GND to GND,");
      Serial.println("        SDA to the pin above, SCL to the pin above, and that the OLED is the 4-pin I2C kind.");
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("MotoNav hw_check (diagnostic, not the MotoNav firmware)");
  runOnce();
  lastRunMs = millis();
}

void loop() {
  if (millis() - lastRunMs >= kRepeatMs) {
    lastRunMs = millis();
    runOnce();
  }
  delay(20);
}
