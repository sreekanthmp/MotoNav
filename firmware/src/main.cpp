// MotoNav firmware, Phase 1: shows simulated navigation on the display.
// No Bluetooth yet. This file is only wiring: hardware setup, a loop that asks
// the simulator for a state, and the Presenter that decides when to redraw.
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#include "display_config.h"
#include "sim/sim.h"
#include "ssd1306_canvas.h"
#include "ui/presenter.h"

using namespace motonav;

namespace {

constexpr uint32_t kInitRetryMs = 2000;
constexpr uint32_t kStatsEveryMs = 60000;

Adafruit_SSD1306 oled(MOTONAV_OLED_WIDTH, MOTONAV_OLED_HEIGHT, &Wire, -1);
Ssd1306Canvas canvas(oled);
Presenter presenter(canvas);
SimController sim;

bool displayReady = false;
uint32_t lastInitTryMs = 0;
uint32_t lastStatsMs = 0;
uint32_t framesPresented = 0;

// Prints every I2C address that answers. Helps identify an unknown display.
void scanI2c() {
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

// Adafruit's begin() does not notice an absent display, so probe it ourselves.
bool initDisplay() {
  Wire.beginTransmission(MOTONAV_OLED_I2C_ADDR);
  const uint8_t error = Wire.endTransmission();
  if (error != 0) {
    Serial.printf("display: no answer at 0x%02X (I2C error %u)\n",
                  static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR), static_cast<unsigned>(error));
    return false;
  }
  // periphBegin=false: we already started Wire on our own pins.
  if (!oled.begin(SSD1306_SWITCHCAPVCC, MOTONAV_OLED_I2C_ADDR, true, false)) {
    Serial.println("display: SSD1306 begin() failed (frame buffer allocation)");
    return false;
  }
  Serial.println("display: initialised");
  presenter.invalidate();  // the panel content is unknown: force a full draw
  return true;
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const int c = Serial.read();
    if (c == '\r' || c == '\n') continue;
    if (sim.handleCommand(static_cast<char>(c), millis())) {
      Serial.printf("sim: command '%c' (%s)\n", c, sim.isAuto() ? "auto" : "hold");
    } else {
      Serial.printf("sim: unknown command '%c'. a=auto 0..4=hold screen n=next maneuver\n", c);
    }
  }
}

void printStats(uint32_t now) {
  Serial.printf("[stats] uptime=%lus heap_free=%lu heap_min=%lu frames=%lu\n",
                static_cast<unsigned long>(now / 1000UL),
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getMinFreeHeap()),
                static_cast<unsigned long>(framesPresented));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("MotoNav firmware - Phase 1 (simulated navigation, no BLE)");
  Serial.printf("config: %dx%d display at 0x%02X, SDA=%d SCL=%d\n",
                MOTONAV_OLED_WIDTH, MOTONAV_OLED_HEIGHT,
                static_cast<unsigned>(MOTONAV_OLED_I2C_ADDR), MOTONAV_I2C_SDA, MOTONAV_I2C_SCL);

  Wire.begin(MOTONAV_I2C_SDA, MOTONAV_I2C_SCL);
  scanI2c();
  displayReady = initDisplay();
  lastInitTryMs = millis();
}

void loop() {
  const uint32_t now = millis();

  // A failed init is reported and retried; it never ends in a reboot loop.
  if (!displayReady) {
    if (now - lastInitTryMs >= kInitRetryMs) {
      lastInitTryMs = now;
      displayReady = initDisplay();
    }
    delay(20);
    return;
  }

  handleSerialCommands();

  if (presenter.update(sim.state(now))) ++framesPresented;

  if (now - lastStatsMs >= kStatsEveryMs) {
    lastStatsMs = now;
    printStats(now);
  }
  delay(10);
}
