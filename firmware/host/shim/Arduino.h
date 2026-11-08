// Minimal Arduino-ESP32 stand-in so firmware/src/tools/hw_check.cpp and
// oled_check.cpp can run unmodified on a normal computer. HOST ONLY.
#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "world.h"

constexpr int INPUT = 1;
constexpr int INPUT_PULLUP = 2;
constexpr int INPUT_PULLDOWN = 3;

class SerialShim {
 public:
  void begin(unsigned long) {}
  void print(const char* s) { out += s; }
  void println(const char* s = "") {
    out += s;
    out += '\n';
  }
  void printf(const char* format, ...) __attribute__((format(printf, 2, 3))) {
    char buffer[400];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    out += buffer;
  }
  int available() const { return pos < input.size() ? 1 : 0; }
  int read() { return pos < input.size() ? static_cast<unsigned char>(input[pos++]) : -1; }

  std::string out;    // everything the firmware printed
  std::string input;  // characters the test "types" into the serial monitor
  size_t pos = 0;
};
extern SerialShim Serial;

class EspShim {
 public:
  const char* getChipModel() const { return shim::world.modelName; }
  unsigned getChipRevision() const { return shim::world.revision; }
  unsigned getChipCores() const { return static_cast<unsigned>(shim::world.cores); }
  uint32_t getCpuFreqMHz() const { return shim::world.cpuMhz; }
  uint32_t getFlashChipSize() const { return shim::world.sdkFlashBytes; }
  uint64_t getEfuseMac() const { return shim::world.mac; }
};
extern EspShim ESP;

uint32_t millis();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void pinMode(int pin, int mode);
int digitalRead(int pin);

// Provided by the firmware under test.
void setup();
void loop();
