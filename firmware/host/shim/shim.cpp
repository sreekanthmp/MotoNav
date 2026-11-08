// Behaviour of the simulated board. HOST ONLY.
#include <map>

#include "Arduino.h"
#include "Wire.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

namespace shim {
World world;
}

SerialShim Serial;
EspShim ESP;
TwoWireShim Wire;

namespace {
std::map<int, int> pinModes;
}

uint32_t millis() { return shim::world.nowMs; }
void delay(uint32_t ms) { shim::world.nowMs += ms; }
void delayMicroseconds(uint32_t) {}
void pinMode(int pin, int mode) { pinModes[pin] = mode; }

int digitalRead(int pin) {
  const shim::World& w = shim::world;
  // Which physical line is this GPIO connected to?
  const bool isSda = pin == w.wireSdaGpio;
  const bool isScl = pin == w.wireSclGpio;
  if ((isSda && w.sdaShortedToGnd) || (isScl && w.sclShortedToGnd)) return 0;
  if ((isSda && w.sdaHasPullup) || (isScl && w.sclHasPullup)) return 1;  // external pull-up wins
  const int mode = pinModes.count(pin) ? pinModes[pin] : INPUT;
  return mode == INPUT_PULLUP ? 1 : 0;  // otherwise only the internal resistors act
}

bool TwoWireShim::begin(int sda, int scl) {
  if (shim::world.wireBeginFails) return false;
  started_ = true;
  sda_ = sda;
  scl_ = scl;
  return true;
}

uint8_t TwoWireShim::endTransmission() {
  const shim::World& w = shim::world;
  if (!started_) return 4;
  // Nothing can answer if the wires are not where the firmware thinks.
  // (Swapped wires are modelled by a scenario that swaps wireSdaGpio and wireSclGpio.)
  const bool wiredAsConfigured = sda_ == w.wireSdaGpio && scl_ == w.wireSclGpio;
  bool present = false;
  for (int d : w.devices) present = present || d == address_;
  if (!present || !wiredAsConfigured || clock_ > w.deviceMaxHz) {
    return static_cast<uint8_t>(w.absentCode);
  }
  ++count_;
  if (w.flakyEvery > 0 && count_ % static_cast<unsigned long>(w.flakyEvery) == 0) return 5;  // timeout
  return 0;
}

void esp_chip_info(esp_chip_info_t* info) {
  info->model = static_cast<esp_chip_model_t>(shim::world.model);
  info->features = shim::world.features;
  info->cores = static_cast<uint8_t>(shim::world.cores);
  info->revision = static_cast<uint8_t>(shim::world.revision);
}

esp_err_t esp_flash_read_id(esp_flash_t*, uint32_t* out_id) {
  if (!shim::world.jedecReadable) return ESP_FAIL;
  *out_id = shim::world.jedecId;
  return ESP_OK;
}
